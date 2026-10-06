# BOM Tracker: web UI and sync (`bom_web.py`)

Applies to `bom_web.py` v1.1.0 (its changelog is in the file header).

A single stdlib-only Python file that adds a phone-friendly web UI to BOM Tracker and keeps
`bom.db` in sync across machines. The C++ app is unchanged and keeps working as before.

## How it works

- On first run it migrates `bom.db` (after saving a backup in `backups/`): adds `uuid` and
  `updated_at` to `projects` and `parts`, a `tombstones` table, and triggers that stamp every
  insert, edit and delete, including those made by the C++ app.
- Rows are matched across machines by uuid. Rows that existed at migration get deterministic
  `legacy-*` ids, so databases that were identical beforehand stay matched.
- Sync is last-write-wins per row. Deletes propagate as tombstones.
- Changes show up live everywhere: the phone page polls `/api/version` every 3 s and refreshes
  itself, and the desktop app (v1.6.0+) watches `PRAGMA data_version` and reloads within about
  a second of another connection writing (it waits until any open dialog is closed).
- Other machines run `sync --watch`: it checks the local database and the server every 3 s and
  syncs only when something changed (plus a full sync every 60 s as a safety net), so an edit on
  one machine reaches the others in a few seconds.

## Commands

```bash
python3 bom_web.py serve   [--host 100.64.0.2] [--port 8787] [--db PATH]   # web UI + API
python3 bom_web.py sync    [--server http://100.64.0.2:8787] [--db PATH]   # one-shot sync
python3 bom_web.py sync --watch [--server ...] [--db PATH]                 # stay running, sync on change
python3 bom_web.py migrate [--db PATH]                                     # schema only
```

Default db: `~/.local/share/bom-tracker/bom.db`. The default host/server address is the author's
Tailscale address; override it with `--host` / `--server`.

**There is no authentication.** Bind to a private interface (for example a Tailscale address)
and never to `0.0.0.0` on an untrusted network.

## Example setup

Server (Linux, systemd user unit `~/.config/systemd/user/bom-web.service`):

```ini
[Unit]
Description=BOM Tracker web
After=network-online.target

[Service]
ExecStart=/usr/bin/python3 /path/to/bom_web.py serve --host <private-ip> --port 8787
Restart=on-failure
RestartSec=5

[Install]
WantedBy=default.target
```

Other machines: fetch the script from the server (`curl http://<private-ip>:8787/bom_web.py`),
then run `python3 bom_web.py sync --watch --server http://<private-ip>:8787` as a long-running
service (a launchd agent with `KeepAlive` on macOS, or a systemd user unit on Linux). A one-shot
`sync` on a timer (cron, or launchd `StartInterval`) also works but is slower to propagate.
