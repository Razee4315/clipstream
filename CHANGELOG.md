# Changelog

All notable changes to ClipStream.

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
