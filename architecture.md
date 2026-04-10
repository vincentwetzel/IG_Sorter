# Project Architecture

The **IG_Sorter** project consists of two parallel implementations:
1. A legacy Python CLI tool (`Instagram_Sorter.py`)
2. A modern Qt 6 C++ desktop application (`IG_Sorter_Qt/`)

Both operate on the same data model but differ significantly in UX, extensibility, and platform support.

## High-Level Workflow

1. **Input**: Image files are placed in a source directory (unsorted downloads).
2. **Directory Cleanup** (pre-sorting): All output directories are scanned to fix numbering gaps and detect files named after people not in the database.
3. **File Grouping**: The app scans the source directory, parses filenames with regex to extract account handles and post timestamps, then groups files by account+post.
4. **Identification**: Each group is cross-referenced against the JSON database (`ig_people.json`) to resolve IRL names.
5. **Batch Sorting**: Files are presented in batches of configurable size. The user selects thumbnails and assigns them to output folders (SFW, MSFW, NSFW, etc.).
6. **Unknown Handling**: Files that can't be matched to a known account prompt the user for identification, with options to add new entries to the database.
7. **Maintenance**: Periodically, `Find_Duplicate_Files.py` can be run to identify redundancies in the destination directories.

## Core Components

### Qt C++ Application (`IG_Sorter_Qt/`)

#### Data Storage
- **`ig_people.json`**: A JSON file mapping Instagram account handles to real names. Each entry has a `type` field: `personal` (account belongs to the named person), `curator` (account posts content featuring the named person — photographers, feature pages, brands), or `irl_only` (no account — name-only entry for validation).

#### Core Engine
- **`FileNameParser`**: Regex-based filename parsing supporting multiple source types: Instaloader downloads, Facebook downloads (`FB_IMG_<unix_ms>`), TikTok slideshows, and unknown/unmatched files.
- **`FileGrouper`**: Groups files by account handle + post timestamp, resolves IRL names via database lookup, and emits progress signals.
- **`DatabaseManager`**: JSON-based account database with CRUD operations, IRL name lookup, and case-insensitive account matching.
- **`DirectoryCleanup`**: Safe two-phase rename to fix numbering gaps in output directories, with unknown name detection.
- **`SorterEngine`**: Orchestrates the full pipeline: extension fixing → cleanup → grouping → sorting.
- **`ExtensionFixer`**: Detects and corrects mismatched file extensions (e.g., `.jpg` files that are actually `.webp`) using magic byte inspection.
- **`FileUtils`**: Safe copy-then-rename file move with no-overwrite guarantees.

#### UI Layer
- **`MainWindow`**: Manages a `QStackedWidget` with screens: Menu → Cleanup → Sorting → Report.
- **`MenuScreen`**: Startup screen with source folder link and start/settings buttons.
- **`CleanupScreen`**: Progress bars per output directory, unknown name resolution with inline input fields.
- **`SortingScreen`**: Main batch-sorting UI with preview grid, sort panel, sub-batch management, and progress header (`Batch X of Y • Z / N sorted`). Numbers formatted with locale-aware thousand separators.
- **`SortPanel`**: Dynamic output folder buttons, Select All/Deselect All toggle button, Skip Batch, Delete Selected, IRL name display, unknown account input with autocomplete (searchable by IRL name and account handle), favorites quick-fill buttons.
- **`ImagePreviewGrid`**: Aspect-ratio-aware grid of selectable thumbnails with filename labels and pixel dimensions display.
- **`ThumbnailWithLabel`**: Combines thumbnail, pixel dimensions label, and clickable filename hyperlink.
- **`ImageThumbnail`**: Single selectable thumbnail widget with WebP WIC decoder fallback.
- **`AddPersonDialog`**: Modal dialog prompting for IRL name and optional Instagram account when adding new people.
- **`ReportScreen`**: Summary of files sorting results, errors, and directory file counts. All file counts formatted with thousand separators.
- **`SettingsDialog`**: Configuration for source folder, output folders, database path, batch size, and theme.

#### Core Engine
- **SorterEngine Caching**: Groups files and caches results. Subsequent loads skip disk scans if source directory hasn't changed. Cache is invalidated when files are sorted or accounts are added to the database.
- **Multithreaded Sorting**: `sortFiles()` runs each file move in its own thread using `QtConcurrent`. Name generation + file move is atomic under a mutex to prevent duplicate filenames.
- **Targeted Cache Updates**: When a new account is added to the database, only matching groups are updated in-place rather than invalidating the entire cache.
- **Curator/IrlOnly Name Separation**: For these account types, `irlName` represents the MODEL in the photos (per-batch), not the photographer. The text field is cleared after each batch to allow different model names.

#### Utilities
- **`ConfigManager`**: QSettings-based persistence for app configuration.
- **`ThemeManager`**: Light/Dark/System theme support via QSS stylesheets.
- **`LogManager`**: Centralized logging with file rotation.
- **`WebpDecoder`**: Windows WIC-based WebP decoder for thumbnail preview when Qt's built-in support is unavailable.

### Python Scripts

#### Regex Engines
Extensively used in `Instagram_Sorter.py` (`get_ig_name_from_filename`) to parse complex filenames generated by various third-party downloaders:
- Instaloader batch downloader
- Chrome extension downloads
- FastSave Android app
- iPad screenshots (`IMG_XXXX.png`)
- Edited Android screenshots
- Twitter downloads
- Facebook downloads (`FB_IMG_<unix_ms>`)

#### File System Operations
Handled via the built-in `os` and `subprocess` modules to move files, rename files, and open Windows Explorer windows.

#### External Integrations
- **Web Browser Integration**: The `webbrowser` module opens Instagram profile pages for unknown accounts.
- **Instaloader Integration**: `instaloader_downloader.py` reads Firefox's `cookies.sqlite` database to extract session cookies and injects them into Instaloader.

## Directory Structure Considerations

The Python scripts currently rely on hardcoded absolute paths pointing to a specific Google Drive directory structure. Users will need to update the `ROOT_PICTURE_DIR` variable to match their local environment. The Qt app uses configurable paths set via the Settings dialog.
