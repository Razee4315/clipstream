# ClipStream

A fast, native clipboard manager for Windows — built in C++ with Qt 6.

![Windows](https://img.shields.io/badge/Windows-0078D6?style=flat&logo=windows&logoColor=white)
![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat&logo=cplusplus&logoColor=white)
![Qt](https://img.shields.io/badge/Qt-6-41CD52?style=flat&logo=qt&logoColor=white)
![License](https://img.shields.io/badge/License-MIT-green.svg)

ClipStream remembers everything you copy — text and images — and brings it back
instantly with a global hotkey, full-text search, and one-keystroke paste.

> **Note:** ClipStream was originally built with Tauri (Rust + Preact). It has been
> rebuilt from the ground up in **C++/Qt 6** for a smaller, faster, fully native app.
> The original implementation lives on the [`legacy`](../../tree/legacy) branch.

## Features

- **Event-driven capture** — listens to the OS clipboard, no polling, ~0% idle CPU
- **Global hotkey** — `Ctrl+Shift+V` opens the overlay anywhere, at your cursor
- **Full-text search** — instant SQLite FTS5 search across your history
- **Text & images** — images stored as files on disk (not bloating the database)
- **Smart actions** — open URLs, reveal files, convert colours, evaluate maths
- **Format on paste** — paste as UPPER / lower / Title / trimmed
- **Quick paste** — `Ctrl+1`–`9` to paste the Nth clip instantly
- **Snippets** — save reusable text (`Ctrl+N`), kept pinned at the top
- **Source tracking** — shows which app each clip came from
- **Privacy first** — masks detected secrets, excludes password managers, pause toggle
- **Themes** — System / Dark / Light
- **Lightweight & native** — ~39 MB bundle, single-file installer ~13 MB

## Install

Download the latest installer from [Releases](https://github.com/Razee4315/clipstream/releases),
or build from source below.

## Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+Shift+V` | Open / close the overlay |
| `↑` `↓` | Navigate |
| `Enter` | Paste selected |
| `Shift+Enter` | Paste with formatting |
| `Ctrl+1`–`9` | Quick-paste the Nth clip |
| `Ctrl+O` | Open link / file (smart action) |
| `Ctrl+N` | New snippet |
| `F2` | Edit clip |
| `Shift+Del` | Delete clip |
| `Esc` | Close |

## Tech stack

- **Language:** C++17
- **UI:** Qt 6 Widgets (frameless translucent overlay, model/view, custom delegate)
- **Storage:** SQLite + FTS5 via Qt SQL
- **Platform:** Win32 (`RegisterHotKey`, `SendInput`, `GetForegroundWindow`) behind a
  cross-platform abstraction (Linux/macOS stubs ready to implement)
- **Build:** CMake + Ninja (MinGW); packaged with windeployqt + Inno Setup

## Build from source

Requires Qt 6 (MinGW kit), CMake, and Ninja.

```powershell
# Configure (Release, Ninja)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"

# Build
cmake --build build

# Bundle the Qt runtime to run anywhere
windeployqt --release --no-translations dist\ClipStream\ClipStream.exe
```

CI builds and packages an installer on every push (see `.github/workflows/build.yml`).

## Project layout

```
src/
├─ main.cpp, theme.{h,cpp}, AppController.{h,cpp}
├─ core/      ClipboardMonitor · Database (SQLite/FTS5) · ContentClassifier · MathEval
├─ ui/        OverlayWindow · HistoryModel · EntryDelegate · SettingsDialog
└─ platform/  HotkeyManager · ForegroundApp · PasteSimulator · Autostart
              ├─ win/   (Windows implementations)
              └─ stub/  (Linux/macOS placeholders)
```

## Author

**Saqlain Abbas**

[![LinkedIn](https://img.shields.io/badge/LinkedIn-0077B5?style=flat&logo=linkedin&logoColor=white)](https://www.linkedin.com/in/saqlainrazee/)
[![GitHub](https://img.shields.io/badge/GitHub-100000?style=flat&logo=github&logoColor=white)](https://github.com/razee4315)

## License

[MIT License](LICENSE) — feel free to use and modify.

## Contributing

Contributions welcome! Please read [CONTRIBUTING.md](CONTRIBUTING.md) first.
