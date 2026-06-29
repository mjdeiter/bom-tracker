# BOM Tracker

A lightweight Bill of Materials tracker built with [Dear ImGui](https://github.com/ocornut/imgui) and SQLite3.
Designed for hardware hobbyists, makers, and homelab builders who want a fast, offline, no-nonsense parts tracker.

![BOM Tracker](https://raw.githubusercontent.com/mjdeiter/bom-tracker/master/screenshot.png)

## Features

- **Markdown Import** — `Project → Import from Markdown...` to auto-populate a project
  from any Markdown BOM file; columns matched by name, `##` headings become section groups
- **Section grouping** — parts are grouped under collapsible `▸ Section` header rows
  matching the `##` structure of the source Markdown
- **Projects** — organize parts into named projects with an optional description
- **Parts list** — track name, part number, vendor, URL, quantity, unit price, status, and section
- **Status tracking** — Needed / Ordered / In Stock / Installed with color coding
- **Auto-totals** — per-project cost total and installed/total part count in the header
- **Sortable table** — click any column header to sort (section grouping preserved)
- **Search** — filter parts by name, part number, vendor, notes, or URL
- **One-click URLs** — double-click any part row (or use Open URL button) to open in browser
- **Print BOM** — generates a print-ready HTML file for the selected project
- **Persistent storage** — SQLite3 database at `~/.local/share/bom-tracker/bom.db`
- **Embedded icon** — gold BOM-themed icon in both window title bar and taskbar
- **Keyboard shortcut** — `Ctrl+N` to create a new project

## Requirements

- Linux (X11 or Wayland/XWayland)
- GCC / G++ with C++17
- GLFW3, OpenGL, SQLite3 development headers

```bash
# Arch / CachyOS
sudo pacman -S glfw sqlite3 mesa

# Debian / Ubuntu
sudo apt install libglfw3-dev libsqlite3-dev libgl-dev
```

You also need ImGui vendored in an `imgui/` subdirectory (not included in this repo — see Build section).

## Build

```bash
git clone https://github.com/mjdeiter/bom-tracker
cd bom-tracker

# Vendor ImGui (copy from your existing project or clone separately)
git clone https://github.com/ocornut/imgui imgui
# Checkout a recent stable tag if you like: cd imgui && git checkout v1.90.x

bash build.sh
```

`build.sh` compiles and installs to `~/.local/bin/bom-tracker`.

## Install desktop entry (taskbar / app launcher)

```bash
mkdir -p ~/.local/share/applications
cat > ~/.local/share/applications/bom-tracker.desktop << 'DESKTOP'
[Desktop Entry]
Name=BOM Tracker
Comment=Bill of Materials tracker
Exec=/home/$USER/.local/bin/bom-tracker
Icon=utilities-system-monitor
Terminal=false
Type=Application
Categories=Utility;
DESKTOP
```

## Usage

| Action | How |
|---|---|
| New project | `+ New Project` button or `Ctrl+N` |
| Import from Markdown | `Project → Import from Markdown...`, browse to a `.md` file, Preview, Import |
| Edit/delete project | Select project, use `Project` menu or toolbar |
| Add part | Select a project, click `+ Add Part` |
| Edit/delete part | Select a row, click `Edit` or `Delete` |
| Open part URL | Double-click a row, or select + `Open URL` |
| Print BOM | Select a project, click `Print BOM` |
| Search | Type in the search box (top-right of parts panel) |

### Markdown Import Format

The importer is flexible — columns are matched by name in any order. A minimal example:

```markdown
# My Project Name
Optional description text.

| Part Name | Part # | Vendor | Qty | Unit Price | Status | Notes | URL |
|-----------|--------|--------|-----|------------|--------|-------|-----|
| ESP32-S3 DevKit | ESP32-S3-DEVKITC-1 | Adafruit | 1 | 13.95 | Needed | Main MCU | https://... |
```

Use `## Section Name` headings between tables to create named groups in the parts view.
Column aliases recognised: `Item`, `Component`, `Material` → Part Name; `SKU`, `MPN` → Part #;
`Description / Specification` → Notes; `Qty` / `Quantity` / `Count` → Qty; etc.

## Data

Database lives at `~/.local/share/bom-tracker/bom.db` — plain SQLite3, back it up or inspect it with any SQLite browser.

## License

MIT
