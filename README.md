# ShareArea

[![zread](https://img.shields.io/badge/Ask_Zread-_.svg?style=flat&color=00b0aa&labelColor=000000&logo=data%3Aimage%2Fsvg%2Bxml%3Bbase64%2CPHN2ZyB3aWR0aD0iMTYiIGhlaWdodD0iMTYiIHZpZXdCb3g9IjAgMCAxNiAxNiIgZmlsbD0ibm9uZSIgeG1sbnM9Imh0dHA6Ly93d3cudzMub3JnLzIwMDAvc3ZnIj4KPHBhdGggZD0iTTQuOTYxNTYgMS42MDAxSDIuMjQxNTZDMS44ODgxIDEuNjAwMSAxLjYwMTU2IDEuODg2NjQgMS42MDE1NiAyLjI0MDFWNC45NjAxQzEuNjAxNTYgNS4zMTM1NiAxLjg4ODEgNS42MDAxIDIuMjQxNTYgNS42MDAxSDQuOTYxNTZDNS4zMTUwMiA1LjYwMDEgNS42MDE1NiA1LjMxMzU2IDUuNjAxNTYgNC45NjAxVjIuMjQwMUM1LjYwMTU2IDEuODg2NjQgNS4zMTUwMiAxLjYwMDEgNC45NjE1NiAxLjYwMDFaIiBmaWxsPSIjZmZmIi8%2BCjxwYXRoIGQ9Ik00Ljk2MTU2IDEwLjM5OTlIMi4yNDE1NkMxLjg4ODEgMTAuMzk5OSAxLjYwMTU2IDEwLjY4NjQgMS42MDE1NiAxMS4wMzk5VjEzLjc1OTlDMS42MDE1NiAxNC4xMTM0IDEuODg4MSAxNC4zOTk5IDIuMjQxNTYgMTQuMzk5OUg0Ljk2MTU2QzUuMzE1MDIgMTQuMzk5OSA1LjYwMTU2IDE0LjExMzQgNS42MDE1NiAxMy43NTk5VjExLjAzOTlDNS42MDE1NiAxMC42ODY0IDUuMzE1MDIgMTAuMzk5OSA0Ljk2MTU2IDEwLjM5OTlaIiBmaWxsPSIjZmZmIi8%2BCjxwYXRoIGQ9Ik0xMy43NTg0IDEuNjAwMUgxMS4wMzg0QzEwLjY4NSAxLjYwMDEgMTAuMzk4NCAxLjg4NjY0IDEwLjM5ODQgMi4yNDAxVjQuOTYwMUMxMC4zOTg0IDUuMzEzNTYgMTAuNjg1IDUuNjAwMSAxMS4wMzg0IDUuNjAwMUgxMy43NTg0QzE0LjExMTkgNS42MDAxIDE0LjM5ODQgNS4zMTM1NiAxNC4zOTg0IDQuOTYwMVYyLjI0MDFDMTQuMzk4NCAxLjg4NjY0IDE0LjExMTkgMS42MDAxIDEzLjc1ODQgMS42MDAxWiIgZmlsbD0iI2ZmZiIvPgo8cGF0aCBkPSJNNCAxMkwxMiA0TDQgMTJaIiBmaWxsPSIjZmZmIi8%2BCjxwYXRoIGQ9Ik00IDEyTDEyIDQiIHN0cm9rZT0iI2ZmZiIgc3Ryb2tlLXdpZHRoPSIxLjUiIHN0cm9rZS1saW5lY2FwPSJyb3VuZCIvPgo8L3N2Zz4K&logoColor=ffffff)](https://zread.ai/yinyajiang/share_area)

[English](README.md) | [简体中文](README_zh.md)


<p align="center">
  <img src="screenshot/en.png" alt="ShareArea screenshot" width="400" />
</p>

ShareArea is a lightweight desktop app for sharing files, folders, and clipboard content on a local network with no setup required. **Transfers are unencrypted by design**, so it is intended only for trusted local networks and your own devices. Open the app on devices in the same LAN, enter the same group code, and start sharing.

## Highlights

- Local-network file sharing with no external server
- Unencrypted transfer by design for simple, low-overhead LAN sharing
- Share files, folders, text, and clipboard text/images
- Drag and drop, or double-click, to publish content immediately
- Lightweight grouping with a simple group code
- Automatic discovery of online devices in the same network
- Clean desktop UI with system tray support

## Getting Started

1. Open ShareArea on devices connected to the same local network.
2. On first launch, enter the same group code on each device.
3. Drag files or folders into the window, double-click to share clipboard content, or copy a file and then double-click the window.
4. Other devices in the same group will immediately see the shared content and can download it directly.

## Download

Prebuilt packages are available on the [Releases](https://github.com/yinyajiang/share_area/releases) page.

- `macOS`: DMG installer
- `Windows`: Installer

### macOS Run Tip

Because the app is currently unsigned, macOS may report it as "damaged" on first launch. Run the following command in Terminal:

```bash
xattr -c /Applications/ShareArea.app/
```

## Platform Support

- `macOS`: supported
- `Windows`: supported
- `Linux`: not verified

## Build From Source

### Requirements

- `CMake 3.20+`
- `Qt 6.10.x`

### Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/Qt/6.10.x/macos
cmake --build build --config Release --parallel
```

## License

This project is licensed under the [MIT License](LICENSE).
