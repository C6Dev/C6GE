#!/usr/bin/env bash
set -euo pipefail

echo "[DirectEngine] Checking and updating submodules (if necessary)..."
if git submodule status --recursive | grep -q '^[+\-]'; then
  git submodule update --init --recursive
  echo "[DirectEngine] Submodules initialized/updated."
else
  echo "[DirectEngine] Submodules already up-to-date."
fi

echo "[DirectEngine] Detecting package manager..."
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

echo "[DirectEngine] Detected package manager: $PM"

deps_apt=(wayland-protocols libwayland-dev libxkbcommon-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev cmake git pkg-config)
deps_pacman=(wayland-protocols wayland libxkbcommon libx11 libxrandr libxinerama libxcursor libxi mesa cmake git pkgconf)
deps_dnf=(wayland-protocols-devel wayland-devel libxkbcommon-devel libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel mesa-libGL-devel cmake git pkgconf)
deps_zypper=(wayland-protocols-devel libwayland-devel libxkbcommon-devel libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel Mesa-devel cmake git pkgconf)

vulkan_deps_apt_base=(vulkan-tools libvulkan-dev)
vulkan_deps_pacman=(vulkan-tools vulkan-headers vulkan-validation-layers)
vulkan_deps_dnf=(vulkan-tools vulkan-validation-layers vulkan-loader-devel)
vulkan_deps_zypper=(vulkan-tools vulkan-validationlayers libvulkan1)

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

install_vulkan_with_apt(){
  sudo apt-get install -y "${vulkan_deps_apt_base[@]}"
  if ! sudo apt-get install -y vulkan-headers; then
    echo "[DirectEngine] vulkan-headers unavailable; continuing with headers from libvulkan-dev."
  fi
  if ! sudo apt-get install -y vulkan-validationlayers-dev; then
    echo "[DirectEngine] vulkan-validationlayers-dev unavailable; trying vulkan-utility-libraries-dev..."
    sudo apt-get install -y vulkan-utility-libraries-dev || echo "[DirectEngine] Could not install Vulkan validation layers automatically."
  fi
}

install_vulkan_with_pacman(){
  sudo pacman -Sy --noconfirm "${vulkan_deps_pacman[@]}" || true
}

install_vulkan_with_dnf(){
  sudo dnf install -y "${vulkan_deps_dnf[@]}" || true
}

install_vulkan_with_zypper(){
  sudo zypper install -y "${vulkan_deps_zypper[@]}" || true
}

ensure_vulkan(){
  if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists vulkan >/dev/null 2>&1; then
    return 0
  fi

  if [ -n "${VULKAN_SDK:-}" ]; then
    local sdk_include="${VULKAN_SDK}/include/vulkan/vulkan.h"
    local sdk_lib_linux="${VULKAN_SDK}/lib/libvulkan.so"
    local sdk_lib_linux64="${VULKAN_SDK}/lib64/libvulkan.so"
    if [ -f "$sdk_include" ] && { [ -f "$sdk_lib_linux" ] || [ -f "$sdk_lib_linux64" ]; }; then
      return 0
    fi
  fi

  return 1
}

case "$PM" in
  apt)
    echo "[DirectEngine] Installing dependencies via apt..."
    install_with_apt
    ;;
  pacman)
    echo "[DirectEngine] Installing dependencies via pacman..."
    install_with_pacman
    ;;
  dnf)
    echo "[DirectEngine] Installing dependencies via dnf..."
    install_with_dnf
    ;;
  zypper)
    echo "[DirectEngine] Installing dependencies via zypper..."
    install_with_zypper
    ;;
  *)
    echo "[DirectEngine] Unknown package manager. Please install the following packages manually: ${deps_apt[*]}"
    ;;
esac

if ensure_vulkan; then
  echo "[DirectEngine] Vulkan SDK detected."
else
  echo "[DirectEngine] Vulkan SDK not detected. Attempting installation..."
  case "$PM" in
    apt)
      install_vulkan_with_apt
      ;;
    pacman)
      install_vulkan_with_pacman
      ;;
    dnf)
      install_vulkan_with_dnf
      ;;
    zypper)
      install_vulkan_with_zypper
      ;;
    *)
      echo "[DirectEngine] Please install the Vulkan SDK manually: https://vulkan.lunarg.com/sdk/home"
      ;;
  esac

  if ensure_vulkan; then
    echo "[DirectEngine] Vulkan SDK installed successfully."
  else
    echo "[DirectEngine] Vulkan SDK still missing. Please install it manually: https://vulkan.lunarg.com/sdk/home"
  fi
fi

echo "[DirectEngine] Ensuring .NET SDK and CMake are present..."
if ! command -v dotnet >/dev/null 2>&1; then
  echo "[DirectEngine] .NET not found. Attempting best-effort installation..."
  if [ "$PM" = "apt" ]; then
    sudo apt-get install -y dotnet-sdk-8.0 || echo "[DirectEngine] Could not install dotnet-sdk via apt. See: https://learn.microsoft.com/dotnet/core/install/linux"
  elif [ "$PM" = "pacman" ]; then
    sudo pacman -Sy --noconfirm dotnet-sdk || echo "[DirectEngine] Please install .NET SDK 8 via your distro or from https://learn.microsoft.com/dotnet/core/install/"
  elif [ "$PM" = "dnf" ]; then
    sudo dnf install -y dotnet-sdk-8.0 || echo "[DirectEngine] Please install .NET SDK 8 via your distro or from https://learn.microsoft.com/dotnet/core/install/"
  else
    echo "[DirectEngine] Please install .NET SDK 8: https://learn.microsoft.com/dotnet/core/install/linux"
  fi
fi

if ! command -v cmake >/dev/null 2>&1; then
  echo "[DirectEngine] CMake not found. Attempting to install via package manager..."
  case "$PM" in
    apt) sudo apt-get install -y cmake || true ;;
    pacman) sudo pacman -Sy --noconfirm cmake || true ;;
    dnf) sudo dnf install -y cmake || true ;;
    zypper) sudo zypper install -y cmake || true ;;
    *) echo "[DirectEngine] Please install CMake manually: https://cmake.org/download/" ;;
  esac
fi

echo "[DirectEngine] Configuring project (cmake)..."
cmake -S . -B build

echo "[DirectEngine] Building project (Release)..."
cmake --build build --config Release

if command -v ctest >/dev/null 2>&1; then
  echo "[DirectEngine] Running tests..."
  ctest --test-dir build --output-on-failure || true
else
  echo "[DirectEngine] ctest not found; skipping tests."
fi

echo "[DirectEngine] Build finished."
