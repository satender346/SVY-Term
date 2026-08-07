# SVY-Term

SVY-Term is a macOS-focused C/C++ remote operations desktop application inspired by the workflow categories documented for MobaXterm.

## Goals

- Native desktop experience for macOS
- C/C++ implementation using Qt6
- Multi-tab terminal workspace with split views
- Session manager for SSH and local sessions
- SSH/SFTP/Tunnel architecture with clear extension points
- Config and startup CLI options for automation

## Current status

This repository currently includes a compilable MVP shell:

- Main window with tabbed workspace
- Local terminal tabs backed by local shell process
- Session manager with persistent storage
- SSH/SFTP/Tunnel service scaffolding in C++
- Config file management
- CLI flags for startup behavior

See docs/MOBAXTERM_PARITY.md for feature-by-feature mapping and implementation status.

## Build (macOS)

Prerequisites:

- CMake 3.21+
- A C++20 compiler (AppleClang)
- Qt6 (Core, Gui, Widgets, Network)
- libssh (for native SSH and SFTP backends)

Build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Run:

```bash
./build/SVY-Term.app/Contents/MacOS/SVY-Term
```

## Linux quick setup (Ubuntu)

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build pkg-config qt6-base-dev qt6-base-dev-tools qt6-tools-dev qt6-tools-dev-tools libssh-dev
```

## macOS quick setup (Homebrew)

```bash
brew install cmake ninja qt@6 libssh
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6)"
cmake --build build -j
```

## macOS install without CMake/Homebrew on your machine

If your Mac policy does not allow installing CMake or Homebrew, use the prebuilt artifact:

1. Push this repository to GitHub.
2. Open GitHub Actions and run workflow `macOS Build`.
3. Download artifact `SVY-Term-macOS`.
4. Unzip `SVY-Term-macOS.zip` and move `SVY-Term.app` into `Applications`.
5. First launch may require right-click -> Open (unsigned app).

Workflow file:

- `.github/workflows/macos-build.yml`

## Publish installable macOS app on GitHub Releases

To automatically publish a downloadable app zip on each release tag:

1. Commit and push your changes.
2. Create and push a version tag, for example:

```bash
git tag v0.1.0
git push origin v0.1.0
```

3. GitHub Actions workflow `macOS Build` will:
- build `SVY-Term.app` on a macOS runner
- package `SVY-Term-macOS.zip`
- attach the zip file to the GitHub Release for that tag

For non-release checks, you can still run the workflow manually and download the Actions artifact.

## CLI options (initial)

- --newtab [local|ssh] opens a new tab of the selected type
- --exec <command> executes a command in a new local terminal tab
- --config <path> uses an alternate config file path
- --hideterm starts with an empty workspace and hidden side panels

## Next implementation milestones

1. Integrate libssh for production SSH and SFTP
2. Add real split layout and detach tab windows
3. Build graphical SFTP side browser
4. Add SSH tunnel editor and runtime manager UI
5. Implement keyboard shortcuts and multi-exec mode
6. Add plugin API for external protocol/tools
