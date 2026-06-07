# Agents & Automated Tools

In the context of the **IG_Sorter** project, "agents" refer to automated tools, scripts, and coding assistants that operate on the image sorting pipeline.

---

## CRITICAL RULE: NO AUTOMATIC GIT OPERATIONS

**AI agents, coding assistants, and automated scripts MUST NEVER run `git commit`, `git push`, `git reset`, `git revert`, or any other git command that modifies repository history or remote state WITHOUT the user's explicit, direct instruction.**

This includes but is not limited to:
- `git commit`
- `git push`
- `git reset`
- `git revert`
- `git amend`
- `git force-push`

**Permitted git operations** (only when helpful):
- `git status` - to check state
- `git diff` - to review changes
- `git log` - to check history
- `git add` - to stage files (only if user requests it)

**When the user says "git push" or "commit and push"**, then and only then may the agent execute those commands.

Violating this rule is a serious breach of trust and must never happen.

---

## 1. The Sorting Application (`IG_Sorter_Qt/`)

This is the primary implementation responsible for the following tasks:
- **File Discovery**: Scans the configured source directory.
- **Identity Resolution**: Parses file names using regular expressions to determine source type and account handle.
- **Database Lookup**: Consults the configured `ig_people.json` database to map account handles to real names.
- **File Operations**: Renames files to a standardized format (`Real Name X.ext`) and moves them to configured destination directories.
- **Interactive Error Handling**: Prompts for unknown accounts in the UI and can open Instagram profiles in the default browser.

## 2. The Duplicate Finder (`IG_Sorter_Qt/src/core/DuplicateFinder.*`)

This tool focuses on maintaining storage efficiency:
- **Byte-size Matching**: Scans destination directories to find files with the exact same file size in bytes.
- **Name Verification**: Verifies potential duplicates by comparing normalized person names.
- **Visual Similarity**: Compares thumbnails to reduce false positives.
- **Review UI**: Presents potential duplicates in the Qt UI for manual review and deletion.

## 3. Private Data and Downloader Tools

Private data and downloader tools are intentionally kept outside this repository:
- `ig_people.json` is user-provided and selected in the app's Settings dialog.
- Instaloader downloader scripts, credentials, session files, download history, and downloaded media belong in a separate private project or local-only location.

---

## Qt C++ Application Standards (`IG_Sorter_Qt/`)

All C++ code in the Qt application MUST follow these standards:

### C++ Style
- Follow Qt coding style guidelines
- Use C++17 features where appropriate
- Use 4 spaces for indentation (no tabs)
- Use PascalCase for class names, camelCase for methods/variables, snake_case for member prefixes
- Use `#include` guards with `#pragma once` in headers

### Qt Conventions
- Use `QObject` parent-child ownership model for memory management
- Use signals/slots for inter-component communication
- Use `QFutureWatcher` + `QtConcurrent` for background tasks to avoid blocking the UI thread
- Use `QString` for all string operations (never `std::string` in UI code)
- Use `QList`, `QHash`, `QVector` for collections

### Architecture
- **Core classes** (`core/`): DatabaseManager, FileNameParser, FileGrouper, SorterEngine, DirectoryCleanup, DuplicateFinder
- **UI classes** (`ui/`): MainWindow, SortingScreen, SortPanel, ImagePreviewGrid, CleanupScreen, ReportScreen, SettingsDialog, DuplicateFinderScreen
- **Utility classes** (`utils/`): ConfigManager, LogManager, ThemeManager, FileUtils, WebpDecoder
- All UI classes use dependency injection where practical
- Long-running operations run on background threads with progress signals
