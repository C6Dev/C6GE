#!/usr/bin/env bash
set -euo pipefail

echo "[C6GE] Checking and updating submodules (if necessary)..."
if git submodule status --recursive | grep -q '^[+\-]'; then
  git submodule update --init --recursive
  echo "[C6GE] Submodules initialized/updated."
else
  echo "[C6GE] Submodules already up-to-date."
fi

if ! command -v brew >/dev/null 2>&1; then
  echo "[C6GE] Homebrew not found. Install Homebrew from https://brew.sh/ and re-run this script."
  exit 1
fi

echo "[C6GE] Updating Homebrew and installing dependencies..."
brew update || true
brew install cmake || true
# Try dotnet SDK via formula first, fall back to cask if needed
if ! brew list --formula | grep -q '^dotnet-sdk$'; then
  brew install dotnet-sdk || brew install --cask dotnet || true
fi

if ! command -v dotnet >/dev/null 2>&1; then
  echo "[C6GE] Could not install .NET SDK automatically. See: https://learn.microsoft.com/dotnet/core/install/macos"
fi

echo "[C6GE] Configuring project (cmake)..."
cmake -S . -B build

echo "[C6GE] Building project (Release)..."
cmake --build build --config Release

if command -v ctest >/dev/null 2>&1; then
  echo "[C6GE] Running tests..."
  ctest --test-dir build --output-on-failure || true
else
  echo "[C6GE] ctest not found; skipping tests."
fi

echo "[C6GE] Build finished."
