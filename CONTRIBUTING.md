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
