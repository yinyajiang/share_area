# AGENTS.md

This file provides guidance to Codex (Codex.ai/code) when working with code in this repository.

## Build Commands

```bash
# Configure (adjust Qt path for your setup)
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/Qt/6.10.x/macos

# Build
cmake --build build --config Release --parallel

# Update translations (after adding/modifying tr() calls)
lupdate src/*.cpp src/**/*.cpp -ts translations/*.ts
lrelease translations/*.ts
```

Qt paths are hardcoded in CMakeLists.txt for CI. Adjust `CMAKE_PREFIX_PATH` for local builds.

## Architecture

ShareArea is a Qt 6 desktop app for local network file sharing. C++17, CMake 3.20+.

**Three-layer design:**

- `src/core/` — App settings (singleton), shared file metadata, auto-start logic
- `src/network/` — UDP peer discovery (port 19876, broadcast every 3s) and TCP file transfer (port 19877, 512KB chunked streaming)
- `src/ui/` — Frameless QMainWindow with custom-painted widgets, system tray, drag-and-drop

**Key patterns:**
- Qt signals/slots for all inter-component communication
- QThread for file transfer operations
- `AppSettings` singleton for persisted config (QSettings backend)
- Custom QWidget subclasses with QSvgRenderer for all UI
- `.mm` files for macOS-native APIs (blur effect via AppKit)

**Network protocol:**
- Devices with matching group codes discover each other via UDP broadcast
- File transfers use TCP with chunked streaming and progress reporting
- Peer timeout: 10 seconds of inactivity

**Translations:** `translations/share_area_zh_CN.ts` and `translations/share_area_en.ts`. All user-visible strings must use `tr()`. Both files need manual updates when source strings change.

**Platform support:** macOS (universal ARM64+x86_64, macdeployqt) and Windows (MSVC 2022, windeployqt + InnoSetup installer). CI builds via GitHub Actions.

## Code Style

LLVM clang-format with 4-space indentation. No tabs. C++17 standard.
