@echo off
setlocal ENABLEDELAYEDEXPANSION

echo [C6GE] Checking and updating submodules (if necessary)...
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

echo [C6GE] Submodules already up-to-date.
goto :after_submodules

:update_submodules
git submodule update --init --recursive
if errorlevel 1 (
    echo [C6GE] Failed to update submodules.
    exit /b 1
)
echo [C6GE] Submodules initialized/updated.

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
    echo [C6GE] Missing dependencies: %missing%
    echo Please install them manually:
    echo - CMake: https://cmake.org/download/
    echo - .NET SDK 8: https://learn.microsoft.com/dotnet/core/install/windows
    echo - Git: https://git-scm.com/download/win
    exit /b 1
)

echo [C6GE] Configuring project (cmake)...
cmake -S . -B build
if errorlevel 1 (
    echo [C6GE] CMake configure failed.
    exit /b 1
)

echo [C6GE] Building project (Release)...
cmake --build build --config Release
if errorlevel 1 (
    echo [C6GE] Build failed.
    exit /b 1
)

REM Check for ctest by asking CMake where it is, then falling back to PATH
where ctest >nul 2>&1
if errorlevel 1 (
    echo [C6GE] ctest not found; skipping tests.
    goto :end
)

echo [C6GE] Running tests...
ctest --test-dir build --output-on-failure
if errorlevel 1 (
    echo [C6GE] Tests failed.
    exit /b 1
)

:end
echo [C6GE] Build finished.
endlocal
exit /b 0