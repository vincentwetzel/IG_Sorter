# IG_Sorter

IG_Sorter is a Qt C++ desktop application for managing, sorting, and renaming image files downloaded from Instagram, Facebook, Twitter, TikTok, and other sources.

## Features

- **Multi-screen workflow**: Menu -> Directory Cleanup -> Batch Sorting -> Report
- **Dynamic favorites**: Frequently-sorted names appear as quick-fill buttons above the sort-to-folder buttons
- **Aspect-ratio-aware image preview grid** with multi-selection and batch assignment
- **WebP preview** via Windows WIC decoder fallback for thumbnail display
- **Settings dialog** for source folder, output folders, database file, batch size, and theme
- **Two-phase safe file renaming** to fix numbering gaps in output directories
- **Responsive preview grid and cleanup UI** with deferred relayouts, batch-aware selection tracking, and safer widget teardown
- **JSON-based account database** with Personal, Curator, and IRL-Only entry types
- **Real-time progress tracking** during cleanup with per-directory file counts
- **Automatic extension fixing** for mismatched image extensions
- **Asynchronous sorting actions** for file moves, recycle-bin deletes, and undo operations
- **Sub-batch name reset** so mixed-people batches can be sorted correctly
- **Add Person dialog** for adding unknown accounts and people during sorting
- **Duplicate Finder screen** with recursive output-folder scanning, hash-based image matching, fast grayscale thumbnail similarity checks, exact-byte fallback for non-images, live group streaming, cancel support, and Recycle Bin deletion
- **Safer sorting and cleanup flow** with empty-group skipping, batch-size clamping, incremental database index updates, and stricter out-of-range handling in the Qt screens

## Project Structure

- `IG_Sorter_Qt/`: Qt 6 C++ desktop application
- `IG_Sorter_Qt/src/core/`: Sorting, grouping, cleanup, duplicate detection, database, and file utility classes
- `IG_Sorter_Qt/src/ui/`: Main window and workflow screens
- `IG_Sorter_Qt/src/utils/`: Configuration, logging, theming, file helpers, and WebP decoder
- `IG_Sorter_Qt/resources/`: Qt resources and QSS theme files

## Dependencies

- Qt 6.5+ (Core, Widgets, Gui, Concurrent, Network)
- CMake 3.16+
- C++17 compiler (MSVC, GCC, or Clang)

## Setup & Usage

1. Build `IG_Sorter_Qt/` using CMake and your preferred Qt 6 toolchain.
2. Launch the app and open **Settings** to configure:
   - Source folder: directory with unsorted images
   - Output folders: SFW, MSFW, NSFW, or any other destinations
   - Database file path: your local `ig_people.json`
   - Batch size and theme
3. Click **Start Sorting** to begin the cleanup -> sorting pipeline.

## Private Data

This repository does not include private data, downloader scripts, credentials, download history, or downloaded media. Keep those in a separate private project or local-only location, then configure this app to use your `ig_people.json` through **Settings**.
