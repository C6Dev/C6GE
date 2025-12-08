#pragma once
#include "../main.h"
#include <string>
#include <filesystem>
#include <fstream>

#include "Logger.h"

class DirectEngine_API Projects
{
    public:
        void CreateProject(const std::string& name, const std::string& location, const std::string& templlate);
        void OpenProject(const std::string& path, const std::string& name);
        void BuildProject(const std::string& path, const std::string& name, const std::string& ExacutableName);
        void CloseProject();
};