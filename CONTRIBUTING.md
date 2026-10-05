# Contributing to ClipStream

Thanks for your interest in contributing!

## Quick Start

1. Fork the repository
2. Create a branch: `git checkout -b feature/your-feature`
3. Make changes and test
4. Commit: `git commit -m "Add your feature"`
5. Push: `git push origin feature/your-feature`
6. Open a Pull Request

## Development Setup

Requires Qt 6 with Widgets, SQL, SVG and Test, plus CMake and a matching C++ kit.
For the Windows MinGW kit, put Qt, MinGW and Ninja on `PATH` first.

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build build
ctest --test-dir build --output-on-failure
```

## Brand assets

The SVGs in `resources/` use the rounded C symbol from Logo Concept 03.
`logo.svg` and `logo-dark.svg` are the compact symbol; `wordmark.svg` and
`wordmark-dark.svg` contain the full lettering. `app-icon.svg` adds a neutral
tile so the Windows icon remains visible on both light and dark backgrounds.
The generator outlines the reference's Sora SemiBold wordmark using the bundled
OFL-licensed font in `resources/fonts/`, so no installed font is needed at runtime.
`readme-logo.svg` and `readme-logo-dark.svg` combine the symbol and lettering
into an aligned horizontal lockup for GitHub.
After editing the SVGs, regenerate the checked-in PNG, multi-size ICO, and
installer bitmaps before building the app or installer:

```powershell
cmake --build build --target ClipStreamLogo
./build/ClipStreamLogo.exe resources
cmake --build build
```

## Tests

`ctest` runs `ClipStreamTests` on Qt's offscreen platform with a temporary database.
It links the stub platform backends, so it never sends real keystrokes: it covers
search, retention, capture rules, and UI rendering, and saves screenshots to
`build/artifacts/`. It cannot prove that a paste reaches another application.

Focus and paste are covered by an optional Windows-only target that links the real
Win32 backend and drives a separate scratch-editor process:

```powershell
cmake --build build --target ClipStreamDesktopCheck
./build/ClipStreamDesktopCheck.exe --verify
```

Run it from an interactive, unlocked desktop session and keep your hands off the
keyboard and mouse for the ten seconds it takes. It opens a small editor window,
takes the foreground, sends real key and mouse input, writes to the clipboard, and
then restores your previous clipboard and foreground window. It waits for input to
be idle before starting and only sends keys while its own editor is in front, but
anything you type during the run lands in that editor and fails the test. It is not
built by default and does not run in CI. Results go to
`build/artifacts/windows-paste-results.xml`; the `OleSetClipboard: Failed` warnings
there come from the test that deliberately holds the clipboard open.

Two more modes help when working on speed or popup placement. Neither sends input:

```powershell
./build/ClipStreamDesktopCheck.exe --bench            # timings for a heavy synthetic history
./build/ClipStreamDesktopCheck.exe --bench <folder>   # timings for a copy of a real data folder
./build/ClipStreamDesktopCheck.exe --caret            # where the foreground app reports its text cursor
```

`--bench` shows the popup in a corner for a few seconds and writes
`build/artifacts/bench.txt`. Filter clicks, selection and scrolling should stay
well under 30 ms. Point it at a copy of a data folder, never the live one.

The scratch editor is a plain Win32 edit control. A pass there does not prove
every application behaves the same, so test changes to `src/platform/win/` in a
few real apps as well. Running `ClipStreamDesktopCheck.exe` with no arguments opens
a manual playground with temporary history.

## Guidelines

- Keep code clean and simple
- Test your changes before submitting
- Follow existing code style
- Update documentation if needed

## Bug Reports

Include:
- OS version
- Steps to reproduce
- Expected vs actual behavior

## Questions?

Open an issue or reach out on [LinkedIn](https://www.linkedin.com/in/saqlainrazee/).
