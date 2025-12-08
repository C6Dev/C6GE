#include "../../include/Projects/Projects.h"

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

using namespace DirectLogger;

namespace fs = std::filesystem;

namespace // anonymous namespace
{
    void EnsureDirectories(const fs::path& path)
    {
        std::error_code ec;
        if (!fs::create_directories(path, ec) && ec) {
            Log(LogLevel::critical, "Failed to create directory: " + path.string() + " (" + ec.message() + ")", "Projects");
            throw;
        }
    }
} // namespace


// The CreateProject method creates a new project with the specified name, location, and template.
void Projects::CreateProject(const std::string& name, const std::string& location, const std::string& templlate)
{
    // Target path: <location>/<name>
    const fs::path projectPath = fs::path(location) / name;
    (void)templlate; // Template selection not implemented yet

    // Make the root directory
    EnsureDirectories(projectPath);

    // make subfolders
    const std::array<const char*, 8> subdirectories = {
        "Scripts",
        "Assets",
        "Assets/Textures",
        "Assets/Models",
        "Assets/Audio",
        "Scenes",
        "EditorSettings",
        "GameSettings"
    };

    // Create subdirectories
    for (const auto& subdir : subdirectories) {
        EnsureDirectories(projectPath / subdir);
    }

    // Generate project file
    std::ofstream projectFile(projectPath / (name + ".deproj"), std::ios::out | std::ios::trunc);
    if (!projectFile.is_open()) {
        Log(LogLevel::critical, "Failed to create project file: " + (projectPath / (name + ".deproj")).string(), "Projects");
        throw;
    }
    projectFile << "ProjectName: " << name << "\n";
    projectFile << "EngineVersion: 1.0.0\n";
    projectFile << "DefaultScene: null\n";
    projectFile.close();
}

// The OpenProject method opens an existing project from the specified path with the given name.
void Projects::OpenProject(const std::string& path, const std::string& name)
{
    // Verify that the project file exists
    const fs::path projectPath = fs::path(path);
    if (!fs::exists(projectPath) || !fs::is_regular_file(projectPath)) {
        Log(LogLevel::critical, "Project file does not exist: " + projectPath.string(), "Projects");
        throw;
    }

    // Read project name .deproj file
    std::ifstream projectFile(projectPath, std::ios::in);
    if (!projectFile.is_open()) {
        Log(LogLevel::critical, "Failed to open project file: " + projectPath.string(), "Projects");
        throw;
    }

    // read project name and save to variable
    std::string line;
    std::string engineVersion;
    std::string defaultScene;
    while (std::getline(projectFile, line)) {
        if (line.find("ProjectName: ") == 0) {
            std::string projectName = line.substr(strlen("ProjectName: "));
            if (projectName != name) {
                Log(LogLevel::critical, "Project name mismatch: expected " + name + ", found " + projectName, "Projects");
                throw;
            }
            // Do not break here, continue to read engineVersion and defaultScene
        }

        // get engiine version and save to variable
        else if (line.find("EngineVersion: ") == 0) {
            engineVersion = line.substr(strlen("EngineVersion: "));
            // For now, we just print the engine version
            // In a real implementation, you might want to check compatibility
            Log(LogLevel::info, "Engine version: " + engineVersion, "Projects");
        }

        // get default scene and save to variable
        else if (line.find("DefaultScene: ") == 0) {
            defaultScene = line.substr(strlen("DefaultScene: "));
            // For now, we just print the default scene
            // In a real implementation, you might want to load this scene
            Log(LogLevel::info, "Default scene: " + defaultScene, "Projects");
        }
    }

    // Close the project file after reading
    projectFile.close();

    // print project name engine version and default scene
    Log(LogLevel::info, "Project opened successfully", "Projects");
    Log(LogLevel::info, "Project Name: " + name, "Projects");
    Log(LogLevel::info, "Project Path: " + projectPath.string(), "Projects");
    Log(LogLevel::info, "Engine Version: " + engineVersion, "Projects");
    Log(LogLevel::info, "Default Scene: " + defaultScene, "Projects");
}


// The BuildProject method builds the project located at the specified path with the given name and executable name.
void Projects::BuildProject(const std::string& path, const std::string& name, const std::string& ExacutableName)
{
    // Compile Project Files To .Pak File TODO
    
    // Copy DirectRuntime executable to the project's build folder DirectRuntime is in the same location as the current executable and Rename The Copied .exe to the executable name
    fs::path currentExePath = fs::current_path();
    fs::path runtimeExePath = currentExePath / "DirectRuntime.exe";
    fs::path buildPath = fs::path(path) / "Build";
    EnsureDirectories(buildPath);
    fs::path targetRuntimePath = buildPath / (ExacutableName + ".exe");
    fs::copy_file(runtimeExePath, targetRuntimePath, fs::copy_options::overwrite_existing);

    // For Now We Will Also Copy DirectRuntime .exe to the main non build folder And Rename the Copied .exe to the executable name
    fs::path targetRuntimePathMain = fs::path(path) / (ExacutableName + ".exe");
    fs::copy_file(runtimeExePath, targetRuntimePathMain, fs::copy_options::overwrite_existing);

    // Copy The DirectEngine.dll to the build folder
    fs::path engineDllPath = currentExePath / "DirectEngine.dll";
    fs::path targetEngineDllPath = buildPath / "DirectEngine.dll";
    fs::copy_file(engineDllPath, targetEngineDllPath, fs::copy_options::overwrite_existing);

    // For Now We Will Also Copy DirectEngine.dll to the main non build folder
    fs::path targetEngineDllPathMain = fs::path(path) / "DirectEngine.dll";
    fs::copy_file(engineDllPath, targetEngineDllPathMain, fs::copy_options::overwrite_existing);
}

void Projects::CloseProject()
{
    // Implementation for closing a project would go here
}