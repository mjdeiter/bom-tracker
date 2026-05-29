# Changelog

## [1.1.1] - 2026-05-29

### Added
- **Running subtotal footer** — bottom of the parts panel shows a live subtotal (excl. tax & shipping) in accent gold
- Footer is filter-aware: subtotal only counts rows matching the current search filter
- Left side shows part count; updates to "X / Y parts shown" when a search is active

### Fixed
- Sort/filter state hoisted above `BeginTable` so footer and table rows always walk the same ordered, filtered view

## [1.1.0] - 2026-05-29

### Fixed
- **Selection ID bug** — `g_sel_part_id` was always updated regardless of whether
  a row was clicked (missing braces around the `if(Selectable)` body). Selection
  now correctly tracks across `db_load()` reloads.
- **PRAGMA foreign_keys** — was combined in a multi-statement `sqlite3_exec` call;
  now called as a dedicated single statement immediately after `sqlite3_open`, so
  cascade deletes reliably fire.
- **About modal from menu bar** — `ImGui::OpenPopup()` called inside `BeginMenuBar`
  context caused the dialog to never open. Fixed by setting `g_show_about = true`
  and letting `draw_modals()` fire `OpenPopup` from the correct stack context.
- **WAL mode** — added `PRAGMA journal_mode = WAL` for better concurrent safety.
- **DB open error** — no feedback when `sqlite3_open` fails; now populates status bar.
- **NULL HOME env** — graceful fallback to `/tmp` if `$HOME` is not set.
- **Delete Project menu item** — was always enabled; now greyed out when no project
  is selected.
- **`g_show_edit_project = false` dead code** — redundant flag clears inside modal
  success blocks removed (flag is cleared on `OpenPopup` call, not inside the modal).
- **`io.IniFilename = nullptr`** — suppressed imgui.ini being written to the working
  directory on every run.

### Added
- **Embedded window icon** — 32×32 RGBA BOM-themed icon (gold circuit aesthetic)
  set on the GLFW window via `glfwSetWindowIcon` (appears in title bar and taskbar).
- **Ctrl+N keyboard shortcut** — opens New Project dialog from anywhere in the app.
- **Installed parts counter** — header now shows `(X/Y installed)` progress alongside
  the project total cost.
- **Newly-added part auto-select** — after adding a part, that part is automatically
  selected in the table.
- **Project description tooltip** — hovering a project name in the sidebar shows the
  description as a tooltip.
- **Case-insensitive sort** — `ORDER BY name COLLATE NOCASE` for both projects and
  parts in all queries.
- **Hand cursor on clickable About links** — repo URL and DB path in About dialog now
  show a hand cursor on hover.
- **`format_price` zero_as_dash flag** — Total column now shows `$0.00` for free
  parts (previously showed em-dash, making free items look like they had no price).
- **`reinterpret_cast`** — replaced C-style casts on `sqlite3_column_text` return values.
- **README.md** and **CHANGELOG.md** added.
- **`.desktop` file** install instructions in README.

### Changed
- Version bumped to `1.1.0`.
- Default window size increased to 1160×700.
- Search box width increased to 220px.
- Notes truncation now uses a Unicode ellipsis (`…`) instead of `...`.
- Em-dash (`—`) used consistently for empty part number, vendor, notes cells.

## [1.0.0] - 2026-05-26

Initial release. Dear ImGui + SQLite3 BOM tracker with projects, parts, status
tracking, sortable table, search, and URL support.
