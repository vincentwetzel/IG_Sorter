# IG_Sorter

IG_Sorter is a set of Python scripts and a Qt C++ desktop application designed to manage, sort, and rename image files downloaded from Instagram, Facebook, Twitter, TikTok, and other sources, keeping them organized into specific folders.

## Features

### Qt C++ Desktop App (`IG_Sorter_Qt/`)
A modern cross-platform GUI application that replaces the CLI Python sorter. Features include:
- **Multi-screen workflow**: Menu → Directory Cleanup → Batch Sorting → Report
- **Dynamic favorites**: Frequently-sorted names appear as quick-fill buttons above the sort-to-folder buttons, auto-scaling to fit available width
- **Aspect-ratio-aware image preview grid** with multi-selection and batch assignment
- **WebP preview** via Windows WIC decoder fallback for thumbnail display
- **Settings dialog**: Source folder, output folders (with duplicate detection and reordering via Move Up/Down buttons), database file, batch size, and theme
- **Two-phase safe file renaming** to fix numbering gaps in output directories
- **JSON-based account database** with Personal, Curator, and IRL-Only entry types
- **Real-time progress tracking** during cleanup with per-directory file counts
- **Automatic extension fixing** — detects and corrects mismatched file extensions before sorting
- **Sub-batch name reset** — unknown accounts reset their name field after each sub-batch so mixed-people batches can be sorted correctly
- **Add Person dialog** — prompts for optional Instagram account when adding a new person, with cursor pre-focused on the account field

### Python Scripts
- **Instagram_Sorter.py**: The main script. Sorts and renames newly downloaded image files (from Instagram, screenshots, Twitter, Facebook, etc.) into appropriate folders based on the content creator's real name or Instagram handle, mapped through a JSON database (`ig_people.json`). It also checks and fixes file numbering for existing files.
- **Find_Duplicate_Files.py**: Searches specific image directories (`SFW`, `MSFW`, `NSFW`) for duplicate files based on file size and similar beginnings of file names.
- **instaloader_downloader.py**: A script to download posts that includes utility functionality to import Instagram session cookies directly from Firefox, bypassing potential `Instaloader` login and authorization issues.

## Project Structure

- `IG_Sorter_Qt/`: Qt 6 C++ desktop application (CMake-based)
- `Instagram_Sorter.py`: Main sorting logic (Python CLI).
- `Find_Duplicate_Files.py`: Duplicate detection.
- `instaloader_downloader.py`: Downloading script with Firefox session cookie importer for Instaloader.
- `private-data/ig_people.json` (Data file): A JSON file mapping Instagram accounts to real names with entry types (Personal, Curator, IRL-Only).
- `run_IG_Sorter.bat`: A batch file to run the main sorting script.

## Dependencies

### Qt C++ App
- Qt 6.5+ (Core, Widgets, Gui, Concurrent, Network)
- CMake 3.16+
- C++17 compiler (MSVC, GCC, or Clang)

### Python Scripts
- Python 3.x
- `pandas`
- `openpyxl` (needed by pandas to read/write Excel files)
- `instaloader` (for `instaloader_downloader.py`)

To install Python dependencies:
```bash
pip install pandas openpyxl instaloader
```

## Setup & Usage

### Qt C++ App
1. Build `IG_Sorter_Qt/` using CMake and your preferred Qt 6 toolchain.
2. Launch the app and open **Settings** to configure:
   - Source folder (directory with unsorted images)
   - Output folders (SFW, MSFW, NSFW, etc.)
   - Database file path (`ig_people.json`)
3. Click **Start Sorting** to begin the cleanup → sorting pipeline.

### Python Scripts
1. **Directories Configuration**: In `Instagram_Sorter.py`, ensure `ROOT_PICTURE_DIR` and `pic_directories_dict` are set to match your local file structure.
2. **Database Setup**: Place `ig_people.json` in the project root directory.
3. **Run the Sorter**: Execute `run_IG_Sorter.bat` or run `python Instagram_Sorter.py` from your terminal.

## Note

These scripts are hardcoded for a specific Google Drive directory structure (`I:/Google Drive (...)`). You will need to modify the paths in the scripts (`Instagram_Sorter.py` and `Find_Duplicate_Files.py`) to match your own environment before running them.
