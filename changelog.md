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
- **Sort panel**: Dynamic output folder buttons (centered, with padding), curator identification per-batch, IRL name autocomplete from database with Tab completion, validation requiring name entry before sorting
- **New person dialog**: Prompts for optional Instagram account when entering an unknown name during curator sorting
- **Progress tracking**: Determinate progress bars during cleanup with file count labels per directory
- **Log cycling**: Rotating log files (max 5, 5 MB each) at `AppData/IG_Sorter/`, logging all file moves, deletes, renames, skips, and errors
- **Configuration persistence**: `QSettings` (INI format) with immediate `sync()` on every write
- `DatabaseManager`: JSON-based account database with Personal/Curator/IrlOnly types
- `FileNameParser`: Regex-based Instaloader filename parsing
- `FileGrouper`: Groups files by account + post timestamp with database lookup
- `DirectoryCleanup`: Safe two-phase file renumbering to fix numbering gaps
- `SorterEngine`: Orchestrates cleanup → grouping → sorting pipeline
- `FileUtils`: Safe copy-then-rename file move with no-overwrite guarantee
- `ThemeManager`: Light/Dark/System theme support via QSS stylesheets
- `LogManager`: Centralized logging with rotation, file operation tracking, and console mirroring
- FlowLayout utility for flexible widget layouts

### Changed
- Database format migrated from `.xlsx` + `.txt` to unified `ig_people.json`
- Output folders now stored in `QSettings` instead of Python config files

### Added
- `Instagram_Sorter.py`: Automated sorting, renaming, and error-handling script for newly downloaded images.
- `Find_Duplicate_Files.py`: Utility script to scan SFW, MSFW, and NSFW directories for duplicate files based on file size.
- `Instaloader_Login_Error_Fixer.py`: Firefox cookie extraction tool to resolve Instaloader login and authorization issues.
- `ig_people.xlsx` integration for mapping Instagram handles to real names using the pandas library.
- `photographers.txt` integration to skip/handle specific known photographer accounts.
- `run_IG_Sorter.bat`: Windows batch file for easy execution of the main sorting script.
- Automatic numbering verification and correction for existing sorted files.
- Interactive console prompts for handling unknown accounts and file types.