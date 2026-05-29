# BOM Tracker

A lightweight Bill of Materials tracker built with [Dear ImGui](https://github.com/ocornut/imgui) and SQLite3.
Designed for hardware hobbyists, makers, and homelab builders who want a fast, offline, no-nonsense parts tracker.

![BOM Tracker](https://raw.githubusercontent.com/mjdeiter/bom-tracker/master/screenshot.png)

## Features

- **Projects** — organize parts into named projects with an optional description
- **Parts list** — track name, part number, vendor, URL, quantity, unit price, and status
- **Status tracking** — Needed / Ordered / In Stock / Installed with color coding
- **Auto-totals** — per-project cost total and installed/total part count in the header
- **Sortable table** — click any column header to sort
- **Search** — filter parts by name, part number, vendor, notes, or URL
- **One-click URLs** — double-click any part row (or use Open URL button) to open in browser
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
| Edit/delete project | Right-click project name in sidebar |
| Add part | Select a project, click `+ Add Part` |
| Edit/delete part | Select a row, click `Edit` or `Delete` |
| Open part URL | Double-click a row, or select + `Open URL` |
| Search | Type in the search box (top-right of parts panel) |

## Data

Database lives at `~/.local/share/bom-tracker/bom.db` — plain SQLite3, back it up or inspect it with any SQLite browser.

## License

MIT
