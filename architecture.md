# Project Architecture

The **IG_Sorter** project is a Qt 6 C++ desktop application (`IG_Sorter_Qt/`). It manages the image sorting workflow while private data and downloader tools live outside this repository.

## High-Level Workflow

1. **Input**: Image files are placed in a configured source directory.
2. **Directory Cleanup**: Output directories are scanned to fix numbering gaps and detect files named after people not in the database.
3. **File Grouping**: The app scans the source directory, parses filenames, extracts account handles and post timestamps, then groups files by account and post.
4. **Identification**: Each group is cross-referenced against the configured JSON database (`ig_people.json`) to resolve IRL names.
5. **Batch Sorting**: Files are presented in batches. The user selects thumbnails and assigns them to output folders.
6. **Unknown Handling**: Files that cannot be matched prompt the user for identification, with options to add new database entries.
7. **Maintenance**: The built-in duplicate finder can be run to identify redundant files in destination directories.

## Core Components

### Data Storage

- **`ig_people.json`**: User-provided JSON database mapping account handles to real names. Each entry has a `type` field: `personal`, `curator`, or `irl_only`.
- The database path is configured in the Settings dialog. The repository does not include a default private database.

### Core Engine

- **`FileNameParser`**: Regex-based filename parsing supporting Instaloader-style downloads, Facebook downloads (`FB_IMG_<unix_ms>`), TikTok slideshows, and unknown/unmatched files.
- **`FileGrouper`**: Groups files by account handle and post timestamp, resolves IRL names via database lookup, and emits progress signals.
- **`DatabaseManager`**: JSON-based account database with CRUD operations, IRL name lookup, case-insensitive account matching, and incremental index maintenance for fast updates.
- **`DirectoryCleanup`**: Safe two-phase rename to fix numbering gaps in output directories, with unknown name detection.
- **`SorterEngine`**: Orchestrates the full pipeline: extension fixing -> cleanup -> grouping -> sorting.
- **`ExtensionFixer`**: Detects and corrects mismatched file extensions using magic byte inspection.
- **`DuplicateFinder`**: Recursively scans configured output folders, caches file hashes, uses perceptual image hashes plus cached 16x16 grayscale thumbnail comparisons for visual matches, skips obvious non-matches by comparing already-sorted person names, falls back to exact byte matching for non-images, and streams groups to the UI as they are discovered.
- **`FileUtils`**: Safe copy-then-rename file move with no-overwrite guarantees.

### UI Layer

- **`MainWindow`**: Manages the screen stack: Menu -> Cleanup -> Sorting -> Report, plus account cleanup and duplicate finder screens.
- **`MenuScreen`**: Startup screen with source folder link and Start Sorting, Clean Up Accounts, Find Duplicates, and Settings buttons.
- **`CleanupScreen`**: Progress bars per output directory and unknown name resolution.
- **`SortingScreen`**: Main batch-sorting UI with preview grid, sort panel, sub-batch management, progress header, empty-group skipping, and defensive bounds handling.
- **`SortPanel`**: Output folder buttons, selection controls, unknown account input, autocomplete, and favorites quick-fill buttons.
- **`ImagePreviewGrid`**: Aspect-ratio-aware grid of selectable thumbnails with filename labels and pixel dimensions.
- **`ThumbnailWithLabel`**: Combines thumbnail, pixel dimensions label, and clickable filename hyperlink.
- **`ImageThumbnail`**: Single selectable thumbnail widget with WebP decoder fallback.
- **`AddPersonDialog`**: Modal dialog prompting for IRL name and optional Instagram account.
- **`ReportScreen`**: Summary of sorting results, errors, and directory file counts.
- **`DuplicateFinderScreen`**: Finds and removes duplicate files across output directories, with live scan progress, cancellation, streaming group discovery, image previews, undo support, responsive label sizing for long filenames, and cached grayscale similarity checks layered on top of perceptual hashes.
- **`SettingsDialog`**: Configuration for source folder, output folders, database path, batch size, and theme.

### Utilities

- **`ConfigManager`**: `QSettings`-based persistence for app configuration.
- **`ThemeManager`**: Light/Dark/System theme support via QSS stylesheets.
- **`LogManager`**: Centralized logging with file rotation.
- **`WebpDecoder`**: Windows WIC-based WebP decoder for thumbnail preview when Qt's built-in support is unavailable.

## Directory Structure Considerations

The Qt app uses configurable source, output, and database paths set via the Settings dialog. This repository does not include private data, downloader scripts, credentials, session files, download history, or downloaded media.
