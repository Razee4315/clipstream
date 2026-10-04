<p align="center">
  <img src="resources/logo.svg" width="88" alt="ClipStream logo">
</p>
<h1 align="center">ClipStream</h1>
<p align="center">Your clipboard, with a memory.</p>
<p align="center">
  A fast, native Windows clipboard manager. Find it. Preview it. Paste it.
</p>
<p align="center">
  <a href="https://github.com/Razee4315/clipstream/releases/latest"><img src="https://img.shields.io/github/v/release/Razee4315/clipstream?color=2563eb&label=download" alt="Latest release"></a>
  <img src="https://img.shields.io/badge/Windows-10%20%2F%2011-2563eb" alt="Windows 10 and 11">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-64748b" alt="MIT license"></a>
</p>
<p align="center">
  <img src="images/overlay-dark.png" width="540" alt="ClipStream showing search, filters, pinned snippets, and clipboard history in dark mode">
</p>

## Get started

[**Download ClipStream for Windows →**](https://github.com/Razee4315/clipstream/releases/latest)

Run the installer, copy something, then press **Ctrl+Shift+V** to open your history.
ClipStream stays in your system tray.

## Made for your everyday copy & paste

- **Find anything faster.** Search text or app names; filter pinned clips, text, images, and links.
- **Keep the useful bits.** Pin favourites, save reusable snippets, and preview full clips before pasting.
- **Do more with a clip.** Open links and files, change text case, convert colours, or paste a calculation result.
- **Stay in control.** Pause capture, exclude apps, set retention, and mask detected secrets. History stays on your device.
- **Feel at home.** Native C++ / Qt 6, keyboard navigation, and eight theme options including System.

## Shortcuts

| Action | Shortcut |
| --- | --- |
| Open / close | **Ctrl+Shift+V** |
| Navigate / paste | **↑ ↓** / **Enter** |
| Quick-paste a clip | **Ctrl+1–9** |
| Preview / pin | **Ctrl+Space** / **Ctrl+P** |
| New snippet / search | **Ctrl+N** / **Ctrl+F** |
| Format on paste | **Shift+Enter** |

<details>
<summary>More shortcuts & building from source</summary>

**Ctrl+O** opens a link or file · **F2** edits · **Shift+Delete** deletes · **Esc** closes.

Requires Qt 6 (MinGW kit, including SVG and Test), CMake, and Ninja on `PATH`.

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build build
ctest --test-dir build --output-on-failure
```

Tests use a temporary database and offscreen clipboard. UI screenshots are saved
in `build/artifacts/`. See [Contributing](CONTRIBUTING.md) for development details.

</details>

---

Made by [Saqlain Abbas](https://www.linkedin.com/in/saqlainrazee/) · [MIT](LICENSE) · [Changelog](CHANGELOG.md) · [Report a bug](https://github.com/Razee4315/clipstream/issues)
