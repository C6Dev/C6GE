# C6GE Windows Build Script
# Powershell required

Write-Host "[C6GE] Checking and updating submodules (if necessary)..."
$submoduleStatus = git submodule status --recursive
if ($submoduleStatus -match '^[+-]') {
    git submodule update --init --recursive
    Write-Host "[C6GE] Submodules initialized/updated."
} else {
    Write-Host "[C6GE] Submodules already up-to-date."
}

# Dependency check
$missing = @()
$deps = @('cmake', 'dotnet', 'git')
foreach ($dep in $deps) {
    if (-not (Get-Command $dep -ErrorAction SilentlyContinue)) {
        $missing += $dep
    }
}
if ($missing.Count -gt 0) {
    Write-Host "[C6GE] Missing dependencies: $($missing -join ', ')"
    Write-Host "Please install them manually:"
    Write-Host "- CMake: https://cmake.org/download/"
    Write-Host "- .NET SDK 8: https://learn.microsoft.com/dotnet/core/install/windows"
    Write-Host "- Git: https://git-scm.com/download/win"
    exit 1
}

Write-Host "[C6GE] Configuring project (cmake)..."
cmake -S . -B build

Write-Host "[C6GE] Building project (Release)..."
cmake --build build --config Release

if (Get-Command ctest -ErrorAction SilentlyContinue) {
    Write-Host "[C6GE] Running tests..."
    ctest --test-dir build --output-on-failure
} else {
    Write-Host "[C6GE] ctest not found; skipping tests."
}

Write-Host "[C6GE] Build finished."
