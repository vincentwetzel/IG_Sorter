# Agents & Automated Scripts

In the context of the **IG_Sorter** project, "agents" refer to the automated Python scripts that perform specialized tasks within the image sorting pipeline.

## 1. The Sorter Agent (`Instagram_Sorter.py`)
This is the primary agent responsible for the following tasks:
- **File Discovery**: Scans the "NEED TO SORT" directories.
- **Identity Resolution**: Analyzes file names using regular expressions to determine the source of the image (Instagram, Twitter, Screenshot, etc.) and extracts the user's handle.
- **Database Lookup**: Consults the `ig_people.xlsx` database and `photographers.txt` to map Instagram handles to real names.
- **File Operations**: Renames files to a standardized format (`Real Name X.ext`) and moves them to the appropriate destination directory (`SFW`, `MSFW`, `NSFW`).
- **Interactive Error Handling**: When an unknown user is encountered, the agent prompts the user via the console, opening web browsers and file explorers to facilitate manual identification.

## 2. The Duplicate Finder Agent (`Find_Duplicate_Files.py`)
This agent focuses on maintaining storage efficiency:
- **Byte-size Matching**: Scans destination directories to find files with the exact same file size in bytes.
- **Name Verification**: Further verifies potential duplicates by checking if the first few characters of the file names match.
- **Reporting**: Outputs a list of potential duplicates to the console for manual review and deletion.

## 3. The Downloader Agent (`instaloader_downloader.py`)
A script designed to download posts and bypass login restrictions:
- **Version Check**: Automatically verifies Instaloader is up to date at startup and attempts auto-upgrade if outdated.
- **Cookie Extraction**: If standard login fails, reads the local `cookies.sqlite` database from the user's Firefox profile.
- **Session Injection**: Injects the extracted Instagram cookies into the `Instaloader` session to bypass 401 login restrictions.
- **Session Persistence**: Saves the authenticated session to a file for future use by other scripts or tools that rely on `Instaloader`.
- **Rate Limit Management**: Implements randomized delays (60-120s) between downloads and initial cooldown (15-60s) after login to avoid 429 errors.
- **Session Limits**: Prompts user for maximum post count per session to control API usage.
- **Downloading**: Uses `instaloader` to download saved posts from the authenticated account.

---

## Code Standards

All Python scripts in this project MUST follow these standards:

### PEP8 Compliance
- Follow PEP8 style guidelines for all Python code
- Maximum line length: 88 characters (compatible with Black formatter)
- Use 4 spaces for indentation (no tabs)
- Use meaningful variable and function names (snake_case for functions/variables, PascalCase for classes)

### Type Hints
- ALL function definitions MUST include type hints for parameters and return values
- Use `typing` module imports for complex types (`Optional`, `List`, `Dict`, `Tuple`, etc.)
- Example:
  ```python
  def get_cookiefile() -> Optional[str]:
      ...
  
  def import_session(cookiefile: str, instaloader_instance: instaloader.Instaloader) -> instaloader.Instaloader:
      ...
  ```

### Documentation
- All functions MUST have docstrings describing purpose, parameters, and return values
- Inline comments should explain *why* something is done, not *what* is done
