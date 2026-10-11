# IG_Sorter

IG_Sorter is a Qt 6 desktop application for reviewing, identifying, sorting, and renaming downloaded media. It supports Instagram-style downloads and several other filename formats, then lets the user assign image groups to configured output folders.

## What it does

- Cleans up numbering in configured output folders and flags names that are not in the selected people database.
- Groups source files by account and post, with support for Instagram downloads, Facebook and Twitter timestamps, TikTok slideshow IDs, and selected Android screenshots.
- Starts showing image previews in configurable batches as source files are discovered, so files can be assigned to a person and destination folder before the scan finishes.
- Supports Personal, Curator, and IRL-only database entries, unknown-account handling, and account cleanup.
- Recognizes Curator and IRL-only source accounts without requiring a stored name, and lets account cleanup save edited entries together.
- Runs output-directory cleanup concurrently and supports the mouse Back button across application screens.
- Includes a Duplicate Finder for reviewing likely matches in configured output folders, with user-confirmed Recycle Bin deletion and undo. The sorting pipeline also removes some exact-byte source duplicates automatically; see [duplicate handling](docs/ARCHITECTURE.md#duplicate-matching-and-file-removal).
- Stores preferences with `QSettings`; logs and the duplicate scan cache use the platform application-data location.

## Build

Requirements:

- CMake 3.16 or newer
- A C++17 compiler
- Qt 6 with Core, Widgets, Gui, and Concurrent components

Configure and build from the repository root:

```powershell
cmake -S IG_Sorter_Qt -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"
cmake --build build --config Release
```

Replace the Qt path with the kit installed on your machine. For a single-configuration generator, omit `--config Release` if desired. The executable name is `IG_Sorter_Qt` (with the platform-specific executable suffix).

## First run

1. Launch the application and open **Settings**.
2. Select the source folder that contains files to sort.
3. Add and order the output folders. Their labels become the sorting buttons.
4. Select your local `ig_people.json` database.
5. Set the preview batch size and theme, then save.
6. Start the sorting flow. Review directory cleanup, assign files in the preview screen, and inspect the report when the run completes.

The account database is a JSON array. Each record uses `name`, `account`, and `type`; `account` is `null` for IRL-only entries. A minimal example is:

```json
[
  { "name": "Example Person", "account": "example.account", "type": "personal" },
  { "name": "Example Model", "account": "feature.page", "type": "curator" },
  { "name": "Another Person", "account": null, "type": "irl_only" }
]
```

Treat the database as private user data. Keep it outside the repository and select it in Settings.

## Duplicate handling and file removal

The sorting pipeline checks exact-byte duplicates while preparing groups and while sorting. When it finds a duplicate, it removes the source file directly. This automatic path does not send the file to the Recycle Bin and is not covered by the sorting undo action. Review this behavior before pointing the app at valuable source files. The separate Duplicate Finder screen is for manual review; files deleted there go to the Recycle Bin and can be restored with its undo workflow.

## Repository layout

- `IG_Sorter_Qt/` — CMake project and Qt application source, resources, and themes.
- `docs/` — architecture, coding guidance, implementation status and roadmap, and changelog.
- `AGENTS.md` — repository-specific instructions for automated tools.

See [the documentation index](docs/README.md) for the guide list. The architecture and coding guides describe the current application structure and conventions.

## Private data

This repository intentionally excludes `ig_people.json`, downloader scripts, credentials, session files, download history, and downloaded media. Keep those in a separate private project or local-only location. Configure the database path in the application rather than copying private data into the repository.



