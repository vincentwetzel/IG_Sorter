# IG Sorter — Coding Standards & Guidelines

This document outlines the coding standards, naming conventions, and best practices for the **IG_Sorter** Qt 6 C++ project. Following these rules ensures the codebase remains maintainable, consistent, thread-safe, and easy to navigate.

---

## 1. Language & Framework Standards
- **Language:** C++17 or newer.
- **Framework:** Qt 6 (Core, Widgets, Gui, Concurrent), matching the modules currently linked by `IG_Sorter_Qt/CMakeLists.txt`. Add a Qt module only when a feature needs it and update CMake and this document together.
- **UI strings:** Use QString for application and UI text. Do not introduce std::string into UI code.
- **Engine Execution:** Heavy operations (e.g., directory parsing, file grouping, duplicate visual matching) must run asynchronously via Qt Concurrent or `QThread` wrappers to keep the main UI responsive.

---

## 2. Naming Conventions

### 2.1 Files
- Source and Header file names must match the class name they declare exactly.
- Use CamelCase for filenames.
  - *Example:* `MainWindow.cpp`, `MainWindow.h`, `ConfigManager.h`

### 2.2 Classes & Structs
- Use **PascalCase** / **CamelCase** starting with an uppercase letter.
  - *Example:* `class SorterEngine;`, `struct FileGroup;`

### 2.3 Functions & Methods
- Use **camelCase** starting with a lowercase letter.
  - *Example:* `void refreshAccountsList();`, `bool hasChanges() const;`

### 2.4 Variables
- **Member Variables:** Prefix with `m_` followed by camelCase.
  - *Example:* `QStackedWidget* m_stackedWidget;`, `DatabaseManager* m_db;`
- **Local Variables:** Use standard camelCase without prefixes.
  - *Example:* `QString sourceFolder;`, `int emptyCount = 0;`
- **Global/Static Constants:** Use CamelCase or snake_case, but keep it highly descriptive. Static instances should follow the standard pointer naming or singleton instance patterns (`s_instance`).

### 2.5 Enums
- Use strongly typed `enum class` declarations where possible, using PascalCase for both the enum name and its values.
  - *Example:*
    ```cpp
    enum class AccountType {
        Personal,
        Curator,
        IrlOnly
    };
    ```

---

## 3. Qt & Modern C++ Best Practices

### 3.1 Memory Management & Object Ownership
- **Qt Parent-Child Relationship:** For classes deriving from `QObject` (especially `QWidget` subclasses), rely on the built-in parent-child ownership. Always pass `parent` to the base constructor.
  ```cpp
  AccountEditRow::AccountEditRow(QWidget* parent) : QWidget(parent) { ... }
  ```
- **Non-QObject Lifetimes:** For plain C++ classes, prefer standard smart pointers (`std::unique_ptr`, `std::shared_ptr`) to raw pointers to avoid memory leaks.

### 3.2 Signals and Slots
- Always use the modern, type-safe pointer-to-member-function connection syntax instead of the older `SIGNAL()` and `SLOT()` macros.
  - *Recommended:*
    ```cpp
    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::startSortingPipeline);
    ```
- Use lambdas for simple, lightweight actions, making sure to capture variables correctly (usually capturing `this` or local copies safely).

### 3.3 Const Correctness & Modern C++ Idioms
- **Const-by-Default:** Mark member functions as `const` if they do not alter the logical state of the object. Pass complex objects (such as `QString`, `QList`, or custom structs) as `const` references to avoid expensive copy operations.
  ```cpp
  bool hasChanges() const; // Read-only method
  void updateConfig(const QString& sourceFolder); // Read-only parameter
  ```
- **Modern C++17 Utilities:** Use modern features where readability and performance can be improved:
  - **Structured Bindings:** Use for readable map and pair unpacking: `auto [key, value] = mapEntry;`
  - **Range-Based For Loops:** Prefer `for (const auto& item : container)` over index-based loops.
  - **std::string_view / QStringView:** Use non-owning views for read-only string slices.

### 3.4 Header Hygiene & Forward Declarations
- Always use `#pragma once` at the very beginning of header files.
- **Minimize Includes in Headers:** Do not `#include` heavy dependencies inside header files if a forward declaration is sufficient. This reduces compilation times and avoids circular dependency loops.
  ```cpp
  // In SorterEngine.h
  class DatabaseManager; // Forward declaration instead of #include "DatabaseManager.h"
  ```
- Order your includes consistently:
  1. Associated header (e.g., `ui/MainWindow.h` in `MainWindow.cpp`)
  2. Standard Library headers (`<vector>`, `<memory>`, etc.)
  3. Qt headers (`<QWidget>`, `<QString>`, etc.)
  4. Project local headers (`core/SorterEngine.h`, `utils/ConfigManager.h`)

### 3.5 Thread Safety & Asynchronous Operations
- **UI Thread Responsiveness:** Heavy disk access, hash generation, and visual perceptual matching must occur on background threads using `QtConcurrent::run`, `QtConcurrent::map`, or a dedicated worker.
- **Mutex Protection:** Any shared data structure (such as grouping caches or filename generators) accessed by background threads must be guarded using `QMutex` and `QMutexLocker`.
- **Cross-Thread Connections:** When connecting signals from a background thread to a slot in the UI thread, rely on Qt's default `Qt::AutoConnection` (which resolves to `Qt::QueuedConnection` when crossing threads) to ensure UI components are only modified on the main GUI thread.
- **Burst UI Updates:** When a widget can lose many children quickly, batch deletions, suppress updates briefly, and defer expensive relayout work instead of recalculating after every single removal.

### 3.6 Code Documentation
- **Doxygen Style:** Document all public APIs and complex implementation blocks using Doxygen comments (`/** ... */`).
- **Methods:** Include descriptions for what the method does, its parameters (`@param`), and its return values (`@return`).
  ```cpp
  /**
   * @brief Matches source file signatures to find visual duplicates.
   * @param targetDir The directory to scan.
   * @return A list of duplicate file groups found.
   */
  QList<DuplicateGroup> scanForDuplicates(const QString& targetDir);
  ```

---

## 4. UI Layout & Styles
- **No Hardcoded Sizes:** Avoid absolute pixel boundaries for controls containing text. Rely on layouts (`QVBoxLayout`, `QHBoxLayout`, `QGridLayout`) and size policies to ensure the application scales correctly across different High-DPI screens.
- **Stylesheets:** Centralize UI styling and colors through `ThemeManager` QSS stylesheets. Do not use inline `setStyleSheet()` calls unless assigning temporary dynamic visual states (e.g., highlighting selected grid borders). Rely on semantic styling IDs (e.g., `#deleteButton`, `#curatorSortButton`).

---

## 5. Error Handling & Logging

### 5.1 Logging
- Do not write directly to `std::cout` or `std::cerr`.
- Use the custom `LogManager` to record file actions, status changes, errors, and system warnings. This ensures debug messages are securely archived in daily log rotations.
  ```cpp
  LogManager::instance()->info(QString("Database path: %1").arg(dbPath));
  LogManager::instance()->error(QString("ERROR: Failed to save database."));
  ```

### 5.2 User Feedback
- Inform users of severe validation issues (e.g., missing configurations, disk errors) using non-blocking status messages or modal notification boxes like `QMessageBox::warning` or `QMessageBox::critical`.

---

## 6. Formatting & Structure

- **Indentation:** Use 4 spaces (no raw tabs).
- **Line Width:** Keep lines under 100-120 characters where possible to improve readability.
- **Brace Style:** Use the standard K&R style or consistent Egyptian brackets:
  ```cpp
  if (condition) {
      // code
  } else {
      // code
  }
  ```
- **Clean Up Comments:** Avoid leaving commented-out legacy code blocks. Keep logical sections divided by structured comment dividers if helpful:
  ```cpp
  // ─── Slot Implementations ──────────────────────────────────────────────────
  ```

---

*By adhering to these rules, we ensure the IG Sorter workspace remains modular, easy to test, and ready for collaborative development.*

## 7. Project-Specific Guidance

### 7.1 Keep behavior and documentation aligned

- Treat `IG_Sorter_Qt/CMakeLists.txt` and the implementation under `IG_Sorter_Qt/src/` as authoritative for the compiled source set and current behavior.
- When changing a filename pattern, database field/type, user-visible workflow, or duplicate matching rule, update `docs/ARCHITECTURE.md` and the root `README.md` when setup or user behavior is affected.
- Record user-visible changes in `docs/CHANGELOG.md`. Keep future work in `docs/IMPLEMENTATION_PLAN.md` clearly marked as planned until implemented.
- Keep private data and downloaded content out of documentation examples that could be mistaken for real user data; use synthetic examples.

### 7.2 Background task boundaries

- Capture immutable values or clearly owned objects when starting `QtConcurrent` work. Do not capture short-lived stack references unless the task lifetime is synchronously bounded and obvious.
- Do not read or mutate widgets from worker functions. Return results or emit signals, then update the UI through the owning widget's thread.
- Ensure a watcher and its owning QObject remain valid until the future finishes. Handle cancellation and completion paths so controls and progress state are restored or reconciled, including when the UI advances optimistically.
- Protect shared state only when it is genuinely accessed concurrently. Prefer task-local state and value results over broad locks.

### 7.3 Build and verification

- Keep the application buildable through the existing CMake project and use C++17 as configured there.
- The current CMake project does not define a test target. When adding tests, introduce an explicit test target and document how to run it rather than implying tests exist.
- For changes to file operations, verify the relevant success, collision, and failure paths before describing the behavior as safe. Avoid changing or deleting user data as part of development verification.


