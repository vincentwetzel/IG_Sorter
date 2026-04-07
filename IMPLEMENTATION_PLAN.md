# IG_Sorter Qt C++ — Implementation Plan

## Overview

Cross-platform Qt C++ desktop application that replaces the CLI Python `Instagram_Sorter.py`
with a modern GUI. The app groups files by account/post, displays image previews in batches,
and lets users sort them with clickable destination-folder buttons. Unknown accounts can be
identified on the fly and saved to a JSON database.

---

## 1. Project Structure & Build System

### 1.1 Technology Stack
- **Build system:** CMake 3.16+
- **UI framework:** Qt 6 (modules: `Core`, `Widgets`, `Gui`, `Concurrent`, `Network`)
- **Language:** C++17
- **Database format:** JSON (`ig_people.json`) — replaces the old `.xlsx`
- **Third-party:** none required beyond Qt itself (JSON is handled via `QJsonDocument`)

### 1.2 Directory Layout

```
IG_Sorter_Qt/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── core/
│   │   ├── FileNameParser.h / .cpp          // Regex-based filename parsing
│   │   ├── FileGrouper.h / .cpp             // Groups files by account + post timestamp
│   │   ├── DatabaseManager.h / .cpp         // Reads/writes ig_people.json
│   │   ├── DirectoryCleanup.h / .cpp        // Fix numbering in output dirs
│   │   └── SorterEngine.h / .cpp            // Orchestrates the full pipeline
│   ├── models/
│   │   └── BatchModel.h / .cpp              // QAbstractListModel for image batches
│   ├── ui/
│   │   ├── MainWindow.h / .cpp              // QStackedWidget managing all screens
│   │   ├── MenuScreen.h / .cpp              // Start / Settings buttons
│   │   ├── CleanupScreen.h / .cpp           // Progress bar during directory cleanup
│   │   ├── SortingScreen.h / .cpp           // Main image-preview + sort UI
│   │   ├── ReportScreen.h / .cpp            // Final summary report
│   │   ├── SettingsDialog.h / .cpp          // Settings modal
│   │   ├── ImagePreviewGrid.h / .cpp        // Custom widget for thumbnail display
│   │   ├── ImageThumbnail.h / .cpp          // Single selectable thumbnail widget
│   │   └── SortPanel.h / .cpp               // Bottom panel: output buttons + IRL name
│   └── utils/
│       ├── ConfigManager.h / .cpp           // Persists app settings (QSettings)
│       └── FileUtils.h / .cpp               // Safe file operations (no-overwrite guarantees)
├── resources/
│   ├── icons/                               // SVG/PNG icons
│   ├── styles/                              // QSS theme files (light / dark)
│   └── resources.qrc
└── data/
    └── ig_people.json                       // Account-to-IRL-name database (user-provided)
```

---

## 2. Core Logic

### 2.1 FileNameParser

Responsible for extracting metadata from a filename using regular expressions.
Only **one** regex pattern is implemented initially; additional patterns will be
added later as new source types are required.

#### 2.1.1 Instaloader Pattern

**Example filenames:**
```
vincentwetzel/joeygore1_2026-03-13_15-30-00_1.jpg
vincentwetzel/joeygore1_2026-03-13_15-30-00_14.jpg
```

**Regex components:**

| Group | Pattern | Meaning | Example Match |
|-------|---------|---------|---------------|
| Account handle | `^.+?(?=_\d{4}-\d{2}-\d{2})` | Everything before `_YYYY-MM-DD` | `joeygore1` |
| Date | `(\d{4}-\d{2}-\d{2})` | Post date | `2026-03-13` |
| Time | `(\d{2}-\d{2}-\d{2})` | Post time | `15-30-00` |
| Sequence number | `_(\d+)$` | Image index within the post | `1`, `14` |

**Combined regex (C++ raw string):**
```cpp
R"((^.+?)(?=_\d{4}-\d{2}-\d{2})_(\d{4}-\d{2}-\d{2})_(\d{2}-\d{2}-\d{2})_(\d+))"
```

#### 2.1.2 Future Pattern Placeholders

These patterns are **not yet implemented** but the architecture reserves source-type
identifiers for them. They will be added to `FileNameParser` as additional regex
branches when required.

| Source Type | Example Filename | Notes |
|-------------|-----------------|-------|
| `screenshot_ipad` | `IMG_4536.jpg` | iPad screenshots — `^IMG_\d{4}\.` |
| `screenshot_edited` | `12345678_1234567890.jpg` | Edited Android screenshots |
| `twitter` | `IMG_12345678_123456.jpg` | Twitter downloads |
| `chrome_dl` | `name_12345678_123456789012345.jpg` | Chrome extension downloads |
| `fastsave` | `name___1234567890.jpg` | FastSave Android app |

Each future pattern will return the same `ParsedResult` struct with its own
`sourceType` string.

#### 2.1.3 ParsedResult struct

```cpp
struct ParsedResult {
    QString accountHandle;     // e.g. "joeygore1"
    QString postDate;          // e.g. "2026-03-13"
    QString postTime;          // e.g. "15-30-00"
    QString postTimestamp;     // Combined: "2026-03-13_15-30-00"
    int      sequenceNumber;   // e.g. 14
    QString  sourceType;       // e.g. "instaloader"
    bool     matched;          // true if regex matched
};
```

#### 2.1.3 Class interface

```cpp
class FileNameParser {
public:
    static ParsedResult parse(const QString& filename);
};
```

- Returns `ParsedResult{matched: false}` when no pattern matches.
- Source type `"instaloader"` is hard-coded for this pattern.
- New patterns will be added as additional `static` methods or a plugin-style registry.

---

### 2.2 DatabaseManager

Manages the unified JSON database that maps Instagram account handles to real
(IRL) names. This single database replaces **both** the old `ig_people.xlsx` and
`photographers.txt`.

#### 2.2.1 Account Types

Every entry in the database has a **type** that describes the relationship between
the account and the person named:

| Type | Meaning | Example |
|------|---------|---------|
| `"personal"` | The account belongs to the named person. | `joeygore1` → "Joey Gore" |
| `"curator"` | The account posts content **featuring** the named person but is not their own account. This covers photographers, feature accounts, brand/team accounts, and aggregate accounts. | `pnv.male_modelnetwork` → "Florian Macek" (photographer), `lakers` → "Kobe Bryant" (team account), `florianmacek` → "Florian Macek" (photographer's own account) |
| `"irl_only"` | No account handle — the person has no Instagram account, deleted it, or is known only by name. Used for validation during directory cleanup. | `null` → "Vincent Wetzel" |

**Why "curator" instead of "photographer"?**
The old Python script used "photographer" as a catch-all for any account that wasn't
a personal account. This was misleading — many such accounts are feature pages,
agencies, or brand accounts. "Curator" accurately describes any account that
collects and posts photos of people other than the account holder.

#### 2.2.2 JSON Format

```json
[
  { "account": "joeygore1",           "name": "Joey Gore",       "type": "personal" },
  { "account": "taylor_farr__",       "name": "Taylor Farr",     "type": "personal" },
  { "account": "pnv.male_modelnetwork", "name": "Florian Macek", "type": "curator" },
  { "account": "florianmacek",        "name": "Florian Macek",   "type": "personal" },
  { "account": "florianmacekbackup",  "name": "Florian Macek",   "type": "personal" },
  { "account": null,                  "name": "Vincent Wetzel",  "type": "irl_only" }
]
```

**Rules:**
- `"account"` may be `null` only when `"type"` is `"irl_only"`. For `"personal"`
  and `"curator"` entries, `"account"` must be a non-empty string.
- `"name"` must always be a non-empty string.
- `"account"` values are case-insensitive; stored lowercase internally.
- **No duplicate `"account"` values** — each Instagram handle appears at most once.
- **The same `"name"` may appear multiple times** with different accounts. For
  example, Florian Macek has entries for both `florianmacek` (personal) and
  `pnv.male_modelnetwork` (curator). This is fully supported.
- The old `photographers.txt` list is **migrated into this database** as entries
  with `"type": "curator"`. The text file is no longer used.

#### 2.2.3 Class Interface

```cpp
enum class AccountType {
    Personal,   // Account belongs to the named person
    Curator,    // Account posts content featuring the named person
    IrlOnly     // No account — name-only entry for validation
};

struct PersonEntry {
    QString     account;    // empty QString when null (irl_only entries)
    QString     irlName;
    AccountType type;
};

class DatabaseManager : public QObject {
    Q_OBJECT
public:
    explicit DatabaseManager(const QString& dbPath, QObject* parent = nullptr);

    bool        load();                          // Read from disk
    bool        save();                          // Write to disk
    bool        hasAccount(const QString& handle) const;
    QString     getIrlName(const QString& handle) const;   // empty if not found
    PersonEntry getEntry(const QString& handle) const;
    QList<PersonEntry> allEntries() const;

    // Add a new account-to-name mapping (e.g. user identifies an unknown account)
    bool        addEntry(const QString& account, const QString& irlName,
                         AccountType type);

    // Remove an account mapping
    bool        removeEntry(const QString& account);

    // Update an existing entry
    bool        updateEntry(const QString& oldAccount, const QString& newAccount,
                            const QString& newName, AccountType newType);

    // Check if an IRL name exists anywhere in the database (any type)
    bool        hasIrlName(const QString& name) const;

signals:
    void databaseChanged();

private:
    QString                m_dbPath;
    QList<PersonEntry>     m_entries;
    QHash<QString, int>    m_accountIndex;  // lowercase account -> index in m_entries
};
```

#### 2.2.4 IRL-only lookups

During **directory cleanup**, files are already named like `"Joey Gore 5.jpg"`.
The cleanup routine validates that the extracted name exists somewhere in the
database by scanning `m_entries` for a matching `irlName` (regardless of type
or whether the account is null). This ensures that files named after people
without Instagram accounts are still validated correctly.

#### 2.2.5 Migrating from the old Python data files

A one-time migration script (or built-in migration routine) will:
1. Read `ig_people.xlsx` → create entries with `"type": "personal"` (or
   `"irl_only"` if account column is blank).
2. Read `photographers.txt` → create entries with `"type": "curator"` and
   `"account"` set to each handle. IRL names for curator accounts will need
   manual assignment (the user will be prompted, or entries will be created
   with a placeholder name to be resolved later).
3. Deduplicate by account handle.
4. Write `ig_people.json`.

---

### 2.3 FileGrouper

Scans the source directory and groups files by account + post timestamp.

#### 2.3.1 Data Structures

```cpp
struct FileGroup {
    QString   accountHandle;       // e.g. "joeygore1", "pnv.male_modelnetwork"
    QString   irlName;             // resolved from DB; empty if unknown
    QString   postTimestamp;       // e.g. "2026-03-13_15-30-00"
    QStringList filePaths;         // full paths
    bool      isKnown;             // true if account exists in DB
    AccountType accountType;       // Personal, Curator, or IrlOnly
};
```

#### 2.3.2 Grouping Algorithm

1. Recursively list all files in the source directory.
2. For each file, run `FileNameParser::parse()`.
3. If matched:
   - Key = `{accountHandle} ||| {postTimestamp}`
   - Append file path to that key's group.
4. If not matched:
   - Place into a special `"Unknown File Type"` group.
5. After grouping, resolve `irlName` for each group via `DatabaseManager`.
6. Set `isKnown` accordingly.

#### 2.3.3 Class Interface

```cpp
class FileGrouper : public QObject {
    Q_OBJECT
public:
    explicit FileGrouper(DatabaseManager* db, QObject* parent = nullptr);

    // Blocking call — run in a worker thread
    QList<FileGroup> group(const QString& sourceDir);

signals:
    void progressChanged(int current, int total);
};
```

---

### 2.4 DirectoryCleanup

Runs on **every** configured output directory **before** sorting begins.
This is a **mandatory** step — the user cannot proceed to sorting until cleanup
completes on all output directories.

#### 2.4.1 Purpose

Ensure that files in each output directory are numbered sequentially with no gaps.

**Example before:**
```
Bo Develius 1.jpg
Bo Develius 2.png
Bo Develius 3.webm
Bo Develius 5.jpg   <-- gap: #4 missing
Bo Develius 6.png   <-- #5 missing
```

**Example after:**
```
Bo Develius 1.jpg
Bo Develius 2.png
Bo Develius 3.webm
Bo Develius 4.jpg   <-- was 5
Bo Develius 5.png   <-- was 6
```

#### 2.4.2 Algorithm

1. For each output directory:
   a. List all files (skip `Thumbs.db`, `desktop.ini`).
   b. Sort files using natural sort order.
   c. For each file, extract `(personName, currentNumber, extension)` via regex:
      - Name: `^(.+?)\s+(\d+)$` → group 1
      - Number: group 2
   d. Validate that `personName` exists in the database (either as an account
      or as an IRL-only entry). If not, **halt and report** — the user must
      resolve this via the database manager before continuing.
   e. Walk through files grouped by `personName`, tracking expected next number.
   f. If a gap is detected, rename using a **two-phase safe approach**:
      - **Phase 1 (shift to temp names):** For every file that needs renumbering,
        rename it to a temporary unique name:
        `{personName}.__TMP_{uuid}.{ext}`
      - **Phase 2 (shift to final names):** Rename each temp file to its correct
        sequential number: `{personName} {expected}.{ext}`
      - This guarantees **zero overwrites** because no temp name can collide with
        any real file name.

#### 2.4.3 Handling Unknown People

If a file in an output directory has a name that does **not** correspond to any
entry in the database (neither account nor IRL-only), cleanup **cannot proceed**
for that file. The app will:

1. Collect all unknown names found across all output directories.
2. Present them to the user on the CleanupScreen.
3. Provide an inline text-entry for each unknown name so the user can:
   - **Add to database** as a new IRL-only entry (no account needed).
   - **Link to an existing account** if the user recognizes it.
4. The user must resolve **all** unknown names before the "Continue" button
   becomes enabled.

This approach ensures the database stays authoritative and no orphaned files
accumulate silently.

#### 2.4.4 Class Interface

```cpp
struct CleanupIssue {
    QString directory;       // output dir path
    QString personName;      // name found on files that isn't in DB
    QStringList filePaths;   // affected files
};

struct CleanupReport {
    int totalDirectoriesScanned = 0;
    int totalFilesRenamed = 0;
    QList<CleanupIssue> unresolvedIssues;
};

class DirectoryCleanup : public QObject {
    Q_OBJECT
public:
    explicit DirectoryCleanup(DatabaseManager* db, QObject* parent = nullptr);

    // Blocking call — run asynchronously via QtConcurrent
    CleanupReport run(const QStringList& outputDirs);

signals:
    void directoryProgress(const QString& dirPath, int current, int total);
    void fileRenamed(const QString& oldPath, const QString& newPath);

private:
    DatabaseManager* m_db;
};
```

---

### 2.5 SorterEngine

Orchestrates the complete pipeline.

#### 2.5.1 Pipeline Steps

```
1. Load database              (DatabaseManager::load)
2. Validate config            (source dir exists, output dirs exist)
3. Directory cleanup          (DirectoryCleanup::run on all output dirs)
                              → MUST complete before step 4
4. File grouping              (FileGrouper::group on source dir)
5. Present batches to user    (UI drives this via SortingScreen)
6. For each batch + selection:
      a. Determine target output dir
      b. Determine next available number for each IRL name in that dir
      c. Rename files safely (two-phase temp-name approach)
      d. Move files to target dir
      e. Verify destination file exists + size matches before deleting source
7. Generate final report
```

#### 2.5.2 Class Interface

```cpp
struct SortResult {
    int filesSorted = 0;
    int filesSkipped = 0;
    int errors = 0;
    QStringList errorMessages;
};

class SorterEngine : public QObject {
    Q_OBJECT
public:
    explicit SorterEngine(DatabaseManager* db, QObject* parent = nullptr);

    bool        initialize(const QString& sourceDir,
                           const QStringList& outputDirs);
    CleanupReport runCleanup();
    QList<FileGroup> groupFiles();

    // Called by UI when user assigns a selection of files to an output dir
    SortResult  sortFiles(const QStringList& filePaths,
                          const QString& accountHandle,
                          const QString& irlName,
                          AccountType accountType,
                          const QString& outputDir);

    const QString& sourceDir() const;
    const QStringList& outputDirs() const;

signals:
    void cleanupProgress(const QString& dir, int current, int total);
    void groupProgress(int current, int total);

private:
    DatabaseManager* m_db;
    QString          m_sourceDir;
    QStringList      m_outputDirs;
    DirectoryCleanup m_cleanup;
    FileGrouper      m_grouper;
};
```

---

### 2.6 Safe File Operations (FileUtils)

**Critical guarantee: no file is ever overwritten, lost, or deleted unintentionally.**

#### 2.6.1 Two-Phase Rename + Move

```cpp
class FileUtils {
public:
    // Returns the final destination path, or empty string on failure
    static QString safeMove(const QString& sourcePath,
                            const QString& destDir,
                            const QString& destFileName);

    // Check if a name already exists in the directory; return next available
    static QString nextAvailableName(const QString& dir,
                                     const QString& baseName,
                                     const QString& ext);
};
```

**Algorithm for `safeMove`:**
1. Compute final destination path: `destDir / destFileName`
2. If a file already exists at that path, increment a counter until a unique name is found.
3. **Copy** the source file to a temporary name in the destination dir:
   `destDir / {destFileName}.part.{uuid}`
4. Verify the copy succeeded (file exists, byte size matches).
5. **Rename** the `.part` temp file to the final name.
6. **Delete** the source file only after steps 3–5 succeed.
7. If any step fails, log the error, leave the source file untouched, and
   remove any partial destination files.

---

## 3. UI Design — Multi-Screen Flow

The application uses a `QStackedWidget` as the central widget of `MainWindow`.
Each "screen" is a separate widget pushed onto the stack.

### 3.1 Screen Flow

```
┌─────────────────┐
│   MenuScreen    │   ← "Start Sorting"  |  "Settings"
└────────┬────────┘
         │  [Start Sorting]
         ▼
┌─────────────────┐
│ CleanupScreen   │   ← Progress bars for each output dir (async)
│                 │   ← If unknown names found: inline resolve UI
│                 │   ← "Continue" button (disabled until cleanup + resolves done)
└────────┬────────┘
         │  [Continue]
         ▼
┌─────────────────┐
│ SortingScreen   │   ← Main batch-sorting UI (see §4)
│                 │   ← "Back" / "Finish" buttons
└────────┬────────┘
         │  [Finish]  (or when all batches are done)
         ▼
┌─────────────────┐
│  ReportScreen   │   ← Summary: files sorted, skipped, errors
│                 │   ← Final state of each output dir
│                 │   ← "Start Over" / "Settings" buttons
└─────────────────┘
```

### 3.2 SettingsDialog

Modal dialog accessible from MenuScreen or via a gear icon on any screen.

**Sections:**
| Setting | Control | Description |
|---------|---------|-------------|
| Source folder | `QLineEdit` + browse button | Single directory containing unsorted images |
| Output folders | `QTableWidget` with Add/Remove | Each row: display name (button label) + folder path |
| Database file | `QLineEdit` + browse button | Path to `ig_people.json` |
| Batch size | `QSpinBox` (1–20, default 5) | Number of thumbnails per sub-batch |
| Theme | `QComboBox`: Light / Dark / System | Global theme (see §5) |

**Persistence:** `QSettings` writes to platform-appropriate config location.

---

## 4. SortingScreen — Main Batch UI

This is the primary user interaction surface.

### 4.1 Layout

```
┌──────────────────────────────────────────────────────┐
│  Header: "Batch 2 of 3  •  moritz_hau  •  9 files"   │
│  (curator accounts show a badge: "moritz_hau [C]")   │
├──────────────────────────────────────────────────────┤
│                                                      │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐       │
│  │ IMG  │ │ IMG  │ │ IMG  │ │ IMG  │ │ IMG  │       │
│  │  #1  │ │  #2  │ │  #3  │ │  #4  │ │  #5  │       │
│  └──────┘ └──────┘ └──────┘ └──────┘ └──────┘       │
│  (clickable thumbnails with selection highlights)    │
│                                                      │
├──────────────────────────────────────────────────────┤
│  [NSFW (3)]    [MSFW (0)]    [SFW (0)]    [Skip]     │
│  (dynamic buttons, one per output folder;            │
│   number in parens = currently selected thumbnails)  │
├──────────────────────────────────────────────────────┤
│  IRL Name:  Moritz Hau                                │
│  ┌─────────────────────────────────────────────────┐  │
│  │ [Open Instagram]  (only shown if account unknown)│ │
│  └─────────────────────────────────────────────────┘  │
├──────────────────────────────────────────────────────┤
│  Unknown account? Enter name: [____________] [Add]   │
│  (only shown if account not in database)             │
└──────────────────────────────────────────────────────┘
```

### 4.2 Thumbnail Selection & Sorting

- Each thumbnail is an `ImageThumbnail` widget (clickable `QLabel` subclass).
- Clicking a thumbnail toggles its **selected** state (visible border highlight).
- The user selects one or more thumbnails, then clicks an output-folder button.
- The selected files are moved to that folder and removed from the current batch.
- Remaining thumbnails stay on screen for the next assignment.
- When all thumbnails from the current sub-batch are assigned, the next
  sub-batch loads automatically.
- When all sub-batches and all groups are done, the "Finish" button is enabled.

### 4.3 Batch Splitting

If a `FileGroup` has more files than `settings.batchSize`:
- Split into sub-batches of `batchSize` images.
- Each sub-batch is presented sequentially.
- All sub-batches share the same account/IRL name header.
- The header shows "Batch 2 of 3" etc.

### 4.4 Unknown Account Handling

When `FileGroup::isKnown == false`:
- The IRL name area shows **"Unknown account: `taylor_farr__`"**
- An **[Open Instagram]** button appears (opens `https://www.instagram.com/taylor_farr__/` in the default browser).
- A text input field + type selector + **[Add]** button appears at the bottom:

```
Unknown account?  Name: [Taylor Farr________]  Type: [Personal ▼]  [Add]
```

  - **Name field:** User types the IRL name (e.g., "Taylor Farr").
  - **Type selector:** A dropdown with options `Personal` | `Curator` | `IRL Only`.
    - `Personal` — the account belongs to this person (default).
    - `Curator` — this is a photographer, feature account, brand, etc. that
      posts content featuring the named person.
    - `IRL Only` — no account; name-only entry (rarely chosen here; mostly for
      cleanup of existing output directories).
  - Clicking **Add** calls `DatabaseManager::addEntry()` with the selected type
    and immediately resolves the name for the current batch.
- Once the account is known, the header updates to show the resolved IRL name
  and the unknown-account UI elements disappear.

---

## 5. Theming — Light / Dark / System

### 5.1 Architecture

- A single `ThemeManager` singleton owns the current theme setting.
- Themes are defined as QSS (Qt Style Sheets) files:
  - `resources/styles/light.qss`
  - `resources/styles/dark.qss`
- `QApplication::setStyleSheet()` is called once when the theme changes.
- **No individual widget is allowed to override the global stylesheet.**
- System theme detection on startup uses `QStyleHints::colorScheme()` (Qt 6.5+)
  or falls back to platform-specific checks.

### 5.2 ThemeManager Interface

```cpp
enum class Theme { Light, Dark, System };

class ThemeManager : public QObject {
    Q_OBJECT
public:
    static ThemeManager* instance();

    Theme currentTheme() const;
    void  setTheme(Theme t);
    void  applyTheme();          // Called on startup and when theme changes

signals:
    void themeChanged(Theme t);

private:
    Theme m_theme = Theme::System;
};
```

### 5.3 QSS Variables

Qt Style Sheets do not support CSS custom properties natively, so each theme
QSS file defines all colors explicitly. Common tokens:

| Token | Light | Dark |
|-------|-------|------|
| Background | `#FFFFFF` | `#1E1E1E` |
| Foreground | `#000000` | `#E0E0E0` |
| Primary | `#0078D4` | `#4CC2FF` |
| Surface | `#F3F3F3` | `#2D2D2D` |
| Border | `#E0E0E0` | `#404040` |
| Selection | `#0078D4` (white text) | `#4CC2FF` (black text) |
| Success | `#107C10` | `#6CCB5F` |
| Warning | `#FFB900` | `#FFD60A` |

---

## 6. Asynchronous Directory Cleanup

### 6.1 Implementation

Each output directory is cleaned in its own thread using `QtConcurrent::run()`.

```cpp
QVector<QFuture<CleanupReport>> futures;
for (const auto& dir : outputDirs) {
    futures.append(QtConcurrent::run([this, dir]() {
        return cleanupSingleDirectory(dir);
    }));
}

// Wait for all and aggregate results
QFutureWatcher<CleanupReport> watcher;
QSignalSpy spy(&watcher, &QFutureWatcher<CleanupReport>::finished);
watcher.setFuture(...);  // or use QFutureSynchronizer
```

### 6.2 UI Feedback

- The `CleanupScreen` shows one progress indicator per output directory.
- Each indicator updates via signals emitted from the worker thread.
- A combined progress summary shows overall completion.
- The "Continue" button is disabled until **all** futures complete.

### 6.3 Thread Safety

- Each worker operates on a **separate directory** — no shared file-system state
  between threads.
- `DatabaseManager` is read-only during cleanup (no writes occur).
- Results are aggregated after all futures complete (no concurrent writes to the
  aggregated report).

---

## 7. Configuration & Persistence

Uses `QSettings` with INI format.

```ini
[General]
SourceFolder=I:/Google Drive/.../Pictures/NEED TO SORT
DatabaseFile=I:/coding_workspaces/Python/IG_Sorter/ig_people.json
BatchSize=5
Theme=System

[OutputFolders]
1\Name=NSFW
1\Path=I:/Google Drive/.../Pictures/NSFW
2\Name=MSFW
2\Path=I:/Google Drive/.../Pictures/MSFW
3\Name=SFW
3\Path=I:/Google Drive/.../Pictures/SFW
size=3
```

**ConfigManager** wraps `QSettings` and provides typed getters/setters.

---

## 8. Final Report Screen

Displayed after all batches are sorted.

### 8.1 Content

```
═══════════════════════════════════════
          SORTING COMPLETE
═══════════════════════════════════════

Files sorted:        147
Files skipped:       0
Errors:              0

── Directory Summary ──
NSFW:    52 files  (now numbered 1–52)
MSFW:    63 files  (now numbered 1–63)
SFW:     32 files  (now numbered 1–32)

── New Accounts Added ──
• taylor_farr__ → Taylor Farr (personal)
• pnv.male_modelnetwork → Florian Macek (curator)

── Accounts by Type ──
Personal:   28 files
Curator:    15 files

═══════════════════════════════════════

[ Done ]    [ Settings ]
```

### 8.2 Button Actions
- **[Done]** — Clears the current session state and navigates back to `MenuScreen`.
- **[Settings]** — Opens `SettingsDialog` modally; returns to `ReportScreen` on close.

### 8.3 Data Source

The `SorterEngine` accumulates statistics during sorting:
- Total files moved.
- Files skipped (user chose "Skip").
- Errors encountered.
- New database entries added during the session.
- Final file count per output directory (read from disk at report time).

---

## 9. Error Handling & Edge Cases

| Scenario | Handling |
|----------|----------|
| Unknown account during sorting | Inline text field + "Open Instagram" button + "Add to DB" button |
| Database file missing or unreadable | Warn on startup; Settings dialog lets user browse to correct path |
| Output directory doesn't exist | Offer to create it; if creation fails, block and warn |
| File move fails (disk full, permission) | Log error, leave source file untouched, show error in report |
| Numbering gap during cleanup | Auto-fix with two-phase safe rename; report count in UI |
| Unknown name in output dir during cleanup | Show on CleanupScreen; require user to add to DB before continuing |
| Empty source directory | Show message on MenuScreen; disable "Start Sorting" |
| No output folders configured | Block start; require at least one in Settings |

---

## 10. Implementation Order

| Phase | Task | Deliverable |
|-------|------|-------------|
| **1** | Project scaffolding | CMakeLists.txt, empty src tree, builds to empty window |
| **2** | ConfigManager + SettingsDialog | Persistent settings, folder management |
| **3** | DatabaseManager | JSON load/save, account lookup, add entry |
| **4** | FileNameParser | Instaloader regex, ParsedResult struct |
| **5** | FileGrouper | Scans source dir, returns grouped files |
| **6** | DirectoryCleanup | Two-phase safe rename, async per-dir, unknown-name resolution |
| **7** | FileUtils | Safe copy-verify-delete, next-available-number |
| **8** | ThemeManager | Light/Dark/System via QSS |
| **9** | MenuScreen + screen navigation | QStackedWidget flow |
| **10** | CleanupScreen | Async progress bars, issue resolution UI |
| **11** | ImageThumbnail + ImagePreviewGrid | Clickable selectable thumbnails |
| **12** | SortPanel | Output buttons, IRL name display, unknown-account UI |
| **13** | SortingScreen | Full batch-sorting workflow |
| **14** | SorterEngine::sortFiles | Pipeline integration with FileUtils |
| **15** | ReportScreen | Summary statistics |
| **16** | Polish | Error handling, edge cases, icons, tooltips, testing |

---

## 12. Post-Implementation Bug Fixes

### First Pass (Initial Build Verification)

After the initial implementation was built, a gap analysis identified and fixed the
following issues. All fixes are applied and verified with a clean build.

| # | Severity | Fix Description |
|---|----------|----------------|
| 1 | **High** | `FileUtils::safeMove()` now returns empty string if source file deletion fails — prevents double-counting a file that exists in both locations |
| 2 | Medium | Dead `QFutureWatcher<CleanupReport>*` removed from `MainWindow::startSortingPipeline()` — it was created, connected, but never had `setFuture()` called |
| 3 | Medium | `QFile::remove()` and `QFile::rename()` return values are now checked in `DirectoryCleanup::cleanupDirectory()` — failures skip that file gracefully instead of silently corrupting |
| 4 | Medium | `MenuScreen::refreshConfigStatus()` added — config status label updates each time the user returns to the menu screen (e.g. after changing settings) |
| 5 | Medium | Database is now loaded at `MainWindow` startup via `m_db->load()` — previously only loaded when the user clicked "Start Sorting" |
| 6 | Medium | `SortingScreen::loadNextBatch()` calls `m_sortPanel->clearSelections()` on batch transition and on `allBatchesDone` — prevents stale "N files selected" label |
| 7 | Low | `FileGrouper::group()` now filters by image extensions (`.jpg`, `.jpeg`, `.png`, `.gif`, `.bmp`, `.webp`, `.tiff`, `.tif`, `.svg`, `.ico`) — non-image files are skipped entirely |
| 8 | Low | `ImageThumbnail` now draws a placeholder with the filename when `QPixmap` fails to load (e.g. `.webm` video files) — no more blank widgets |
| 9 | Low | `SortingScreen::finishClicked` signal is now connected in `MainWindow` — clicking "Finish" early generates a report with partial results |
| 10 | Low | `DirectoryCleanup` constructor now stores the `DatabaseManager*` pointer instead of ignoring it — adds a `run(QString)` instance method |
| 11 | Low | `#include <QSet>` added to `FileGrouper.cpp` |
| 12 | Low | Variable shadowing fixed in `FileGrouper::group()` — removed duplicate `QFileInfo fi` declaration in the else branch |

### Second Pass (Full Audit)

A comprehensive section-by-section audit identified these additional fixes:

| # | Severity | Fix Description |
|---|----------|----------------|
| 13 | Medium | `QSettings::setDefaultFormat(QSettings::IniFormat)` added to `main.cpp` — configs now stored as cross-platform INI files instead of Windows registry |
| 14 | Medium | `DirectoryCleanup::cleanupDirectoryInstance()` added — emits `directoryProgress` and `fileRenamed` signals during cleanup; `SorterEngine::runCleanup()` now uses instance-based cleanup with per-directory `QFutureWatcher` and progress signal forwarding |
| 15 | Medium | `SorterEngine::runCleanup()` is now properly wired — `MainWindow` calls `m_engine->runCleanup()` via `QFutureWatcher` instead of bypassing the engine with raw `QtConcurrent::run` |
| 16 | Low | `SortReportData::filesByAccountType` map added; `SortingScreen` tracks per-type file counts during sort; `ReportScreen` renders "── Accounts by Type ──" section |
| 17 | Low | `QSet` include explicitly added to `FileGrouper.cpp` (was relying on transitive include) |

### Intentional Deviations from Plan

These items differ from the plan by design decision, not by omission:

| Item | Plan Description | Actual Implementation | Reason |
|------|-----------------|----------------------|--------|
| BatchModel | `QAbstractListModel` for batches | `ImagePreviewGrid` widget manages `ImageThumbnail` children directly | Simpler architecture — no model/view complexity needed for a fixed-size grid |
| DirectoryCleanup::run() signature | `run(const QStringList& outputDirs)` | `runSingle(static, QString)` + `run(instance, QString)` + `SorterEngine::runCleanup()` orchestrates | Better separation of concerns — engine handles multi-dir coordination, static method for thread-safe calls |
| SorterEngine members | `DirectoryCleanup m_cleanup; FileGrouper m_grouper;` as private members | Created as locals within methods | Thread safety — objects created per-invocation avoid stale state |
| Qt Network module | Listed in §1.1 as required | Not included in CMake | No network operations needed — Instagram opens via `QDesktopServices::openUrl` |
| icons/ directory | Listed in §1.2 | Empty | Cosmetic — all buttons use text labels; icons can be added later |
| Progress bars | Per-directory animated progress via signals | Per-directory determinate progress bars updated by engine signals | Same user experience, simpler implementation |

---

## 13. Key Differences from Python Version

| Aspect | Python CLI | Qt C++ GUI |
|--------|-----------|------------|
| Input structure | 3 hardcoded `NEED TO SORT` folders | Single user-defined source folder |
| Output folders | 3 hardcoded (SFW/MSFW/NSFW) | User-defined, add/remove in settings |
| Database | `.xlsx` (Excel) + `photographers.txt` (separate) | Single unified `.json` with `personal` / `curator` / `irl_only` types |
| Curator accounts | Handled via separate `photographers.txt` list | Merged into main DB with `"type": "curator"` |
| Interaction | Console prompts, opens Explorer/browser | In-UI previews, clickable buttons, inline forms |
| Batch display | None (processes all at once) | Visual batches with pagination + multi-select |
| Cleanup | Runs silently at start | Async progress shown in dedicated screen |
| Unknown accounts | Console input + web browser | Inline text + type dropdown + "Open Instagram" button + DB add |
| Cross-platform | Windows-only (`os.startfile`) | Fully cross-platform (Qt) |
| Theming | None | Light / Dark / System via QSS |
| File operations | `os.rename` (direct) | Two-phase safe copy-verify-delete |
| Cleanup threading | Sequential | Async per-directory via `QtConcurrent` |
