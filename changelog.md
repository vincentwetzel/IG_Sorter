# Changelog

All notable changes to the IG_Sorter project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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
- `instaloader_downloader.py`: Firefox cookie extraction tool to resolve Instaloader login and authorization issues.
- `ig_people.json` integration for mapping Instagram handles to real names.
- `photographers.txt` integration to skip/handle specific known photographer accounts.
- `run_IG_Sorter.bat`: Windows batch file for easy execution of the main sorting script.
- Automatic numbering verification and correction for existing sorted files.
- Interactive console prompts for handling unknown accounts and file types.
