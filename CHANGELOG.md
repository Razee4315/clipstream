# Changelog

All notable changes to ClipStream.

## [0.3.2] - 2026-10-05

### Changed
- The popup opens at the text cursor, like the Windows clipboard panel, and falls back to
  the mouse pointer when the app does not report one (Settings can force the pointer)
- Every open starts on All clips with an empty search

### Fixed
- Filter, selection, scrolling and search were slow: about 75-120 ms per filter click on a
  small history and up to a second with large clips or screenshots; now about 10-20 ms
  - the drop shadow was a live blur that re-rendered the whole popup on every repaint
  - every row's timestamp went through the system time zone on each reload and repaint
  - long clips were loaded and laid out in full just to show one line
  - screenshots were decoded on the UI thread, repeatedly
  - the row action bar was restyled on every selection change
- Saving a captured screenshot no longer stalls the hotkey and popup
- The first open after startup is no longer slower than the rest

## [0.3.1] - 2026-10-05

### Changed
- Smaller popup (420 × 480) with tighter rows; a Comfortable size is available in Settings
- The popup opens without taking focus, so the original app keeps its selection
- A single click pastes a clip; row actions moved below the title
- Settings reorganised into General, Privacy, and History tabs; changes save automatically

### Added
- Clear unpinned history, separate from the destructive Delete everything
- Clips that password managers mark as private are not captured
- A tray notice when a clip could not be pasted automatically (it stays copied)
- Optional Windows desktop check for real focus and paste behaviour

### Fixed
- A busy clipboard could make ClipStream paste the previous clipboard content instead of
  the chosen clip; the write is now confirmed and retried, and nothing is pasted if it fails
- Pasting returns to the original window and control after searching or opening Settings
- Paste waits for Ctrl/Shift/Alt to be released and never sends to a different window
- Pasting into an administrator window reports a failure instead of silently doing nothing
- History limits apply as clips arrive, not only at startup
- A second copy of ClipStream no longer starts alongside the first
- The popup no longer lingers when a menu is dismissed by switching apps
- Captured text no longer picks up a stray trailing NUL when the clipboard is busy

## [0.3.0] - 2026-10-04

### Added
- Roomier clipboard overlay with All clips, Pinned, Text, Images, and Links filters
- Full text/image previews, explicit sensitive-content reveal, and capture controls
- Ctrl+Space to preview, Ctrl+P to pin, and Ctrl+F to focus search
- New blue clipboard-and-stream logo across the app, tray, and installer
- Nine regression tests and automated CI checks

### Fixed
- Literal searches for URLs, paths, punctuation, and source app names
- Selection stays on the same clip when history changes or a clip is pinned
- Copying preserves indentation and trailing newlines
- Pausing capture cancels pending images; missing images no longer paste stale content
- Saving an existing pinned snippet keeps it pinned
- Retention limits count unpinned clips separately; edits refresh type and secret detection

### Changed
- Shorter README with an actual app screenshot and a direct download link

## [0.2.4] - 2026

### Changed
- Logo recoloured to blue (no purple)

### Fixed
- Action buttons now land in the correct position on the very first open
- The most-recent clip can now be deleted (its button was mispositioned)

### Added
- "Clear all history" button in Settings (deletes every clip + image files)

## [0.2.3] - 2026

### Changed
- New minimalist logo
- Action buttons now appear on row hover (not only after selecting)
- Settings page made more compact with plain-language labels
- Simplified, clearer footer hints

### Fixed
- A screenshot posted as a burst no longer creates duplicate image entries
  (image capture is now debounced)

## [0.2.2] - 2026

### Added
- Real icon set (Lucide SVGs, recoloured per theme) across the app — no emojis
- Interactive row action bar (Pin/Copy/Edit/Delete) with hover states and a
  copy → check-mark confirmation
- New ClipStream logo (replacing the inherited icon)
- Redesigned, theme-consistent Settings page

### Fixed
- A single screenshot no longer creates multiple duplicate image entries
- Sensitive/other rows that previously couldn't be deleted now delete correctly
  (FTS triggers made symmetric)

## [0.2.1] - 2026

### Added
- Inline row action buttons (Pin / Copy / Edit / Delete) on the selected or
  hovered row — no right-click needed
- More themes: Midnight, Nord, Forest, Rosé, Solarized Light (plus System/Dark/Light)

## [0.2.0] - 2026

### Changed
- **Rebuilt in C++/Qt 6** (from the original Tauri/Rust/Preact app, preserved on
  the `legacy` branch). Smaller, faster, fully native.

### Added
- Event-driven clipboard capture (no polling, ~0% idle CPU)
- Images stored as PNG files on disk instead of base64 in the database
- Smart actions: open URLs, reveal files, convert colours, evaluate maths
- `Ctrl+1`–`9` quick paste; `Ctrl+N` snippets; `Ctrl+O` open
- Privacy: secret masking, password-manager exclusions, "never store secrets",
  tray pause toggle
- System / Dark / Light themes; fade-in animation; first-run onboarding
- Multi-monitor aware overlay positioning
- Cross-platform abstraction layer (Windows complete; Linux/macOS stubbed)
- CMake + Ninja build, windeployqt bundle, Inno Setup installer, GitHub Actions CI

## [0.1.0] - 2024

### Added
- Initial release
- Global hotkey (Ctrl+Shift+V)
- Clipboard monitoring with source app detection
- SQLite storage with FTS5 search
- Pin/unpin clipboard entries
- Auto-paste functionality
- System tray integration
- Dark/Light theme support
- Keyboard navigation
- Auto-cleanup (7 days, 500 max entries)
