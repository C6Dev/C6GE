# C6GE Build Scripts

This folder contains build scripts for Linux, macOS, and Windows.

## Usage

- **Linux:**
  ```sh
  ./scripts/build-linux.sh
  ```
- **macOS:**
  ```sh
  ./scripts/build-macos.sh
  ```
- **Windows:**
  ```powershell
  ./scripts/build-windows.ps1
  ```

Each script will:
- Ensure all git submodules are initialized and updated
- Check for required dependencies and install (or prompt for manual install)
- Configure and build the project using CMake
- Run tests if possible

## Dependencies
- CMake
- .NET SDK 8
- Git
- Platform-specific libraries (see script for details)

If a dependency cannot be installed automatically, the script will provide instructions.
