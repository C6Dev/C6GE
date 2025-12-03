@echo off
setlocal ENABLEDELAYEDEXPANSION

echo [DirectEngine] Checking and updating submodules (if necessary)...
for /f "usebackq delims=" %%A in (`git submodule status --recursive`) do (
    set "submoduleStatus=%%A"
    goto :check_submodule_done
)
:check_submodule_done

if defined submoduleStatus (
    set "firstChar=!submoduleStatus:~0,1!"
    if "!firstChar!"=="+" goto :update_submodules
    if "!firstChar!"=="-" goto :update_submodules
)

echo [DirectEngine] Submodules already up-to-date.
goto :after_submodules

:update_submodules
git submodule update --init --recursive
if errorlevel 1 (
    echo [DirectEngine] Failed to update submodules.
    exit /b 1
)
echo [DirectEngine] Submodules initialized/updated.

:after_submodules

REM Dependency check
set "missing="

where cmake >nul 2>&1
if errorlevel 1 (
    if defined missing (
        set "missing=%missing%, cmake"
    ) else (
        set "missing=cmake"
    )
)

where dotnet >nul 2>&1
if errorlevel 1 (
    if defined missing (
        set "missing=%missing%, dotnet"
    ) else (
        set "missing=dotnet"
    )
)

where git >nul 2>&1
if errorlevel 1 (
    if defined missing (
        set "missing=%missing%, git"
    ) else (
        set "missing=git"
    )
)

if defined missing (
    echo [DirectEngine] Missing dependencies: %missing%
    echo Please install them manually:
    echo - CMake: https://cmake.org/download/
    echo - .NET SDK 8: https://learn.microsoft.com/dotnet/core/install/windows
    echo - Git: https://git-scm.com/download/win
    exit /b 1
)

call :check_vulkan
if errorlevel 1 (
    call :ensure_vulkan
    call :check_vulkan
    if errorlevel 1 (
        echo [DirectEngine] Vulkan SDK is still missing. Please install it manually from https://vulkan.lunarg.com/sdk/home and re-run this script.
        exit /b 1
    )
)

echo [DirectEngine] Configuring project (cmake)...
cmake -S . -B build
if errorlevel 1 (
    echo [DirectEngine] CMake configure failed.
    exit /b 1
)

echo [DirectEngine] Building project (Release)...
cmake --build build
if errorlevel 1 (
    echo [DirectEngine] Build failed.
    exit /b 1
)

REM Check for ctest by asking CMake where it is, then falling back to PATH
where ctest >nul 2>&1
if errorlevel 1 (
    echo [DirectEngine] ctest not found; skipping tests.
    goto :end
)

echo [DirectEngine] Running tests...
ctest --test-dir build --output-on-failure
if errorlevel 1 (
    echo [DirectEngine] Tests failed.
    exit /b 1
)

:end
echo [DirectEngine] Build finished.
endlocal
exit /b 0

:check_vulkan
where vulkaninfo >nul 2>&1
if not errorlevel 1 (
    echo [DirectEngine] Vulkan SDK detected via PATH.
    exit /b 0
)

if defined VULKAN_SDK (
    if exist "%VULKAN_SDK%\Bin\vulkaninfo.exe" (
        echo [DirectEngine] Vulkan SDK detected via VULKAN_SDK.
        exit /b 0
    )
)

exit /b 1

:ensure_vulkan
echo [DirectEngine] Vulkan SDK not detected. Checking for winget...
where winget >nul 2>&1
if errorlevel 1 (
    echo [DirectEngine] winget not available; install Vulkan SDK manually from https://vulkan.lunarg.com/sdk/home
    exit /b 1
)

echo [DirectEngine] Attempting Vulkan SDK installation via winget (LunarG.VulkanSDK)...
winget install --id LunarG.VulkanSDK --exact --source winget --silent --accept-package-agreements --accept-source-agreements
if errorlevel 1 (
    echo [DirectEngine] Automatic installation failed. Please download the installer from https://vulkan.lunarg.com/sdk/home
    exit /b 1
)

echo [DirectEngine] Vulkan SDK installation requested via winget. You may need to accept license prompts in the pop-up window.
exit /b 0