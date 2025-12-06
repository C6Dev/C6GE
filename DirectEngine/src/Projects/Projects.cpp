#include "../../include/Projects/Projects.h"

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace
{
    void EnsureDirectories(const fs::path& path)
    {
        std::error_code ec;
        if (!fs::create_directories(path, ec) && ec) {
            throw std::runtime_error("Failed to create directory: " + path.string() + " (" + ec.message() + ")");
        }
    }
}


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

    for (const auto& subdir : subdirectories) {
        EnsureDirectories(projectPath / subdir);
    }

    // Generate project file
    std::ofstream projectFile(projectPath / (name + ".deproj"), std::ios::out | std::ios::trunc);
    if (!projectFile.is_open()) {
        throw std::runtime_error("Failed to create project file");
    }
    projectFile << "ProjectName: " << name << "\n";
    projectFile << "EngineVersion: 1.0.0\n";
    projectFile << "DefaultScene: null\n";
    projectFile.close();
}

void Projects::OpenProject(const std::string& path, const std::string& name)
{
    const fs::path projectPath = fs::path(path);
    if (!fs::exists(projectPath) || !fs::is_regular_file(projectPath)) {
        throw std::runtime_error("Project file does not exist: " + projectPath.string());
    }

    // Read project name .deproj file
    std::ifstream projectFile(projectPath, std::ios::in);
    if (!projectFile.is_open()) {
        throw std::runtime_error("Failed to open project file: " + projectPath.string());
    }

    // read project name and save to variable
    std::string line;
    std::string engineVersion;
    std::string defaultScene;
    while (std::getline(projectFile, line)) {
        if (line.find("ProjectName: ") == 0) {
            std::string projectName = line.substr(strlen("ProjectName: "));
            if (projectName != name) {
                throw std::runtime_error("Project name mismatch: expected " + name + ", found " + projectName);
            }
            // Do not break here, continue to read engineVersion and defaultScene
        }

        // get engiine version and save to variable
        else if (line.find("EngineVersion: ") == 0) {
            engineVersion = line.substr(strlen("EngineVersion: "));
            // For now, we just print the engine version
            // In a real implementation, you might want to check compatibility
            printf("Opening project with engine version: %s\n", engineVersion.c_str());
        }

        // get default scene and save to variable
        else if (line.find("DefaultScene: ") == 0) {
            defaultScene = line.substr(strlen("DefaultScene: "));
            // For now, we just print the default scene
            // In a real implementation, you might want to load this scene
            printf("Default scene: %s\n", defaultScene.c_str());
        }
    }

    projectFile.close();

    // print project name engine version and default scene
    printf("Project '%s' opened successfully from %s\n", name.c_str(), projectPath.string().c_str());
    printf("Project Name: %s\n", name.c_str());
    printf("Project Path: %s\n", projectPath.string().c_str());
    printf("Engine Version: %s\n", engineVersion.c_str());
    printf("Default Scene: %s\n", defaultScene.c_str());
}

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