# Project Architecture

IG_Sorter is a Qt 6 / C++17 desktop application. Its primary project is `IG_Sorter_Qt/`; user databases, downloaded media, and downloader tooling are deliberately kept outside the repository.

## Workflow

1. **Configure** source, destination, database, batch size, and theme in Settings. These preferences are persisted with `QSettings`.
2. **Clean output directories** to repair file numbering and identify names that need review against the people database.
3. **Discover and group source files** by walking the source tree, correcting detected image-extension mismatches, parsing filenames, and resolving account handles. The sorting screen receives groups as the scan runs. Exact-byte duplicate checks against output folders are deferred until the user sorts a batch.
4. **Review batches** in the preview grid. The user selects files, supplies or confirms the person name, and chooses a destination.
5. **Review the report** for sorted/skipped counts, errors, and output-folder counts.
6. **Find duplicates when needed** by scanning configured output folders, reviewing streamed groups, and choosing files to keep or send to the Recycle Bin.

## Application layers

### Core (`IG_Sorter_Qt/src/core/`)

- `FileNameParser` recognizes supported filename conventions and returns the account/source metadata used downstream.
- `FileGrouper` scans source directories and resolves known accounts. The interactive sorting flow uses `groupStreaming`, which emits sorted groups after each chunk of up to 20 eligible image files and at each directory boundary. Grouping is local to each emitted chunk, so one post's files can appear in separate groups if a chunk boundary falls between them. This streaming path does not perform the screenshot-group merging used by the non-streaming `group` method.
- `DatabaseManager` loads and saves the selected JSON database, provides case-insensitive account lookup, and maintains an account-to-entry index.
- `DirectoryCleanup` analyzes output directory names and performs safe numbering cleanup.
- `ExtensionFixer` checks file content and corrects mismatched image extensions during streaming source discovery.
- `SorterEngine` coordinates cleanup, grouping, sorting, duplicate checks during sorting, and undo history. It also keeps an in-memory grouping cache.
- `DuplicateFinder` recursively scans output paths, computes and caches file metadata/hashes, and emits likely duplicate groups.
- `Types.h` contains shared report data types.

### UI (`IG_Sorter_Qt/src/ui/`)

- `MainWindow` owns the screen flow and starts longer cleanup and grouping tasks.
- `MenuScreen` provides entry points to sorting, account cleanup, duplicate finding, and Settings.
- `CleanupScreen` presents cleanup progress and unresolved names.
- `CleanUpAccountsScreen` supports reviewing account entries in the database.
- `SortingScreen` presents groups and sub-batches, and coordinates asynchronous move, delete, and undo actions.
- `ImagePreviewGrid`, `ImageThumbnail`, `ThumbnailWithLabel`, and `FlowLayout` implement the responsive preview surface.
- `SortPanel` provides destination, selection, name, and unknown-account controls.
- `AddPersonDialog` creates person/account entries when needed.
- `DuplicateFinderScreen` displays streamed matches and manages review, deletion, and undo.
- `ReportScreen` summarizes the sorting run.
- `SettingsDialog` edits the source, outputs, database, batch size, and theme.

### Utilities (`IG_Sorter_Qt/src/utils/`)

- `ConfigManager` persists settings through `QSettings`.
- `FileUtils` implements safe file movement and related path operations.
- `LogManager` centralizes rotating application logs.
- `ThemeManager` applies light, dark, or system theme behavior using QSS resources.
- `WebpDecoder` uses Windows Imaging Component as a WebP preview fallback on Windows.

## Filename recognition

The parser works on the filename portion of a path. Current recognized patterns include:

| Source | Filename shape | Grouping identity |
| --- | --- | --- |
| Instagram / Instaloader | `<account>_YYYY-MM-DD_HH-MM-SS[_<sequence>].ext` | Account and post timestamp |
| TikTok slideshow | 32 hexadecimal characters followed by an extension | Shared TikTok slideshow source label |
| Facebook | `FB_IMG_<13-digit Unix milliseconds>.ext` | Converted timestamp and Facebook source label |
| Twitter / X | `YYYYMMDD_HHMMSS.ext` | Converted timestamp and Twitter source label |
| Android screenshot | `Screenshot_YYYYMMDD_HHMMSS_<Instagram|Snapchat|Chrome>.ext` | App source label and screenshot timestamp |

Unrecognized names remain unmatched and are still available to the grouping/sorting workflow according to the grouper's fallback behavior. Changes to parser patterns should be reflected here and in the README when they affect first-run usage.

## Database and settings

The database path is selected by the user; there is no default private database in the repository. The JSON root is an array of objects with `name`, `account`, and `type` fields. Supported types are `personal`, `curator`, and `irl_only`. An IRL-only record has a null account. Account matching is case-insensitive.

`QSettings` stores the source directory, ordered output folder labels and paths, database path, batch size, and theme. Logs and duplicate-hash cache are placed under the platform's application data location. These runtime files are not source-controlled project content.

## Duplicate matching and file removal

The duplicate finder recursively walks the configured output directories. It reuses cached file results when path metadata (including size and modification time) remains valid. Image candidates use perceptual image hashes followed by a small grayscale thumbnail comparison; unsupported/non-image cases use exact SHA-256 comparison. Candidate grouping also considers file size and the person name inferred from the standardized output filename.

These are candidate-matching rules, not a guarantee that every visual variation is found. Resized, recompressed, or cropped files may not match. The review screen is the confirmation step: deletion is user-directed and uses the Recycle Bin/undo workflow.

### Automatic exact duplicate removal in sorting

The sorting pipeline has an additional exact-byte check, separate from the Duplicate Finder screen. The earlier pre-group pass that hashed source files against each other has been removed. When the user sorts a batch, the pipeline checks same-name candidates in configured output folders by size and SHA-256, then removes a confirmed duplicate source instead of moving it. This automatic removal is not sent to the Recycle Bin and is not included in the undo workflow. A hash/read failure prevents a match. The interactive grouping path corrects detected image-extension mismatches as it discovers eligible files, then emits groups in chunks of up to 20 files and flushes at directory boundaries. This lets sorting begin before the scan finishes, but a post may be split when its files cross a chunk or directory boundary. The non-streaming grouper's screenshot merge is not part of this interactive path.

The Duplicate Finder screen remains the manual review flow: it presents likely groups and sends user-selected files to the Recycle Bin with its own undo support. It uses perceptual matching for supported images and exact-byte SHA-256 matching for unsupported content.

## Background work and ownership

Filesystem scans, grouping, cleanup, duplicate scanning, and sorting actions can be long-running. The UI uses `QtConcurrent` and `QFutureWatcher` where appropriate, then applies results and updates widgets on the UI thread. QObject-derived components use Qt parent-child ownership. Keep worker inputs/results value-based where practical and do not access widgets from worker tasks.

## Source tree

```text
IG_Sorter_Qt/
├── CMakeLists.txt
├── resources/                 # Qt resource collection and QSS themes
└── src/
    ├── core/                  # Parsing, grouping, database, sorting, cleanup, duplicates
    ├── ui/                    # Screens, dialogs, and preview widgets
    ├── utils/                 # Settings, logging, themes, file and image helpers
    └── main.cpp
```

The authoritative list of compiled sources and Qt modules is in `IG_Sorter_Qt/CMakeLists.txt`.

## Private data boundary

`ig_people.json`, credentials, downloader scripts, session files, download history, and downloaded media belong in a separate private project or local-only location. The application accesses the database only through the path selected in Settings.



