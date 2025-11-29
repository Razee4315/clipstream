# ClipStream

A lightweight, context-aware clipboard manager for Windows that feels native to the OS.

![Windows](https://img.shields.io/badge/Windows-0078D6?style=flat&logo=windows&logoColor=white)
![Rust](https://img.shields.io/badge/Rust-000000?style=flat&logo=rust&logoColor=white)
![Tauri](https://img.shields.io/badge/Tauri-24C8DB?style=flat&logo=tauri&logoColor=white)
![License](https://img.shields.io/badge/License-MIT-green.svg)

## Features

- **Global Hotkey** - `Ctrl+Shift+V` to open anywhere
- **Smart Search** - Full-text search with FTS5
- **Source Tracking** - Shows which app copied the text
- **Auto Paste** - Select and paste in one action
- **Pin Items** - Keep important clips at top
- **Lightweight** - Under 25MB, minimal RAM usage
- **Native Feel** - Frameless overlay, system tray

## Installation

Download the latest release from [Releases](https://github.com/razee4315/clipstream/releases).

**Or build from source:**

```bash
# Prerequisites: Node.js, Rust, MinGW-w64

git clone https://github.com/razee4315/clipstream.git
cd clipstream
npm install
npm run tauri build
```

## Usage

| Shortcut | Action |
|----------|--------|
| `Ctrl+Shift+V` | Open/Close |
| `↑` `↓` | Navigate |
| `Enter` | Paste selected |
| `Esc` | Close |
| `Shift+Del` | Delete entry |

## Tech Stack

- **Backend**: Rust + Tauri v2
- **Frontend**: Preact + TypeScript
- **Database**: SQLite with FTS5
- **Styling**: Tailwind CSS

## Author

**Saqlain Abbas**

[![LinkedIn](https://img.shields.io/badge/LinkedIn-0077B5?style=flat&logo=linkedin&logoColor=white)](https://www.linkedin.com/in/saqlainrazee/)
[![GitHub](https://img.shields.io/badge/GitHub-100000?style=flat&logo=github&logoColor=white)](https://github.com/razee4315)

## License

[MIT License](LICENSE) - feel free to use and modify.

## Contributing

Contributions welcome! Please read [CONTRIBUTING.md](CONTRIBUTING.md) first.
