# BOM Tracker: web UI and sync (`bom_web.py`)

Applies to `bom_web.py` v1.0.0 (its changelog is in the file header).

A single stdlib-only Python file that adds a phone-friendly web UI to BOM Tracker and keeps
`bom.db` in sync across machines. The C++ app is unchanged and keeps working as before.

## How it works

- On first run it migrates `bom.db` (after saving a backup in `backups/`): adds `uuid` and
  `updated_at` to `projects` and `parts`, a `tombstones` table, and triggers that stamp every
  insert, edit and delete, including those made by the C++ app.
- Rows are matched across machines by uuid. Rows that existed at migration get deterministic
  `legacy-*` ids, so databases that were identical beforehand stay matched.
- Sync is last-write-wins per row. Deletes propagate as tombstones.
- The desktop app loads data at startup and after its own edits, so restart it to see changes
  made elsewhere.

## Commands

```bash
python3 bom_web.py serve   [--host 100.64.0.2] [--port 8787] [--db PATH]   # web UI + API
python3 bom_web.py sync    [--server http://100.64.0.2:8787] [--db PATH]   # run on other machines
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
then run `python3 bom_web.py sync --server http://<private-ip>:8787` on a timer
(cron, or a launchd agent with `StartInterval` 300 on macOS).
