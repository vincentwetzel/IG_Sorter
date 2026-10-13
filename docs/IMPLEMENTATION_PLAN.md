# Implementation Status and Plan

This document is a maintained snapshot of the current application plus follow-up work. The implementation in `IG_Sorter_Qt/src/` is authoritative when behavior and prose differ. See [Architecture](ARCHITECTURE.md) for the current design and [Changelog](CHANGELOG.md) for recorded user-visible changes.

## Implemented

- Qt 6 / C++17 desktop application built with CMake.
- Multi-screen workflow for menu, output cleanup, batch sorting, reporting, account cleanup, Settings, and duplicate review.
- Filename parsing for Instaloader-style Instagram media, TikTok slideshow IDs, Facebook media timestamps, Twitter/X timestamps, and Instagram/Snapchat/Chrome Android screenshots.
- JSON people database with Personal, Curator, and IRL-only entry types, case-insensitive account lookup, editing, and incremental lookup indexing.
- Configurable source and ordered destination folders, database path, preview batch size, and theme.
- Preview selection and sub-batching, unknown account/person flows, favorites, add-person handling, and account cleanup with search, missing-name filtering, inline edits, and entry removal. Sorting requires a matching database identity; confirming a new IRL-only person creates a name-only database entry.
- Favorite-name quick-fill counts track successfully handled photos, correct partial failures, and favor less recently used names when photo counts tie.
- Output numbering cleanup, extension correction, safe file movement, undo for recent sort moves, and report summaries.
- Exact-byte duplicate checks when sorting batches; confirmed output duplicates are still removed directly from the source with `QFile::remove`.
- Asynchronous source discovery with progress reporting, output cleanup, sorting file actions, deletion, and undo paths to keep the UI responsive. The sorting screen receives completed groups after scanning and consolidating posts for known Personal accounts; Curator and IRL-only groups stay separate, while unknown files may be grouped by save-time proximity within one directory.
- Recursive duplicate review with cached file hashes, perceptual image matching followed by 16x16 and 128x128 grayscale comparisons, exact SHA-256 fallback, progress, cancellation, streamed groups that remain navigable as results arrive, Recycle Bin deletion, and undo support.
- Responsive sorting controls: favorite-name quick-fill buttons fit a single row and hide overflow, and thumbnail selection indicators follow the displayed image bounds.
- Atomic JSON database replacement on save to preserve the last complete database if writing fails.
- Light and dark QSS themes, application logging, and Windows WIC WebP preview fallback.

## Follow-up work

Items below are opportunities for future work, not claims that the feature is missing from every related flow. Add or remove items as implementation needs change.

### Reliability and data safety

- Add focused automated coverage for filename parsing, database read/write and indexing, grouping, numbering cleanup, duplicate matching, and safe file operations.
- Review write-failure handling for database save and file operations so failures are consistently surfaced in the UI and report.
- Revisit automatic exact-duplicate removal: current matches are deleted directly from the source with `QFile::remove`, outside the Recycle Bin and undo flows. Decide whether to make this configurable, reviewable, or recoverable before expanding it.
- Document and exercise recovery behavior for interrupted two-phase renames and duplicate-review undo staging.

### Performance and scale

- Revisit grouping-cache invalidation: the current cache is in-memory and relies on source-directory metadata, so changes deep in a directory may not always be represented by a root timestamp on every filesystem.
- Profile duplicate scans on large output trees and tune cache serialization, candidate bucketing, and cancellation responsiveness based on measurements.
- Profile preview layout and thumbnail loading with large and mixed-aspect-ratio batches.

### Packaging and usability

- Add reproducible CMake presets or platform-specific build notes once supported toolchains are established.
- Define a release packaging and update process for supported operating systems.
- Keep supported filename patterns, database format, and first-run setup synchronized across README and architecture documentation.

## Completion guidance

When implementing a follow-up item, update the implementation, add focused verification where appropriate, and then update this status list and the changelog. Do not mark planned work as implemented based solely on a design or declaration.


