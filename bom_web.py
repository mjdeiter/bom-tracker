#!/usr/bin/env python3
"""bom_web.py - BOM Tracker web UI + sync companion (works alongside the C++ app)
Version: 1.0.0
Changelog:
  1.0.0 (2026-10-06) Initial release. Idempotent schema migration (uuid, updated_at,
        tombstones + triggers, so the unmodified C++ app is tracked), mobile web UI,
        JSON API, and a 'sync' client mode (last-write-wins per row, by uuid).
Usage:
  bom_web.py serve [--host 100.64.0.2] [--port 8787] [--db PATH]
  bom_web.py sync  [--server http://100.64.0.2:8787] [--db PATH]
  bom_web.py migrate [--db PATH]
"""
import argparse, json, os, sqlite3, sys, time, urllib.parse, urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

VERSION = "1.0.0"
DEF_DB = os.path.expanduser("~/.local/share/bom-tracker/bom.db")
NOW = "CAST((julianday('now')-2440587.5)*86400000 AS INTEGER)"
ms = lambda: int(time.time() * 1000)


def connect(db):
    c = sqlite3.connect(db, timeout=10)
    c.row_factory = sqlite3.Row
    c.execute("pragma foreign_keys=on")
    return c


def migrate(c, db):
    if "uuid" not in {r[1] for r in c.execute("pragma table_info(parts)")}:
        d = os.path.join(os.path.dirname(db), "backups")
        os.makedirs(d, exist_ok=True)
        b = sqlite3.connect(os.path.join(d, time.strftime("bom-%Y%m%d-%H%M%S-pre-sync.db")))
        c.backup(b); b.close()
    c.execute("create table if not exists tombstones(uuid text primary key, kind text not null, deleted_at integer not null)")
    for t, kind, k in (("projects", "project", "p"), ("parts", "part", "r")):
        if "uuid" not in {r[1] for r in c.execute(f"pragma table_info({t})")}:
            c.execute(f"alter table {t} add column uuid text")
            c.execute(f"alter table {t} add column updated_at integer not null default 0")
        c.execute(f"update {t} set uuid='legacy-{k}-'||id where uuid is null")
        c.execute(f"create unique index if not exists {t}_uuid on {t}(uuid)")
        c.executescript(f"""
        create trigger if not exists {t}_ai after insert on {t} when new.uuid is null begin
          update {t} set uuid=lower(hex(randomblob(16))), updated_at={NOW} where id=new.id; end;
        create trigger if not exists {t}_au after update on {t} when new.updated_at=old.updated_at begin
          update {t} set updated_at={NOW} where id=new.id; end;
        create trigger if not exists {t}_ad after delete on {t} when old.uuid is not null begin
          insert or replace into tombstones values(old.uuid,'{kind}',{NOW}); end;""")
    c.commit()


def dump(c, since=0):
    q = lambda s, *a: [dict(r) for r in c.execute(s, a)]
    return {"now": ms(),
            "projects": q("select uuid,name,description,updated_at from projects where updated_at>?", since),
            "parts": q("select p.uuid as project_uuid,r.uuid as uuid,r.name,r.url,r.part_number,r.vendor,r.notes,"
                       "r.quantity,r.unit_price,r.status,r.section,r.updated_at from parts r "
                       "join projects p on p.id=r.project_id where r.updated_at>?", since),
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
    for t in d.get("tombstones", []):
        tbl = "projects" if t["kind"] == "project" else "parts"
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


class H(BaseHTTPRequestHandler):
    db = DEF_DB

    def send(self, obj, code=200, ctype="application/json"):
        b = obj if isinstance(obj, bytes) else json.dumps(obj).encode()
        self.send_response(code)
        for k, v in (("Content-Type", ctype), ("Content-Length", str(len(b))), ("Cache-Control", "no-store")):
            self.send_header(k, v)
        self.end_headers(); self.wfile.write(b)

    def do_GET(self):
        u = urllib.parse.urlparse(self.path)
        try:
            if u.path == "/": return self.send(PAGE.encode(), ctype="text/html; charset=utf-8")
            if u.path == "/bom_web.py": return self.send(open(__file__, "rb").read(), ctype="text/plain; charset=utf-8")
            c = connect(self.db)
            if u.path == "/api/data":
                return self.send({"projects": [dict(r) for r in c.execute("select id,name,description from projects order by name collate nocase")],
                                  "parts": [dict(r) for r in c.execute("select id,project_id,name,url,part_number,vendor,notes,quantity,unit_price,status,section from parts order by section collate nocase,name collate nocase")]})
            if u.path == "/api/sync":
                return self.send(dump(c, int(urllib.parse.parse_qs(u.query).get("since", ["0"])[0])))
            self.send({"error": "not found"}, 404)
        except Exception as e: self.send({"error": str(e)}, 500)

    def do_POST(self):
        try:
            b = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
            c = connect(self.db)
            if self.path == "/api/part": return self.send(save_part(c, b))
            if self.path == "/api/part/delete":
                with c: c.execute("delete from parts where id=?", (int(b["id"]),))
                return self.send({"ok": 1})
            if self.path == "/api/project":
                if not str(b.get("name", "")).strip(): raise ValueError("Name is required")
                with c: cur = c.execute("insert into projects(name,description) values(?,?)", (b["name"].strip(), str(b.get("description", ""))))
                return self.send({"id": cur.lastrowid})
            if self.path == "/api/sync":
                with c: apply(c, b)
                return self.send(dump(c, int(b.get("since", 0))))
            self.send({"error": "not found"}, 404)
        except Exception as e: self.send({"error": str(e)}, 400)

    def log_message(self, *a): pass


def post(url, obj):
    r = urllib.request.Request(url, json.dumps(obj).encode(), {"Content-Type": "application/json"})
    return json.load(urllib.request.urlopen(r, timeout=30))


def sync(db, server):
    c = connect(db); migrate(c, db)
    sp = os.path.join(os.path.dirname(db), "bom-sync-state.json")
    st = json.load(open(sp)) if os.path.exists(sp) else {"local": 0, "server": 0}
    t0 = ms(); out = dump(c, st["local"]); out["since"] = st["server"]
    back = post(server.rstrip("/") + "/api/sync", out)
    if "error" in back: sys.exit("server error: " + back["error"])
    with c: apply(c, back)
    json.dump({"local": t0 - 2000, "server": back["now"] - 2000}, open(sp, "w"))
    print("sync ok: sent %d/%d/%d, received %d/%d/%d (projects/parts/deletes)" % (
        len(out["projects"]), len(out["parts"]), len(out["tombstones"]),
        len(back["projects"]), len(back["parts"]), len(back["tombstones"])))


PAGE = r"""<!doctype html><html lang=en><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1,viewport-fit=cover"><meta name=theme-color content="#1a1c21">
<meta name=apple-mobile-web-app-capable content=yes><title>BOM Tracker</title><style>
:root{--bg:#1a1c21;--pn:#212329;--bd:#383d47;--tx:#e0e0d9;--dim:#8c9199;--ac:#f2a626}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--tx);font:16px system-ui,sans-serif;padding-bottom:5rem}
header{position:sticky;top:0;background:var(--pn);border-bottom:1px solid var(--bd);padding:.6rem .8rem;z-index:2}
header div{display:flex;gap:.5rem}select,input,textarea,button{font:inherit;color:var(--tx);background:var(--bg);border:1px solid var(--bd);border-radius:8px;padding:.5rem}
select{flex:1;min-width:0}#q{width:100%;margin-top:.5rem}#stats{color:var(--dim);font-size:.85rem;margin-top:.4rem}
h3{margin:1rem .8rem .3rem;color:var(--ac);font-size:.8rem;text-transform:uppercase;letter-spacing:.05em}
.row{display:flex;align-items:center;gap:.6rem;padding:.6rem .8rem;border-bottom:1px solid var(--bd)}
.m{flex:1;min-width:0}.m b{display:block}.m small{display:block;color:var(--dim)}.n{white-space:pre-wrap}
.pill{border:0;color:#111;font-weight:600;font-size:.75rem;padding:.4rem .5rem;min-width:4.6rem;border-radius:99px}
a{color:var(--ac);font-size:1.4rem;text-decoration:none;padding:.3rem}.empty{text-align:center;color:var(--dim)}
#add{position:fixed;right:1rem;bottom:calc(1rem + env(safe-area-inset-bottom));background:var(--ac);color:#111;border:0;font-weight:700;padding:.8rem 1.2rem;border-radius:99px}
dialog{background:var(--pn);color:var(--tx);border:1px solid var(--bd);border-radius:12px;width:min(94vw,30rem);padding:1rem}
dialog::backdrop{background:#000a}dialog label{display:block;font-size:.8rem;color:var(--dim);margin-top:.5rem}
dialog input,dialog select,dialog textarea{width:100%}.g{display:flex;gap:.5rem}.g>*{flex:1}.bt{display:flex;gap:.5rem;margin-top:1rem}.bt button{flex:1}
</style></head><body><header><div><select id=proj onchange="cur=+this.value;localStorage.p=cur;render()"></select>
<button onclick=newproj()>+</button></div><input id=q type=search placeholder=Search oninput=render()><div id=stats></div></header>
<main id=list></main><button id=add onclick=edit(0)>+ Part</button>
<dialog id=dlg><label>Name<input id=f_name></label><div class=g><label>Part #<input id=f_part_number></label><label>Vendor<input id=f_vendor></label></div>
<label>URL<input id=f_url type=url></label><div class=g><label>Qty<input id=f_quantity type=number min=1></label><label>Unit price<input id=f_unit_price type=number step=.01 min=0></label></div>
<div class=g><label>Section<input id=f_section list=secs></label><label>Status<select id=f_status><option value=0>Needed<option value=1>Ordered<option value=2>In Stock<option value=3>Installed</select></label></div>
<datalist id=secs></datalist><label>Notes<textarea id=f_notes rows=3></textarea></label>
<div class=bt><button onclick=save() style="background:var(--ac);color:#111;border:0">Save</button><button id=del onclick=del() style="color:#e05555">Delete</button><button onclick=dlg.close()>Cancel</button></div></dialog>
<script>
const S=["Needed","Ordered","In Stock","Installed"],C=["#d94747","#e6cc33","#599be6","#59c76b"],F=["name","part_number","vendor","url","quantity","unit_price","section","status","notes"];
const $=id=>document.getElementById(id),e=s=>String(s).replace(/[&<>"]/g,c=>({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;"}[c]));
let D={projects:[],parts:[]},cur=+localStorage.p||0,eid=0;
async function api(p,b){const r=await fetch(p,b&&{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(b)});return r.json()}
async function load(){D=await api("/api/data");if(!D.projects.some(p=>p.id==cur))cur=D.projects[0]?.id||0;render()}
function render(){
 $("proj").innerHTML=D.projects.map(p=>`<option value=${p.id} ${p.id==cur?"selected":""}>${e(p.name)}</option>`).join("");
 const q=$("q").value.toLowerCase(),all=D.parts.filter(x=>x.project_id==cur),L=all.filter(x=>!q||[x.name,x.part_number,x.vendor,x.notes,x.section].join(" ").toLowerCase().includes(q));
 $("stats").textContent=`${all.filter(x=>x.status==3).length}/${all.length} installed · $${all.reduce((s,x)=>s+x.quantity*x.unit_price,0).toFixed(2)}`;
 let h="",sec=null;
 for(const x of L){if(x.section!==sec){sec=x.section;h+=`<h3>${e(sec||"Parts")}</h3>`}
  h+=`<div class=row><button class=pill style="background:${C[x.status]}" onclick=cyc(${x.id})>${S[x.status]}</button><div class=m onclick=edit(${x.id})><b>${e(x.name)}</b><small>${e([x.part_number,x.vendor].filter(Boolean).join(" · "))}${x.quantity>1||x.unit_price?` ×${x.quantity} @ $${x.unit_price.toFixed(2)}`:""}</small>${x.notes?`<small class=n>${e(x.notes)}</small>`:""}</div>${/^https?:/i.test(x.url)?`<a href="${e(x.url)}" target=_blank rel=noopener>↗</a>`:""}</div>`}
 $("list").innerHTML=h||"<p class=empty>No parts</p>"}
function edit(id){const x=id?D.parts.find(p=>p.id==id):{name:"",part_number:"",vendor:"",url:"",quantity:1,unit_price:0,section:"",status:0,notes:""};eid=id;F.forEach(k=>$("f_"+k).value=x[k]);$("del").style.display=id?"":"none";
 $("secs").innerHTML=[...new Set(D.parts.filter(p=>p.project_id==cur).map(p=>p.section).filter(Boolean))].map(s=>`<option value="${e(s)}">`).join("");$("dlg").showModal()}
async function save(){const b={id:eid,project_id:cur};F.forEach(k=>b[k]=$("f_"+k).value);const r=await api("/api/part",b);if(r.error)return alert(r.error);$("dlg").close();load()}
async function del(){if(confirm("Delete this part?")){await api("/api/part/delete",{id:eid});$("dlg").close();load()}}
async function cyc(id){const x=D.parts.find(p=>p.id==id);x.status=(x.status+1)%4;await api("/api/part",x);load()}
async function newproj(){const n=prompt("New project name");if(n){const r=await api("/api/project",{name:n});if(r.id)cur=r.id;load()}}
load();document.addEventListener("visibilitychange",()=>{if(!document.hidden&&!$("dlg").open)load()});
</script></body></html>"""


def main():
    ap = argparse.ArgumentParser(description="BOM Tracker web + sync v" + VERSION)
    ap.add_argument("cmd", choices=["serve", "sync", "migrate"])
    ap.add_argument("--db", default=DEF_DB)
    ap.add_argument("--host", default="100.64.0.2")
    ap.add_argument("--port", type=int, default=8787)
    ap.add_argument("--server", default="http://100.64.0.2:8787")
    a = ap.parse_args()
    if a.cmd == "sync": return sync(a.db, a.server)
    c = connect(a.db); migrate(c, a.db); c.close()
    if a.cmd == "migrate": return print("migrated", a.db)
    H.db = a.db
    print("BOM web v%s on http://%s:%d (db %s)" % (VERSION, a.host, a.port, a.db), flush=True)
    ThreadingHTTPServer((a.host, a.port), H).serve_forever()


if __name__ == "__main__":
    main()
