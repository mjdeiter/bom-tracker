# Changelog

## [1.3.0] - 2026-06-29

### Added
- **Markdown BOM Import** — `Project → Import from Markdown...` parses any Markdown file
  and creates a project with all parts pre-populated.
  - First `# Heading` becomes the project name; body text before the first table becomes
    the description.
  - Common title prefixes (`Bill of Materials (BOM):`, `BOM:`, etc.) are stripped
    automatically from the project name.
  - All tables in the file are scanned and merged into one project.
  - Columns matched case-insensitively by name: `Part Name` / `Item` / `Component` /
    `Material`, `Part #` / `SKU`, `Vendor`, `Qty`, `Unit Price`, `Status`,
    `Description` / `Description / Specification`, `Notes`, `URL`.
  - Inline Markdown formatting (`**bold**`, `*italic*`, `` `code` ``) stripped from
    cell values automatically.
  - Non-integer quantities (`As needed`, `1 spool`, `2–4`) handled gracefully — falls
    back to qty 1 with the raw value appended to notes.
  - `Description` and `Notes` columns merged as `spec — note` when both are present.
  - **Preview step** shows project name, description, and up to 8 parts before
    committing to the database.
  - **Browse button** uses `kdialog` (KDE/Plasma) with `zenity` (GNOME/GTK) as fallback.
- **Section grouping** — `## Headings` in Markdown files are tracked during import and
  stamped onto each part as its section. Parts table displays dim `▸ Section Name` header
  rows between groups. Section is the primary sort key; the user-chosen column sort is
  secondary within each section.
- **`section` field** added to `Part` and SQLite schema. Existing databases are
  auto-migrated via `ALTER TABLE parts ADD COLUMN section` on first launch.

### Fixed
- **Column resize** — switched parts table from `SizingStretchProp` to `SizingFixedFit`
  so dragging a column border only affects that column; `Notes` column uses
  `WidthStretch` to fill remaining space.
- **Browse button** — now correctly finds and uses `kdialog` on KDE/Plasma systems
  where `zenity` is not installed.

## [1.2.0] - 2026-05-29

### Added
- **Print BOM** — toolbar button and Project menu item that generate a print-ready HTML file
  for the selected project and open it in the default browser. The page auto-triggers
  `window.print()` on load, shows all columns (Part Name, Part #, Vendor, Qty, Unit Price,
  Total, Status, Notes), includes a grand total footer row, links part names to their URLs,
  and uses `@media print` CSS for clean output. File written to `/tmp/bom_<project>.html`.

## [1.1.2] - 2026-05-29

### Fixed
- **No emoji in search hint** — removed 🔍 glyph from search placeholder; ImGui font atlas does not cover emoji codepoints, causing startup font errors
- **Shell injection in open_url** — replaced `system()` call with `fork()`+`execl()` to prevent malicious URLs from executing shell code
- **NULL crash in db_load** — `sqlite3_column_text` can return NULL for empty columns; added `col_str()` helper with null guard throughout
- **Status bar never clears** — DB error messages now fade out over 3 s and clear after 8 s
- **Double sort per frame** — sort lambda extracted to `do_sort()` and called once; the redundant duplicate inside `SpecsDirty` replaced with the shared call
- **Selected project lost after db_load** — added `g_sel_project_id` mirroring the pattern used for parts; selection survives reloads and reorders

### Changed
- Header bar no longer duplicates the subtotal — shows part count and installed progress only; cost is shown exclusively in the footer
- Added `PRAGMA synchronous = NORMAL` for faster writes under WAL mode

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
