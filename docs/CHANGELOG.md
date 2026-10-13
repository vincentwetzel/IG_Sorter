# Changelog

All notable changes to the IG_Sorter project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed
- **Account cleanup table**: added account search, a missing-name filter, inline editing of account/name/type fields, and removal of the selected entry. Pending edits are applied together with Save Changes.
- **Unknown-file grouping**: unknown filename types start as individual files and are combined only when they share a directory and have creation/save times within five seconds of the cluster's first file; files without a usable timestamp remain separate.
- **Favorite quick-fill layout**: buttons stay on one row and overflow entries are hidden when the panel is too narrow. Name ghost completion is limited to typing at the end of the field.
- **Database persistence**: JSON saves now use an atomic file replacement, and updates reject duplicate account handles or duplicate name-only aliases already represented by an account-backed entry.
- **Sorting name resolution**: an entered name takes precedence over the group's stored name for the current sort. Curator and IRL-only batch names are not saved as name-only database entries, and screenshot source labels do not trigger Instagram account-link prompts.
- **Cleanup duplicate-name handling**: resolving an output-folder name already present in the database no longer adds a duplicate name-only entry.
- **Person-name normalization**: names are compatibility-normalized and trimmed when entered or stored, so stylized Unicode characters such as mathematical alphabets resolve consistently with ordinary text.
- **Sort undo coordination**: Undo is disabled while sort tasks are pending and serialized with sorting. After restoring files, the preview returns to their batch, skips files that remain sorted, and subtracts restored files from the sorted count.
- **Duplicate image verification**: image candidates that pass the fast 16x16 grayscale comparison are checked again at 128x128 resolution with a stricter similarity threshold before they are grouped.
- **Duplicate scan navigation**: when review reaches the end of currently available groups during a scan, newly discovered groups open as they arrive; the active review is restored if the scan finishes while waiting.
- **Curator Add Person flow**: the sorting panel now shows an account-type selector beside **Add Person**. The selected type is carried into the add dialog, the curator source handle is not prefilled as the model's account, and duplicate or failed additions give visible feedback.
- **C++ maintenance cleanup**: removed redundant state and duplicate code paths across sorting, grouping, cleanup, file utilities, and UI components; retained behavior while reducing code size.
- **Sorting group consolidation**: posts are combined across post boundaries only for known Personal accounts; Curator and IRL-only groups remain separate for per-batch name entry.
- **Sorting preview layout**: favorite-name quick-fill buttons hide overflow at narrower panel widths, and selection borders/checkmarks stay aligned with the displayed image rather than the full thumbnail cell.
- **Continue button theme styling**: added matching enabled, hover, and pressed states in the light and dark themes.
- **Favorite-name quick-fill feedback**: the selected name's usage count updates as soon as sorting starts, then rolls back if every selected file operation fails.
- **Output cleanup concurrency**: configured output directories are cleaned in parallel on a dedicated thread pool, with per-directory progress and completion updates.
- **Screen navigation**: added screen history and mouse Back-button navigation, while returning to the menu resets the history.
- **Known Curator and IRL-only accounts**: these account types remain recognized when their stored name is empty; the sorting name is supplied per batch.
- **Database manager indexing**: account lookups now keep an incremental in-memory index instead of rebuilding the whole map after every update or delete.
- **Duplicate finder thumbnail path**: perceptual-hash matches now run through a cached grayscale thumbnail similarity pass that compares actual pixel data rather than palette indices.
- **Sorting groups by account**: known Personal account posts are consolidated after source discovery, while Curator and IRL-only posts remain separate. The sorting screen waits for the scan to finish before presenting groups, and the sort panel labels groups by account type.
- **Duplicate-check timing**: removed the pre-group source-to-source hash pass; exact-byte checks against existing output files now run when a batch is sorted.
- **Source discovery and grouping**: detected image-extension mismatches are corrected as eligible files are found; the completed scan consolidates known Personal account posts while keeping Curator and IRL-only groups separate.
- **Resizable preview grid**: preview cells now yield to available window space, so long filenames and previously wide layouts do not force the sorting window to stay large.
- **Preview grid relayouts**: thumbnail removals now batch layout updates and defer relayout work, which keeps the sorting grid responsive during large deletes or batch transitions.
- **Cleanup screen rebuilds**: directory progress and resolution widgets are torn down and rebuilt safely between cleanup sessions to avoid stale UI state.
- **Async sort actions**: file moves, recycle-bin deletes, and undo operations run off the UI thread. The sorting screen removes selected thumbnails and advances optimistically, then reconciles its progress count when the operation completes.

### Removed
- Removed the Instaloader downloader script, download history, downloader settings, downloaded media, and private-data submodule from this project. Downloader tooling and private data now live outside this repository.

### Added
- **Exact-byte duplicate handling in sorting**: known-person files are compared with matching-name output files when a batch is sorted. A detected source duplicate is removed directly with `QFile::remove`, without Recycle Bin recovery or undo. Sorting also rejects blank names before starting file operations.
- **Sorting UI responsiveness**: selected thumbnails are removed and the next batch is shown before the asynchronous move completes; the progress count is corrected when failures return. Sort tasks are serialized to avoid overlapping operations.
- **Preview and cleanup efficiency**: preview selection/removal uses standard algorithms and batched/deferred relayouts; cleanup reuses compiled filename matching expressions.
- **Preview grid responsiveness**: the image preview grid now uses standard algorithms, batched update suppression, and deferred relayouts to avoid layout thrashing when thumbnails are removed in quick succession.
- **Cleanup resolution lifecycle fixes**: the cleanup screen now clears and recreates resolution rows cleanly, including layout teardown, before showing a new set of unresolved names.
- **Thumbnail label simplification**: filename labels now slice the display name directly from the path and use a simpler platform-open path for the hyperlink handler.
- **Duplicate finder scan quality improvements**: duplicate matching now uses a cached 16x16 grayscale thumbnail similarity pass on top of perceptual hashes, with a name-based fast reject for already-sorted files to reduce false positives
- **Duplicate finder UI sizing fixes**: the duplicate finder screen now constrains label growth and elides long filenames so the window stays stable during review
- **IDE-style Tab autocomplete** for "Who is in these photos" field: gray ghost text appears after typed text, Tab commits the full name with proper capitalization and closes the completer dropdown
- **App-wide button styling via QSS themes**: removed all inline `setStyleSheet()` calls, added semantic button variants (#deleteButton, #curatorSortButton, #scanButton, etc.) to both light and dark themes
- **Find Duplicate Files screen**: New menu option launching a dedicated duplicate finder with image preview grid, visual similarity comparison, and one-click delete with undo
  - Recursive scanning of all configured output folders
  - Live streaming of groups as they are discovered during the scan
  - Cancel/restart support while the background worker is running
  - Hash cache persisted under the app data directory to speed up repeat scans
  - Perceptual image hashing for visual matches and exact-byte fallback for unsupported images / non-images
  - Groups files by byte size AND extracted person name (e.g. "Michael Doherty" vs "Michael Zanderigo" are never paired)
  - Visual similarity check: 16x16 grayscale thumbnail comparison, 85% similarity threshold required
  - Files sorted by trailing number in filename (lowest number shown first, auto-selected as the keeper)
  - Side-by-side green/red labels show which files will be kept vs deleted
  - Files sent to Recycle Bin (not permanently deleted)
  - Undo button restores files from temp staging back to original locations
  - Async background scan with progress indicator
  - Human-readable file sizes (KB, MB, GB)
- **Grouping cache**: SorterEngine caches grouping results and skips redundant disk scans when source directory hasn't changed, dramatically improving sorting screen load time on subsequent runs
- **Select All / Deselect All button**: Positioned between Skip Batch and Delete Selected in the sort panel. Automatically toggles between "Select All" and "Deselect All" based on current selection state
- **Pixel dimensions display**: Each thumbnail in the sorting grid now shows its dimensions (e.g. `720x720`) above the filename hyperlink
- **Sorting header progress**: Combined "sorted" and "remaining" into a single progress indicator (`86 / 2086 sorted`)
- **Multithreaded file sorting**: `sortFiles()` now runs each file move in its own thread using `QtConcurrent` + `QFutureSynchronizer`, with mutex-protected name generation to prevent duplicate filenames
- **Comma-formatted numbers**: Large file counts in the report screen and sorting header now display with thousand separators (e.g. `15,634`)

### Fixed
- **Duplicate finder visual comparison**: grayscale pixel comparisons now use actual pixel colors instead of palette indices, which improves similarity scoring for real images
- **Duplicate finder scanning stability**: progress updates now keep the UI responsive during long scans, and the hash cache version was bumped so stale cache entries do not mix with the new thumbnail data
- **AddPersonDialog no longer shown unnecessarily**: For Curator and IrlOnly accounts, the dialog is now skipped when the model name already exists in the database and there's no account to link
- **Curator/IrlOnly model name separation**: The name field for Curator and IrlOnly accounts now correctly represents the MODEL in the photos (per-batch), not the account owner (photographer). After adding a curator, the text field clears so the user can enter the model's name
- **Curator account type persistence**: `handleAddUnknownAccount` now uses `dialogType` (what the user selected in the Add Person dialog) for the group's account type, ensuring Curator sorting uses the text field for model name
- **Cancel prevents sorting on Curator**: Clicking Cancel/No on the "Add Curator Account" confirmation now properly aborts sorting instead of proceeding
- **Cancel aborts on unknown personal**: Clicking Cancel in the Add Person dialog for unknown accounts now clears the text field and aborts sorting instead of silently proceeding with the entered name
- **Add Person button reset on Cancel**: Cancelling the Add Person dialog now resets the UI back to its original state so the button stays usable
- **Clear Curator account confirmation**: When the model exists but the curator account doesn't, a clear message distinguishes "source account (photographer)" from "model (person in photos)"
- **Button text preservation**: The "Add Person" button for Curator accounts no longer changes to "Sort", allowing users to add new models to the database during sorting
- **Cache invalidation on DB changes**: Grouping cache is invalidated whenever accounts are added to the database, ensuring subsequent batches correctly recognize known accounts
- **Select All button toggle**: Fixed Deselect All not working — button now correctly reads current selection state and emits the appropriate signal
- **Equal button widths**: Skip Batch, Select All, and Delete Selected buttons now have fixed equal widths for proper centering
- **Account name label centering**: Curator/IrlOnly account label is now centered on the full screen width using QGridLayout, independent of the Open Instagram button
- **Removed files selected counter**: The "X files selected" label has been removed from the sorting UI
- **Duplicate finder scan flow**: The duplicate finder now streams matches into the UI while scanning, updates progress in two phases, and preserves already-visible selections while new groups arrive
- **Output-folder validation**: Missing or duplicate output directories are filtered out before the duplicate finder opens
- **Sort-to-folder file removal**: The sorting screen now only removes thumbnails whose source file actually disappeared after a move, which avoids stale UI state when a file move fails

### Changed
- **Duplicate finder compare path**: after a perceptual-hash match, the scanner now performs a fast in-memory 16x16 grayscale similarity check before confirming a duplicate group
- **Duplicate finder list ordering**: already-sorted filenames are parsed up front so files with different person names are not compared against one another unnecessarily
- **Targeted cache updates**: Instead of invalidating the entire grouping cache when a new account is added, only the affected groups are updated in-place (O(n) where n = number of groups)
- **Curator/IrlOnly name handling**: For these account types, `irlName` is no longer persisted to the group state or UI. The text field is cleared after each batch to allow entering a different model name
- **FileGrouper optimization**: For Curator and IrlOnly accounts found in the database, `irlName` is no longer set since the account is the photographer, not the model
- **Completer per-account entries**: The name completer now shows one entry per account instead of deduplicating by IRL name, enabling search by account handle (including substrings)
- **Sorting header layout**: Removed account name from the header; now shows just `Batch X of Y • Z / N sorted`
- **AddPersonDialog defaults**: Account type dropdown now always defaults to Personal based on the sort panel's dropdown selection

### Added — Qt C++ Desktop App (`IG_Sorter_Qt/`)
- Modern Qt 6 C++ GUI replacing the CLI `Instagram_Sorter.py`
- Multi-screen flow: Menu → Cleanup → Sorting (batch preview) → Report
- **Settings dialog**: Source folder, output folders (with duplicate detection and reorder via Move Up/Down buttons), database file, batch size, theme
- **Clickable source folder link** on the startup menu to open in file explorer
- **Directory cleanup phase**: Sequential file renumbering with safe two-phase rename, unknown name detection with inline resolution UI
- **Batch image preview**: Aspect-ratio-aware grid layout, resizable thumbnails with selection, `KeepAspectRatio` rendering (no cropping), per-file filename labels
- **Sort panel**: Dynamic output folder buttons at bottom, curator identification per-batch, IRL name autocomplete from database with Tab completion, validation requiring name entry before sorting
- **Dynamic favorites**: Frequently-sorted names appear as quick-fill buttons above sort-to-folder buttons, auto-scaling to fit available screen width with overflow trimming
- **Add Person dialog**: Prompts for optional Instagram account when adding a new person, cursor pre-focused on the account field, button text reads "Add Person"
- **Add Person dialog UI tweaks**: Button text changed from "Add & Sort" to "Add", cursor now starts on the Instagram Account field instead of Name
- **Input widget permanence rule**: The IRL name text entry field MUST NEVER be hidden or replaced by static text. It stays visible at all times during sorting so users can always enter new names without interruption.
- **Sub-batch name reset**: Unknown personal accounts reset their name field after each sub-batch so mixed-people batches can be sorted correctly with different names
- **Progress tracking**: Determinate progress bars during cleanup with file count labels per directory
- **Log cycling**: Rotating log files (max 5, 5 MB each) at `AppData/IG_Sorter/`, logging all file moves, deletes, renames, skips, and errors
- **Configuration persistence**: `QSettings` (INI format) with immediate `sync()` on every write
- `DatabaseManager`: JSON-based account database with Personal/Curator/IrlOnly types, `hasIrlName()` lookup, CRUD operations
- `FileNameParser`: Regex-based filename parsing supporting Instaloader, Facebook (`FB_IMG_<unix_ms>`), TikTok slideshows, and unknown source types
- `FileGrouper`: Groups files by account + post timestamp with database lookup and progress signals
- `DirectoryCleanup`: Safe two-phase file renumbering to fix numbering gaps
- `SorterEngine`: Orchestrates extension fixing → cleanup → grouping → sorting pipeline, async file grouping to prevent UI freezes
- `FileUtils`: Safe copy-then-rename file move with no-overwrite guarantee
- `ExtensionFixer`: Detects and corrects mismatched file extensions using magic byte inspection
- `WebpDecoder`: Windows WIC-based WebP decoder fallback for thumbnail display
- `ThemeManager`: Light/Dark/System theme support via QSS stylesheets
- `LogManager`: Centralized logging with rotation, file operation tracking, and console mirroring
- `ConfigManager`: QSettings-based persistence with output folder management
- FlowLayout utility for flexible widget layouts

### Changed
- Database format migrated from `.xlsx` + `.txt` to unified `ig_people.json`
- Output folders now stored in `QSettings` instead of Python config files
- Unknown account resolution: clicking "Add Person" now prompts for Instagram account via dialog instead of silently adding with null account
- File grouping runs asynchronously on background thread to prevent UI freeze during "Continue to Sorting"
- Favorites cap removed — all names with usage counts are shown, with overflow buttons trimmed to fit available width

### Added
- `Instagram_Sorter.py`: Automated sorting, renaming, and error-handling script for newly downloaded images.
- `Find_Duplicate_Files.py`: Utility script to scan SFW, MSFW, and NSFW directories for duplicate files based on file size.
- `ig_people.json` integration for mapping Instagram handles to real names.
- `photographers.txt` integration to skip/handle specific known photographer accounts.
- `run_IG_Sorter.bat`: Windows batch file for easy execution of the main sorting script.
- Automatic numbering verification and correction for existing sorted files.
- Interactive console prompts for handling unknown accounts and file types.



