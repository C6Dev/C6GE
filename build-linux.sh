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

deps_apt=(wayland-protocols libwayland-dev libxkbcommon-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev cmake git pkg-config)
deps_pacman=(wayland-protocols wayland libxkbcommon libx11 libxrandr libxinerama libxcursor libxi mesa cmake git pkgconf)
deps_dnf=(wayland-protocols-devel wayland-devel libxkbcommon-devel libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel mesa-libGL-devel cmake git pkgconf)
deps_zypper=(wayland-protocols-devel libwayland-devel libxkbcommon-devel libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel Mesa-devel cmake git pkgconf)

vulkan_deps_apt_base=(vulkan-tools libvulkan-dev mesa-vulkan-drivers vulkan-validationlayers)
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
    echo "[C6GE] vulkan-headers unavailable; continuing with headers from libvulkan-dev."
  fi
  if ! sudo apt-get install -y vulkan-validationlayers-dev; then
    echo "[C6GE] vulkan-validationlayers-dev unavailable; trying vulkan-utility-libraries-dev..."
    sudo apt-get install -y vulkan-utility-libraries-dev || echo "[C6GE] Could not install Vulkan validation layers automatically."
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

install_validation_layers_with_apt(){
  local candidates=(vulkan-validationlayers vulkan-validationlayers-dev vulkan-utility-libraries vulkan-utility-libraries-dev)
  for pkg in "${candidates[@]}"; do
    if sudo apt-get install -y "$pkg"; then
      return 0
    fi
  done
  return 1
}

install_validation_layers_with_pacman(){
  sudo pacman -Sy --noconfirm vulkan-validation-layers || true
}

install_validation_layers_with_dnf(){
  sudo dnf install -y vulkan-validation-layers || true
}

install_validation_layers_with_zypper(){
  sudo zypper install -y vulkan-validationlayers || true
}

have_validation_layers(){
  if command -v vulkaninfo >/dev/null 2>&1; then
    if vulkaninfo --summary 2>/dev/null | grep -q "VK_LAYER_KHRONOS_validation"; then
      return 0
    fi
  fi

  local manifest_dirs=(/usr/share/vulkan/explicit_layer.d /etc/vulkan/explicit_layer.d)
  for dir in "${manifest_dirs[@]}"; do
    if [ -d "$dir" ] && ls "$dir"/VK_LAYER_KHRONOS_validation*.json >/dev/null 2>&1; then
      return 0
    fi
  done

  return 1
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

if ensure_vulkan; then
  echo "[C6GE] Vulkan SDK detected."
else
  echo "[C6GE] Vulkan SDK not detected. Attempting installation..."
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
      echo "[C6GE] Please install the Vulkan SDK manually: https://vulkan.lunarg.com/sdk/home"
      ;;
  esac

  if ensure_vulkan; then
    echo "[C6GE] Vulkan SDK installed successfully."
  else
    echo "[C6GE] Vulkan SDK still missing. Please install it manually: https://vulkan.lunarg.com/sdk/home"
  fi
fi

if have_validation_layers; then
  echo "[C6GE] Vulkan validation layers detected."
else
  echo "[C6GE] Vulkan validation layers not detected. Attempting installation..."
  case "$PM" in
    apt)
      install_validation_layers_with_apt || true
      ;;
    pacman)
      install_validation_layers_with_pacman
      ;;
    dnf)
      install_validation_layers_with_dnf
      ;;
    zypper)
      install_validation_layers_with_zypper
      ;;
    *)
      echo "[C6GE] Please install the Vulkan validation layers manually (VK_LAYER_KHRONOS_validation)."
      ;;
  esac

  if have_validation_layers; then
    echo "[C6GE] Vulkan validation layers installed successfully."
  else
    echo "[C6GE] Vulkan validation layers still missing. Install instructions: https://vulkan.lunarg.com/sdk/home"
  fi
fi

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
