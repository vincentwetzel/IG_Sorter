# Project Architecture

IG_Sorter is a Qt 6 / C++17 desktop application. Its primary project is `IG_Sorter_Qt/`; user databases, downloaded media, and downloader tooling are deliberately kept outside the repository.

## Workflow

1. **Configure** source, destination, database, batch size, and theme in Settings. These preferences are persisted with `QSettings`.
2. **Clean output directories** concurrently across configured destinations to repair file numbering and identify names that need review against the people database.
3. **Discover and group source files** by walking the source tree, correcting detected image-extension mismatches, parsing filenames, and resolving account handles. After discovery finishes, posts for known Personal accounts are consolidated into one group before the sorting screen receives results. Curator and IRL-only posts remain separate because each may need a different per-batch sorting name. Unrecognized files begin as individual groups, then files in the same directory whose creation/save times fall within five seconds are conservatively combined. Exact-byte duplicate checks against output folders are deferred until the user sorts a batch.
4. **Review batches** in the preview grid. The user selects files, supplies or confirms the person name, and chooses a destination. For Curator groups, the name field identifies the person shown in that batch; the adjacent account-type selector controls the type suggested when adding that person. The curator remains the source account and is not automatically assigned to the person. Existing names are recognized, and database-add failures are reported in the UI.
5. **Review the report** for sorted/skipped counts, errors, and output-folder counts.
6. **Find duplicates when needed** by scanning configured output folders, reviewing streamed groups, and choosing files to keep or send to the Recycle Bin.

## Application layers

### Core (`IG_Sorter_Qt/src/core/`)

- `FileNameParser` recognizes supported filename conventions and returns the account/source metadata used downstream.
- `FileGrouper` scans source directories and resolves known accounts. Personal entries need a stored name to be considered known; Curator and IRL-only entries are known by type even when their name is empty, because the sorting name is entered for each batch. The interactive sorting flow uses `groupStreaming` to scan in chunks and report progress, then consolidates posts only for known Personal accounts after discovery finishes. Curator and IRL-only groups remain separate. Unknown files are first kept separate and then grouped only when their creation/save times are within five seconds in the same directory; if no usable timestamp exists, they remain separate. Screenshot groups follow their source-specific merge behavior. The non-streaming `group` method follows the same rules.
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
- `CleanUpAccountsScreen` presents the database in an editable table with account/name/type columns, text search, a missing-name filter, selected-entry removal, and a Save Changes action that applies pending row edits together.
- `SortingScreen` presents completed account groups and sub-batches, coordinates asynchronous move, delete, and undo actions, and updates favorite-name quick-fill counts as a sort starts. If the whole sort action fails, it removes that provisional count; a partial success keeps the count. When undo returns to an earlier batch, the preview omits paths that are still absent from the source directory and the sorted count is reduced by the number of restored files.
- `ImagePreviewGrid`, `ImageThumbnail`, `ThumbnailWithLabel`, and `FlowLayout` implement the responsive preview surface.
- `SortPanel` provides destination, selection, name, and unknown-account controls. Curator batches show an account-type selector beside **Add Person** and pass the selected type to the person-resolution flow.
- `AddPersonDialog` creates person/account entries when needed, with the account type suggested by the sorting panel and the account handle left blank unless the user supplies one.
- `DuplicateFinderScreen` displays streamed matches and manages review, deletion, and undo. While scanning, it opens new groups as they arrive if the user has reached the end of available results, and resumes the current review if the scan finishes while waiting.
- `ReportScreen` summarizes the sorting run.
- `SettingsDialog` edits the source, outputs, database, batch size, and theme.
- `MainWindow` tracks screen history for the mouse Back button and clears that history when returning to the main menu.

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

Unrecognized names remain available to the grouping/sorting workflow. Unknown files are initially separate, then files with creation/save times within five seconds of the first file in a cluster are combined when they share a directory; files without a usable timestamp stay separate. Posts with the same known Personal account handle are combined after scanning, regardless of post timestamp; Curator and IRL-only posts stay separate. Changes to parser patterns should be reflected here and in the README when they affect first-run usage.

The sorting preview adapts to the available panel width: favorite-name quick-fill buttons remain on one row and entries beyond the available width are hidden. Name completion is shown only when the cursor is at the end of the typed text. Thumbnail selection outlines and checkmarks track the displayed image bounds, including when aspect-ratio padding leaves empty space inside a cell.

## Database and settings

The database path is selected by the user; there is no default private database in the repository. The JSON root is an array of objects with `name`, `account`, and `type` fields. Supported types are `personal`, `curator`, and `irl_only`. An IRL-only record has a null account. Account matching is case-insensitive. Person names are compatibility-normalized using Unicode NFKC and trimmed when loaded, saved, added, or edited. This maps compatibility characters, including mathematical styled letters, to their ordinary equivalents so lookups and sorted filenames use a consistent name.

`QSettings` stores the source directory, ordered output folder labels and paths, database path, batch size, and theme. Database saves use `QSaveFile` so incomplete writes do not replace the existing JSON file. Logs and duplicate-hash cache are placed under the platform's application data location. These runtime files are not source-controlled project content.

## Duplicate matching and file removal

The duplicate finder recursively walks the configured output directories. It reuses cached file results when path metadata (including size and modification time) remains valid. Image candidates use perceptual image hashes, then a fast 16x16 grayscale thumbnail comparison at an 85% similarity threshold, followed by a 128x128 grayscale pixel comparison at a 94% threshold. The detailed comparison confirms image matches after the small thumbnail has identified a candidate. Unsupported/non-image cases use exact SHA-256 comparison. Candidate grouping also considers file size and the person name inferred from the standardized output filename.

These are candidate-matching rules, not a guarantee that every visual variation is found. Resized, recompressed, or cropped files may not match. The review screen is the confirmation step: deletion is user-directed and uses the Recycle Bin/undo workflow.

### Automatic exact duplicate removal in sorting

The sorting pipeline has an additional exact-byte check, separate from the Duplicate Finder screen. The earlier pre-group pass that hashed source files against each other has been removed. When the user sorts a batch, the pipeline checks same-name candidates in configured output folders by size and SHA-256, then removes a confirmed duplicate source instead of moving it. This automatic removal is not sent to the Recycle Bin and is not included in the undo workflow. A hash/read failure prevents a match. The interactive grouping path corrects detected image-extension mismatches as it discovers eligible files, then emits groups in chunks of up to 20 files and flushes at directory boundaries. This lets sorting begin before the scan finishes, but a post may be split when its files cross a chunk or directory boundary. The non-streaming grouper's screenshot merge is not part of this interactive path.

The Duplicate Finder screen remains the manual review flow: it presents likely groups and sends user-selected files to the Recycle Bin with its own undo support. It uses perceptual matching for supported images and exact-byte SHA-256 matching for unsupported content.

## Background work and ownership

Filesystem scans, grouping, cleanup, duplicate scanning, and sorting actions can be long-running. Output-directory cleanup submits one task per configured directory to a dedicated thread pool and reports progress as tasks run. Group discovery reports progress while scanning, but the sorting screen waits for the complete scan and account consolidation before presenting groups. The UI uses `QtConcurrent` and `QFutureWatcher` where appropriate, then applies results and updates widgets on the UI thread. Sort tasks and Undo share a mutex so undo history is not read or changed while a sort is mutating it; the Undo control is disabled until pending sorts finish. After a successful undo, the preview returns to the batch containing a restored source file when that file remains in the current groups, filters out paths that are still missing from the source directory, and updates the sorted count. QObject-derived components use Qt parent-child ownership. Keep worker inputs/results value-based where practical and do not access widgets from worker tasks.

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



