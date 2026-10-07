#!/usr/bin/env python3
"""bom_web.py - BOM Tracker web UI + sync companion (works alongside the C++ app)
Version: 1.2.0
Changelog:
  1.2.0 (2026-10-07) Shop notes and blueprints. Per-project notes (title, type, body) with file
        attachments (photos, PDFs, drawings; up to 15 MB each) that sync like parts. New Notes tab
        on the phone page with upload from the camera or file picker. The /api/version token and
        'sync --watch' now cover notes and attachments; watch migrates the database up front.
        Attachments are served inline only for images and PDFs; everything else downloads.
  1.1.0 (2026-10-06) Live updates. New /api/version change token; the phone page polls it
        and refreshes itself; 'sync --watch' keeps a machine in step within seconds
        (checks every 3 s, pushes/pulls only when something changed, full sync every 60 s).
  1.0.0 (2026-10-06) Initial release. Idempotent schema migration (uuid, updated_at,
        tombstones + triggers, so the unmodified C++ app is tracked), mobile web UI,
        JSON API, and a 'sync' client mode (last-write-wins per row, by uuid).
Usage:
  bom_web.py sync  --watch [--server URL] [--db PATH]   # long-running, replaces timer-based sync
  bom_web.py serve [--host 100.64.0.2] [--port 8787] [--db PATH]
  bom_web.py sync  [--server http://100.64.0.2:8787] [--db PATH]
  bom_web.py migrate [--db PATH]
"""
import argparse, base64, json, mimetypes, os, sqlite3, sys, time, urllib.parse, urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

VERSION = "1.2.0"
DEF_DB = os.path.expanduser("~/.local/share/bom-tracker/bom.db")
NOW = "CAST((julianday('now')-2440587.5)*86400000 AS INTEGER)"
ms = lambda: int(time.time() * 1000)

MAX_FILE = 15 * 1024 * 1024          # per attachment
KINDS = ["Shop Note", "Blueprint", "Wiring", "Assembly", "Safety", "Reference"]
INLINE = {"image/png", "image/jpeg", "image/gif", "image/webp", "application/pdf"}  # safe to show in-browser
# table, tombstone kind, legacy uuid prefix (None = random uuid for rows that predate the migration)
SYNCED = (("projects", "project", "legacy-p-"), ("parts", "part", "legacy-r-"),
          ("notes", "note", None), ("attachments", "attachment", None))
TABLE_OF = {k: t for t, k, _ in SYNCED}


def connect(db):
    c = sqlite3.connect(db, timeout=10)
    c.row_factory = sqlite3.Row
    c.execute("pragma foreign_keys=on")
    return c


def migrate(c, db):
    have = {r[0] for r in c.execute("select name from sqlite_master where type='table'")}
    if "uuid" not in {r[1] for r in c.execute("pragma table_info(parts)")} or "notes" not in have:
        d = os.path.join(os.path.dirname(db), "backups")
        os.makedirs(d, exist_ok=True)
        b = sqlite3.connect(os.path.join(d, time.strftime("bom-%Y%m%d-%H%M%S-pre-sync.db")))
        c.backup(b); b.close()
    c.execute("create table if not exists tombstones(uuid text primary key, kind text not null, deleted_at integer not null)")
    c.execute("""create table if not exists notes(
        id integer primary key autoincrement,
        project_id integer not null references projects(id) on delete cascade,
        title text not null default '', kind text not null default 'Shop Note', body text not null default '')""")
    c.execute("""create table if not exists attachments(
        id integer primary key autoincrement,
        note_id integer not null references notes(id) on delete cascade,
        filename text not null default '', mime text not null default 'application/octet-stream',
        size integer not null default 0, data blob not null)""")
    for t, kind, legacy in SYNCED:
        if "uuid" not in {r[1] for r in c.execute(f"pragma table_info({t})")}:
            c.execute(f"alter table {t} add column uuid text")
            c.execute(f"alter table {t} add column updated_at integer not null default 0")
        if legacy:
            c.execute(f"update {t} set uuid='{legacy}'||id where uuid is null")
        else:  # no machine has legacy notes/attachments, so a shared deterministic id would collide
            c.execute(f"update {t} set uuid=lower(hex(randomblob(16))), updated_at={NOW} where uuid is null")
        c.execute(f"create unique index if not exists {t}_uuid on {t}(uuid)")
        c.executescript(f"""
        create trigger if not exists {t}_ai after insert on {t} when new.uuid is null begin
          update {t} set uuid=lower(hex(randomblob(16))), updated_at={NOW} where id=new.id; end;
        create trigger if not exists {t}_au after update on {t} when new.updated_at=old.updated_at begin
          update {t} set updated_at={NOW} where id=new.id; end;
        create trigger if not exists {t}_ad after delete on {t} when old.uuid is not null begin
          insert or replace into tombstones values(old.uuid,'{kind}',{NOW}); end;""")
    c.commit()


def token(c):
    """Cheap fingerprint of the synced data; changes whenever any row is added, edited or deleted."""
    f = [c.execute(f"select coalesce(max(updated_at),0)||'.'||count(*) from {t}").fetchone()[0]
         for t, _, _ in SYNCED]
    f.append(c.execute("select coalesce(max(deleted_at),0)||'.'||count(*) from tombstones").fetchone()[0])
    return "|".join(f)


def dump(c, since=0):
    q = lambda s, *a: [dict(r) for r in c.execute(s, a)]
    files = []
    for r in c.execute("select a.uuid as uuid,n.uuid as note_uuid,a.filename,a.mime,a.size,a.data,a.updated_at "
                       "from attachments a join notes n on n.id=a.note_id where a.updated_at>?", (since,)):
        x = dict(r); x["data"] = base64.b64encode(r["data"]).decode(); files.append(x)
    return {"now": ms(),
            "projects": q("select uuid,name,description,updated_at from projects where updated_at>?", since),
            "parts": q("select p.uuid as project_uuid,r.uuid as uuid,r.name,r.url,r.part_number,r.vendor,r.notes,"
                       "r.quantity,r.unit_price,r.status,r.section,r.updated_at from parts r "
                       "join projects p on p.id=r.project_id where r.updated_at>?", since),
            "notes": q("select p.uuid as project_uuid,n.uuid as uuid,n.title,n.kind,n.body,n.updated_at from notes n "
                       "join projects p on p.id=n.project_id where n.updated_at>?", since),
            "attachments": files,
            "tombstones": q("select uuid,kind,deleted_at from tombstones where deleted_at>?", since)}


def dead(c, u, ts):
    r = c.execute("select deleted_at from tombstones where uuid=?", (u,)).fetchone()
    return r and r[0] >= ts


def apply(c, d):
    for p in d.get("projects", []):
        if dead(c, p["uuid"], p["updated_at"]): continue
        r = c.execute("select updated_at from projects where uuid=?", (p["uuid"],)).fetchone()
        if r is None:
            c.execute("insert into projects(name,description,uuid,updated_at) values(?,?,?,?)",
                      (p["name"], p["description"], p["uuid"], p["updated_at"]))
        elif p["updated_at"] > r[0]:
            c.execute("update projects set name=?,description=?,updated_at=? where uuid=?",
                      (p["name"], p["description"], p["updated_at"], p["uuid"]))
        else: continue
        c.execute("delete from tombstones where uuid=?", (p["uuid"],))
    cols = ("name", "url", "part_number", "vendor", "notes", "quantity", "unit_price", "status", "section")
    for x in d.get("parts", []):
        if dead(c, x["uuid"], x["updated_at"]): continue
        pj = c.execute("select id from projects where uuid=?", (x["project_uuid"],)).fetchone()
        if pj is None: continue
        r = c.execute("select updated_at from parts where uuid=?", (x["uuid"],)).fetchone()
        v = tuple(x[k] for k in cols)
        if r is None:
            c.execute("insert into parts(%s,project_id,uuid,updated_at) values(%s)" % (",".join(cols), ",".join("?" * 12)),
                      v + (pj[0], x["uuid"], x["updated_at"]))
        elif x["updated_at"] > r[0]:
            c.execute("update parts set %s,project_id=?,updated_at=? where uuid=?" % ",".join(k + "=?" for k in cols),
                      v + (pj[0], x["updated_at"], x["uuid"]))
        else: continue
        c.execute("delete from tombstones where uuid=?", (x["uuid"],))
    for x in d.get("notes", []):
        if dead(c, x["uuid"], x["updated_at"]): continue
        pj = c.execute("select id from projects where uuid=?", (x["project_uuid"],)).fetchone()
        if pj is None: continue
        r = c.execute("select updated_at from notes where uuid=?", (x["uuid"],)).fetchone()
        v = (x["title"], x["kind"], x["body"])
        if r is None:
            c.execute("insert into notes(title,kind,body,project_id,uuid,updated_at) values(?,?,?,?,?,?)",
                      v + (pj[0], x["uuid"], x["updated_at"]))
        elif x["updated_at"] > r[0]:
            c.execute("update notes set title=?,kind=?,body=?,project_id=?,updated_at=? where uuid=?",
                      v + (pj[0], x["updated_at"], x["uuid"]))
        else: continue
        c.execute("delete from tombstones where uuid=?", (x["uuid"],))
    for x in d.get("attachments", []):
        if dead(c, x["uuid"], x["updated_at"]): continue
        n = c.execute("select id from notes where uuid=?", (x["note_uuid"],)).fetchone()
        if n is None: continue
        r = c.execute("select updated_at from attachments where uuid=?", (x["uuid"],)).fetchone()
        blob = base64.b64decode(x["data"])
        if r is None:
            c.execute("insert into attachments(filename,mime,size,data,note_id,uuid,updated_at) values(?,?,?,?,?,?,?)",
                      (x["filename"], x["mime"], len(blob), blob, n[0], x["uuid"], x["updated_at"]))
        elif x["updated_at"] > r[0]:
            c.execute("update attachments set filename=?,mime=?,size=?,data=?,note_id=?,updated_at=? where uuid=?",
                      (x["filename"], x["mime"], len(blob), blob, n[0], x["updated_at"], x["uuid"]))
        else: continue
        c.execute("delete from tombstones where uuid=?", (x["uuid"],))
    for t in d.get("tombstones", []):
        tbl = TABLE_OF.get(t["kind"])
        if tbl is None: continue          # a kind from a newer version: ignore rather than guess
        r = c.execute(f"select updated_at from {tbl} where uuid=?", (t["uuid"],)).fetchone()
        if r and r[0] <= t["deleted_at"]:
            c.execute(f"delete from {tbl} where uuid=?", (t["uuid"],))
        if r is None or r[0] <= t["deleted_at"]:
            c.execute("insert into tombstones values(?,?,?) on conflict(uuid) do update set deleted_at=max(deleted_at,excluded.deleted_at)",
                      (t["uuid"], t["kind"], t["deleted_at"]))


def save_part(c, b):
    s = lambda k: str(b.get(k) or "").strip()
    if not s("name"): raise ValueError("Name is required")
    v = (s("name"), s("url"), s("part_number"), s("vendor"), s("notes"), max(1, int(b.get("quantity") or 1)),
         float(b.get("unit_price") or 0), min(3, max(0, int(b.get("status") or 0))), s("section"))
    with c:
        if b.get("id"):
            c.execute("update parts set name=?,url=?,part_number=?,vendor=?,notes=?,quantity=?,unit_price=?,status=?,section=? where id=?", v + (int(b["id"]),))
        else:
            c.execute("insert into parts(name,url,part_number,vendor,notes,quantity,unit_price,status,section,project_id) values(?,?,?,?,?,?,?,?,?,?)", v + (int(b["project_id"]),))
    return {"ok": 1}


def save_note(c, b):
    title = str(b.get("title") or "").strip()
    if not title: raise ValueError("Title is required")
    kind = str(b.get("kind") or "").strip() or KINDS[0]
    body = str(b.get("body") or "").replace("\r\n", "\n").rstrip()
    with c:
        if b.get("id"):
            nid = int(b["id"])
            if c.execute("update notes set title=?,kind=?,body=? where id=?", (title, kind, body, nid)).rowcount == 0:
                raise ValueError("Note no longer exists")
        else:
            nid = c.execute("insert into notes(project_id,title,kind,body) values(?,?,?,?)",
                            (int(b["project_id"]), title, kind, body)).lastrowid
    return {"id": nid}


def save_attachment(c, note_id, name, mime, data):
    if not data: raise ValueError("File is empty")
    if len(data) > MAX_FILE: raise ValueError("File is larger than %d MB" % (MAX_FILE >> 20))
    name = os.path.basename(str(name or "").replace("\\", "/")).strip() or "file"
    mime = (mime or mimetypes.guess_type(name)[0] or "application/octet-stream").split(";")[0].strip().lower()
    if mime == "application/octet-stream":
        mime = mimetypes.guess_type(name)[0] or mime
    with c:
        cur = c.execute("insert into attachments(note_id,filename,mime,size,data) values(?,?,?,?,?)",
                        (note_id, name, mime, len(data), data))
    return {"id": cur.lastrowid}


class H(BaseHTTPRequestHandler):
    db = DEF_DB

    def send(self, obj, code=200, ctype="application/json", extra=(), cache="no-store"):
        b = obj if isinstance(obj, bytes) else json.dumps(obj).encode()
        self.send_response(code)
        for k, v in (("Content-Type", ctype), ("Content-Length", str(len(b))), ("Cache-Control", cache)) + tuple(extra):
            self.send_header(k, v)
        self.end_headers(); self.wfile.write(b)

    def do_GET(self):
        u = urllib.parse.urlparse(self.path)
        qs = urllib.parse.parse_qs(u.query)
        try:
            if u.path == "/": return self.send(PAGE.replace("__KINDS__", json.dumps(KINDS)).replace("__MAXF__", str(MAX_FILE)).encode(),
                                               ctype="text/html; charset=utf-8")
            if u.path == "/bom_web.py": return self.send(open(__file__, "rb").read(), ctype="text/plain; charset=utf-8")
            c = connect(self.db)
            if u.path == "/api/data":
                return self.send({"projects": [dict(r) for r in c.execute("select id,name,description from projects order by name collate nocase")],
                                  "parts": [dict(r) for r in c.execute("select id,project_id,name,url,part_number,vendor,notes,quantity,unit_price,status,section from parts order by section collate nocase,name collate nocase")],
                                  "notes": [dict(r) for r in c.execute("select id,project_id,title,kind,body from notes order by kind collate nocase,title collate nocase,id")],
                                  "files": [dict(r) for r in c.execute("select id,note_id,filename,mime,size from attachments order by filename collate nocase,id")]})
            if u.path == "/api/attachment":
                r = c.execute("select filename,mime,data from attachments where id=?", (int(qs["id"][0]),)).fetchone()
                if r is None: return self.send({"error": "not found"}, 404)
                inline = r["mime"] in INLINE
                disp = ("inline" if inline else "attachment") + "; filename*=UTF-8''" + urllib.parse.quote(r["filename"])
                return self.send(bytes(r["data"]), ctype=r["mime"] if inline else "application/octet-stream",
                                 extra=(("Content-Disposition", disp), ("X-Content-Type-Options", "nosniff")),
                                 cache="private, max-age=3600")
            if u.path == "/api/version": return self.send({"token": token(c), "now": ms()})
            if u.path == "/api/sync":
                return self.send(dump(c, int(qs.get("since", ["0"])[0])))
            self.send({"error": "not found"}, 404)
        except Exception as e: self.send({"error": str(e)}, 500)

    def do_POST(self):
        u = urllib.parse.urlparse(self.path)
        try:
            c = connect(self.db)
            n = int(self.headers.get("Content-Length", 0))
            if u.path == "/api/attachment":          # raw file body; note_id and name in the query string
                if n > MAX_FILE:
                    self.close_connection = True
                    return self.send({"error": "File is larger than %d MB" % (MAX_FILE >> 20)}, 413)
                qs = urllib.parse.parse_qs(u.query)
                return self.send(save_attachment(c, int(qs["note_id"][0]), qs.get("name", ["file"])[0],
                                                 self.headers.get("Content-Type"), self.rfile.read(n)))
            b = json.loads(self.rfile.read(n) or b"{}")
            if u.path == "/api/part": return self.send(save_part(c, b))
            if u.path == "/api/part/delete":
                with c: c.execute("delete from parts where id=?", (int(b["id"]),))
                return self.send({"ok": 1})
            if u.path == "/api/note": return self.send(save_note(c, b))
            if u.path == "/api/note/delete":
                with c: c.execute("delete from notes where id=?", (int(b["id"]),))
                return self.send({"ok": 1})
            if u.path == "/api/attachment/delete":
                with c: c.execute("delete from attachments where id=?", (int(b["id"]),))
                return self.send({"ok": 1})
            if u.path == "/api/project":
                if not str(b.get("name", "")).strip(): raise ValueError("Name is required")
                with c: cur = c.execute("insert into projects(name,description) values(?,?)", (b["name"].strip(), str(b.get("description", ""))))
                return self.send({"id": cur.lastrowid})
            if u.path == "/api/sync":
                with c: apply(c, b)
                return self.send(dump(c, int(b.get("since", 0))))
            self.send({"error": "not found"}, 404)
        except Exception as e: self.send({"error": str(e)}, 400)

    def log_message(self, *a): pass


def post(url, obj):
    r = urllib.request.Request(url, json.dumps(obj).encode(), {"Content-Type": "application/json"})
    return json.load(urllib.request.urlopen(r, timeout=120))


def sync(db, server, quiet=False):
    c = connect(db); migrate(c, db)
    sp = os.path.join(os.path.dirname(db), "bom-sync-state.json")
    st = json.load(open(sp)) if os.path.exists(sp) else {"local": 0, "server": 0}
    t0 = ms(); out = dump(c, st["local"]); out["since"] = st["server"]
    back = post(server.rstrip("/") + "/api/sync", out)
    if "error" in back: sys.exit("server error: " + back["error"])
    with c: apply(c, back)
    json.dump({"local": t0 - 2000, "server": back["now"] - 2000}, open(sp, "w"))
    K = ("projects", "parts", "notes", "attachments", "tombstones")
    n = tuple(len(out[k]) for k in K) + tuple(len(back.get(k, [])) for k in K)
    if not (quiet and not any(n)):
        print("%s sync ok: sent %d/%d/%d/%d/%d, received %d/%d/%d/%d/%d (projects/parts/notes/files/deletes)" % ((time.strftime("%H:%M:%S"),) + n), flush=True)


def get(url):
    return json.load(urllib.request.urlopen(url, timeout=10))


def watch(db, server, every=3, full=60):
    """Stay running: sync when either side's data changes (checked every `every` s), plus a full sync every `full` s."""
    server = server.rstrip("/")
    last_l = last_r = None; last_full = 0.0; last_err = None; migrated = False
    print("watching %s (db %s), checking every %ds" % (server, db, every), flush=True)
    while True:
        try:
            if not migrated: c = connect(db); migrate(c, db); c.close(); migrated = True
            c = connect(db); l = token(c); c.close()
            r = get(server + "/api/version")["token"]
            if l != last_l or r != last_r or time.time() - last_full >= full:
                sync(db, server, quiet=True)
                c = connect(db); last_l = token(c); c.close()
                last_r = get(server + "/api/version")["token"]
                last_full = time.time()
            if last_err: print(time.strftime("%H:%M:%S"), "recovered", flush=True)
            last_err = None
        except (Exception, SystemExit) as e:
            if str(e) != last_err:
                print(time.strftime("%H:%M:%S"), "watch error:", e, flush=True)
            last_err = str(e)
            last_l = last_r = None
        time.sleep(every)


PAGE = r"""<!doctype html><html lang=en><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1,viewport-fit=cover"><meta name=theme-color content="#1a1c21">
<meta name=apple-mobile-web-app-capable content=yes><title>BOM Tracker</title><style>
:root{--bg:#1a1c21;--pn:#212329;--bd:#383d47;--tx:#e0e0d9;--dim:#8c9199;--ac:#f2a626}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--tx);font:16px system-ui,sans-serif;padding-bottom:5rem}
header{position:sticky;top:0;background:var(--pn);border-bottom:1px solid var(--bd);padding:.6rem .8rem;z-index:2}
header div{display:flex;gap:.5rem}select,input,textarea,button{font:inherit;color:var(--tx);background:var(--bg);border:1px solid var(--bd);border-radius:8px;padding:.5rem}
select{flex:1;min-width:0}#q{width:100%;margin-top:.5rem}#stats{color:var(--dim);font-size:.85rem;margin-top:.4rem}
.tabs{margin-top:.5rem}.tabs button{flex:1}.tabs .on{background:var(--ac);color:#111;border-color:var(--ac);font-weight:600}
h3{margin:1rem .8rem .3rem;color:var(--ac);font-size:.8rem;text-transform:uppercase;letter-spacing:.05em}
.row{display:flex;align-items:center;gap:.6rem;padding:.6rem .8rem;border-bottom:1px solid var(--bd)}
.m{flex:1;min-width:0}.m b{display:block}.m small{display:block;color:var(--dim)}.n{white-space:pre-wrap}
.cl{display:-webkit-box;-webkit-line-clamp:2;-webkit-box-orient:vertical;overflow:hidden}
.pill{border:0;color:#111;font-weight:600;font-size:.75rem;padding:.4rem .5rem;min-width:4.6rem;border-radius:99px}
span.pill{display:inline-block;text-align:center}
a{color:var(--ac);font-size:1.4rem;text-decoration:none;padding:.3rem}.empty{text-align:center;color:var(--dim)}
#add{position:fixed;right:1rem;bottom:calc(1rem + env(safe-area-inset-bottom));background:var(--ac);color:#111;border:0;font-weight:700;padding:.8rem 1.2rem;border-radius:99px}
dialog{background:var(--pn);color:var(--tx);border:1px solid var(--bd);border-radius:12px;width:min(94vw,30rem);padding:1rem}
dialog::backdrop{background:#000a}dialog label{display:block;font-size:.8rem;color:var(--dim);margin-top:.5rem}
dialog input,dialog select,dialog textarea{width:100%}.g{display:flex;gap:.5rem}.g>*{flex:1}.bt{display:flex;gap:.5rem;margin-top:1rem}.bt button{flex:1}
#n_body{font:.85rem ui-monospace,Menlo,Consolas,monospace;white-space:pre;overflow:auto}
.att{display:flex;align-items:center;gap:.5rem;margin-top:.5rem;flex-wrap:wrap}.att img{max-width:100%;max-height:14rem;border-radius:8px;border:1px solid var(--bd);display:block}
.att a{font-size:1rem;padding:0;word-break:break-all}.att small{color:var(--dim)}.att button{padding:.2rem .5rem;color:#e05555}
.att .th{flex-basis:100%}#n_pend{color:var(--dim);font-size:.85rem;margin-top:.3rem;white-space:pre-wrap}
</style></head><body><header><div><select id=proj onchange="cur=+this.value;localStorage.p=cur;render()"></select>
<button onclick=newproj()>+</button></div>
<div class=tabs><button id=t_parts onclick="tab('parts')">Parts</button><button id=t_notes onclick="tab('notes')">Notes</button></div>
<input id=q type=search placeholder=Search oninput=render()><div id=stats></div></header>
<main id=list></main><button id=add onclick=addnew()>+ Part</button>
<dialog id=dlg><label>Name<input id=f_name></label><div class=g><label>Part #<input id=f_part_number></label><label>Vendor<input id=f_vendor></label></div>
<label>URL<input id=f_url type=url></label><div class=g><label>Qty<input id=f_quantity type=number min=1></label><label>Unit price<input id=f_unit_price type=number step=.01 min=0></label></div>
<div class=g><label>Section<input id=f_section list=secs></label><label>Status<select id=f_status><option value=0>Needed<option value=1>Ordered<option value=2>In Stock<option value=3>Installed</select></label></div>
<datalist id=secs></datalist><label>Notes<textarea id=f_notes rows=3></textarea></label>
<div class=bt><button onclick=save() style="background:var(--ac);color:#111;border:0">Save</button><button id=del onclick=del() style="color:#e05555">Delete</button><button onclick=dlg.close()>Cancel</button></div></dialog>
<dialog id=ndlg><div class=g><label>Title<input id=n_title></label><label>Type<select id=n_kind></select></label></div>
<label>Note<textarea id=n_body rows=12 wrap=off></textarea></label>
<label>Files (photos, PDFs, drawings)</label><div id=n_files></div>
<input id=n_new type=file multiple onchange=pickf(this) style="margin-top:.5rem"><div id=n_pend></div>
<div class=bt><button onclick=savenote() style="background:var(--ac);color:#111;border:0">Save</button><button id=ndel onclick=delnote() style="color:#e05555">Delete</button><button onclick=ndlg.close()>Cancel</button></div></dialog>
<script>
const S=["Needed","Ordered","In Stock","Installed"],C=["#d94747","#e6cc33","#599be6","#59c76b"],F=["name","part_number","vendor","url","quantity","unit_price","section","status","notes"];
const KINDS=__KINDS__,MAXF=__MAXF__,KC={"Shop Note":"#f2a626",Blueprint:"#599be6",Wiring:"#e6cc33",Assembly:"#59c76b",Safety:"#d94747",Reference:"#8c9199"};
const INL=["image/png","image/jpeg","image/gif","image/webp"];
const $=id=>document.getElementById(id),e=s=>String(s).replace(/[&<>"]/g,c=>({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;"}[c]));
const fsz=n=>n<1024?n+" B":n<1048576?Math.round(n/1024)+" KB":(n/1048576).toFixed(1)+" MB";
let D={projects:[],parts:[],notes:[],files:[]},cur=+localStorage.p||0,eid=0,nid=0,pend=[],view=localStorage.v==="notes"?"notes":"parts";
async function api(p,b){const r=await fetch(p,b&&{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(b)});return r.json()}
async function load(){D=await api("/api/data");if(!D.projects.some(p=>p.id==cur))cur=D.projects[0]?.id||0;render()}
function tab(v){view=v;localStorage.v=v;render()}
function render(){
 $("proj").innerHTML=D.projects.map(p=>`<option value=${p.id} ${p.id==cur?"selected":""}>${e(p.name)}</option>`).join("");
 $("t_parts").className=view=="parts"?"on":"";$("t_notes").className=view=="notes"?"on":"";
 $("add").textContent=view=="parts"?"+ Part":"+ Note";
 view=="parts"?renderParts():renderNotes()}
function renderParts(){
 const q=$("q").value.toLowerCase(),all=D.parts.filter(x=>x.project_id==cur),L=all.filter(x=>!q||[x.name,x.part_number,x.vendor,x.notes,x.section].join(" ").toLowerCase().includes(q));
 $("stats").textContent=`${all.filter(x=>x.status==3).length}/${all.length} installed · $${all.reduce((s,x)=>s+x.quantity*x.unit_price,0).toFixed(2)}`;
 let h="",sec=null;
 for(const x of L){if(x.section!==sec){sec=x.section;h+=`<h3>${e(sec||"Parts")}</h3>`}
  h+=`<div class=row><button class=pill style="background:${C[x.status]}" onclick=cyc(${x.id})>${S[x.status]}</button><div class=m onclick=edit(${x.id})><b>${e(x.name)}</b><small>${e([x.part_number,x.vendor].filter(Boolean).join(" · "))}${x.quantity>1||x.unit_price?` ×${x.quantity} @ $${x.unit_price.toFixed(2)}`:""}</small>${x.notes?`<small class=n>${e(x.notes)}</small>`:""}</div>${/^https?:/i.test(x.url)?`<a href="${e(x.url)}" target=_blank rel=noopener>↗</a>`:""}</div>`}
 $("list").innerHTML=h||"<p class=empty>No parts</p>"}
function renderNotes(){
 const q=$("q").value.toLowerCase(),all=D.notes.filter(n=>n.project_id==cur),L=all.filter(n=>!q||[n.title,n.kind,n.body].join(" ").toLowerCase().includes(q)),nf=n=>D.files.filter(f=>f.note_id==n);
 $("stats").textContent=`${all.length} note${all.length==1?"":"s"} · ${all.reduce((s,n)=>s+nf(n.id).length,0)} file(s)`;
 $("list").innerHTML=L.map(n=>{const k=nf(n.id).length;return `<div class=row onclick=editn(${n.id})><span class=pill style="background:${KC[n.kind]||"#8c9199"}">${e(n.kind)}</span><div class=m><b>${e(n.title)}</b>${n.body?`<small class="n cl">${e(n.body)}</small>`:""}${k?`<small>📎 ${k} file${k>1?"s":""}</small>`:""}</div></div>`}).join("")||"<p class=empty>No notes</p>"}
function addnew(){view=="parts"?edit(0):editn(0)}
function edit(id){const x=id?D.parts.find(p=>p.id==id):{name:"",part_number:"",vendor:"",url:"",quantity:1,unit_price:0,section:"",status:0,notes:""};eid=id;F.forEach(k=>$("f_"+k).value=x[k]);$("del").style.display=id?"":"none";
 $("secs").innerHTML=[...new Set(D.parts.filter(p=>p.project_id==cur).map(p=>p.section).filter(Boolean))].map(s=>`<option value="${e(s)}">`).join("");$("dlg").showModal()}
async function save(){const b={id:eid,project_id:cur};F.forEach(k=>b[k]=$("f_"+k).value);const r=await api("/api/part",b);if(r.error)return alert(r.error);$("dlg").close();load()}
async function del(){if(confirm("Delete this part?")){await api("/api/part/delete",{id:eid});$("dlg").close();load()}}
async function cyc(id){const x=D.parts.find(p=>p.id==id);x.status=(x.status+1)%4;await api("/api/part",x);load()}
async function newproj(){const n=prompt("New project name");if(n){const r=await api("/api/project",{name:n});if(r.id)cur=r.id;load()}}
function filesHtml(id){return D.files.filter(f=>f.note_id==id).map(f=>{const u=`/api/attachment?id=${f.id}`;
 return `<div class=att>${INL.includes(f.mime)?`<a class=th href="${u}" target=_blank><img src="${u}" loading=lazy alt=""></a>`:""}<a href="${u}" target=_blank>${e(f.filename)}</a><small>${fsz(f.size)}</small><button onclick=delfile(${f.id})>✕</button></div>`}).join("")}
function pendTxt(){$("n_pend").textContent=pend.length?"To upload on Save:\n"+pend.map(f=>f.name+" ("+fsz(f.size)+(f.size>MAXF?" - too large, will be skipped":"")+")").join("\n"):""}
function pickf(i){pend=[...pend,...i.files];i.value="";pendTxt()}
function editn(id){const n=id?D.notes.find(x=>x.id==id):{title:"",kind:KINDS[0],body:""};nid=id;pend=[];
 const ks=KINDS.includes(n.kind)?KINDS:[...KINDS,n.kind];$("n_kind").innerHTML=ks.map(k=>`<option ${k==n.kind?"selected":""}>${e(k)}</option>`).join("");
 $("n_title").value=n.title;$("n_body").value=n.body;$("n_files").innerHTML=filesHtml(id);pendTxt();$("ndel").style.display=id?"":"none";$("ndlg").showModal()}
async function savenote(){const r=await api("/api/note",{id:nid,project_id:cur,title:$("n_title").value,kind:$("n_kind").value,body:$("n_body").value});if(r.error)return alert(r.error);
 for(const f of pend){if(f.size>MAXF){alert(f.name+" is larger than "+fsz(MAXF)+" and was skipped");continue}
  try{const u=await fetch(`/api/attachment?note_id=${r.id}&name=${encodeURIComponent(f.name)}`,{method:"POST",headers:{"Content-Type":f.type||"application/octet-stream"},body:f}),j=await u.json();if(j.error)alert(f.name+": "+j.error)}catch(x){alert(f.name+": upload failed")}}
 $("ndlg").close();load()}
async function delnote(){if(confirm("Delete this note and its files?")){await api("/api/note/delete",{id:nid});$("ndlg").close();load()}}
async function delfile(id){if(confirm("Remove this file?")){await api("/api/attachment/delete",{id});await load();$("n_files").innerHTML=filesHtml(nid)}}
let tok=null;const busy=()=>$("dlg").open||$("ndlg").open;
async function poll(){if(document.hidden)return;try{const v=await api("/api/version");if(tok===null)tok=v.token;else if(v.token!==tok&&!busy()){await load();tok=v.token}}catch(_){}}
load();poll();setInterval(poll,3000);
document.addEventListener("visibilitychange",()=>{if(!document.hidden&&!busy())load().then(poll)});
</script></body></html>"""


def main():
    ap = argparse.ArgumentParser(description="BOM Tracker web + sync v" + VERSION)
    ap.add_argument("cmd", choices=["serve", "sync", "migrate"])
    ap.add_argument("--db", default=DEF_DB)
    ap.add_argument("--watch", action="store_true", help="with 'sync': keep running and sync on change")
    ap.add_argument("--host", default="100.64.0.2")
    ap.add_argument("--port", type=int, default=8787)
    ap.add_argument("--server", default="http://100.64.0.2:8787")
    a = ap.parse_args()
    if a.cmd == "sync": return watch(a.db, a.server) if a.watch else sync(a.db, a.server)
    c = connect(a.db); migrate(c, a.db); c.close()
    if a.cmd == "migrate": return print("migrated", a.db)
    H.db = a.db
    print("BOM web v%s on http://%s:%d (db %s)" % (VERSION, a.host, a.port, a.db), flush=True)
    ThreadingHTTPServer((a.host, a.port), H).serve_forever()


if __name__ == "__main__":
    main()
