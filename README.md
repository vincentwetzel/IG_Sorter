# IG_Sorter

IG_Sorter is a set of Python scripts designed to manage, sort, and rename image files downloaded from Instagram or other sources, keeping them organized into specific folders.

## Features

- **Instagram_Sorter.py**: The main script. Sorts and renames newly downloaded image files (from Instagram, screenshots, Twitter, etc.) into appropriate folders based on the content creator's real name or Instagram handle, mapped through an Excel database (`ig_people.xlsx`). It also checks and fixes file numbering for existing files.
- **Find_Duplicate_Files.py**: Searches specific image directories (`SFW`, `MSFW`, `NSFW`) for duplicate files based on file size and similar beginnings of file names.
- **instaloader_downloader.py**: A script to download posts that includes utility functionality to import Instagram session cookies directly from Firefox, bypassing potential `Instaloader` login and authorization issues.

## Project Structure

- `Instagram_Sorter.py`: Main sorting logic.
- `Find_Duplicate_Files.py`: Duplicate detection.
- `instaloader_downloader.py`: Downloading script with Firefox session cookie importer for Instaloader.
- `ig_people.xlsx` (Data file - not included by default): An Excel spreadsheet mapping Instagram accounts to real names. Requires columns `Account` and `Name`.
- `photographers.txt`: A text file containing a list of known photographer Instagram accounts.
- `run_IG_Sorter.bat`: A batch file to run the main sorting script.

## Dependencies

- Python 3.x
- `pandas`
- `openpyxl` (needed by pandas to read/write Excel files)
- `instaloader` (for `instaloader_downloader.py`)

To install dependencies:
```bash
pip install pandas openpyxl instaloader
```

## Setup & Usage

1. **Directories Configuration**: In `Instagram_Sorter.py`, ensure `ROOT_PICTURE_DIR` and `pic_directories_dict` are set to match your local file structure.
2. **Database Setup**: You need an `ig_people.xlsx` file in the same directory as `Instagram_Sorter.py` with headers `Account` and `Name`.
3. **Run the Sorter**: Execute the `run_IG_Sorter.bat` file or run `python Instagram_Sorter.py` from your terminal.

## Note

These scripts are hardcoded for a specific Google Drive directory structure (`I:/Google Drive (...)`). You will need to modify the paths in the scripts (`Instagram_Sorter.py` and `Find_Duplicate_Files.py`) to match your own environment before running them.