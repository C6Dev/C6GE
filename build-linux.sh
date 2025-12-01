#!/usr/bin/env bash
set -euo pipefail

echo "[C6GE] Checking and updating submodules (if necessary)..."
if git submodule status --recursive | grep -q '^[+\-]'; then
  git submodule update --init --recursive
  echo "[C6GE] Submodules initialized/updated."
else
  echo "[C6GE] Submodules already up-to-date."
fi

echo "[C6GE] Detecting package manager..."
PM=unknown
if command -v apt-get >/dev/null 2>&1; then
  PM=apt
elif command -v pacman >/dev/null 2>&1; then
  PM=pacman
elif command -v dnf >/dev/null 2>&1; then
  PM=dnf
elif command -v zypper >/dev/null 2>&1; then
  PM=zypper
fi

echo "[C6GE] Detected package manager: $PM"

deps_apt=(wayland-protocols libwayland-dev libxkbcommon-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev cmake git)
deps_pacman=(wayland-protocols wayland libxkbcommon libx11 libxrandr libxinerama libxcursor libxi mesa cmake git)
deps_dnf=(wayland-protocols-devel wayland-devel libxkbcommon-devel libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel mesa-libGL-devel cmake git)
deps_zypper=(wayland-protocols-devel libwayland-devel libxkbcommon-devel libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel Mesa-devel cmake git)

install_with_apt(){
  sudo apt-get update
  sudo apt-get install -y "${deps_apt[@]}"
}

install_with_pacman(){
  sudo pacman -Sy --noconfirm "${deps_pacman[@]}" || true
}

install_with_dnf(){
  sudo dnf install -y "${deps_dnf[@]}" || true
}

install_with_zypper(){
  sudo zypper install -y "${deps_zypper[@]}" || true
}

case "$PM" in
  apt)
    echo "[C6GE] Installing dependencies via apt..."
    install_with_apt
    ;;
  pacman)
    echo "[C6GE] Installing dependencies via pacman..."
    install_with_pacman
    ;;
  dnf)
    echo "[C6GE] Installing dependencies via dnf..."
    install_with_dnf
    ;;
  zypper)
    echo "[C6GE] Installing dependencies via zypper..."
    install_with_zypper
    ;;
  *)
    echo "[C6GE] Unknown package manager. Please install the following packages manually: ${deps_apt[*]}"
    ;;
esac

echo "[C6GE] Ensuring .NET SDK and CMake are present..."
if ! command -v dotnet >/dev/null 2>&1; then
  echo "[C6GE] .NET not found. Attempting best-effort installation..."
  if [ "$PM" = "apt" ]; then
    sudo apt-get install -y dotnet-sdk-8.0 || echo "[C6GE] Could not install dotnet-sdk via apt. See: https://learn.microsoft.com/dotnet/core/install/linux"
  elif [ "$PM" = "pacman" ]; then
    sudo pacman -Sy --noconfirm dotnet-sdk || echo "[C6GE] Please install .NET SDK 8 via your distro or from https://learn.microsoft.com/dotnet/core/install/"
  elif [ "$PM" = "dnf" ]; then
    sudo dnf install -y dotnet-sdk-8.0 || echo "[C6GE] Please install .NET SDK 8 via your distro or from https://learn.microsoft.com/dotnet/core/install/"
  else
    echo "[C6GE] Please install .NET SDK 8: https://learn.microsoft.com/dotnet/core/install/linux"
  fi
fi

if ! command -v cmake >/dev/null 2>&1; then
  echo "[C6GE] CMake not found. Attempting to install via package manager..."
  case "$PM" in
    apt) sudo apt-get install -y cmake || true ;;
    pacman) sudo pacman -Sy --noconfirm cmake || true ;;
    dnf) sudo dnf install -y cmake || true ;;
    zypper) sudo zypper install -y cmake || true ;;
    *) echo "[C6GE] Please install CMake manually: https://cmake.org/download/" ;;
  esac
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
