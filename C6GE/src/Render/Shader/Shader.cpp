#include "Shader.h"

namespace C6GE {

    // Loads a shader file and returns its contents as a const char*

    // Helper to get executable directory (macOS)
    std::string GetExecutableDir() {
        char path[1024];
        uint32_t size = sizeof(path);
        if (_NSGetExecutablePath(path, &size) == 0) {
            return std::string(dirname(path));
        }
        return "";
    }

    const char* LoadShader(const std::string& path) {
        std::string fullPath = path;
        // If path is not absolute, prepend executable dir
        if (!path.empty() && path[0] != '/') {
            fullPath = GetExecutableDir() + "/" + path;
        }
        std::ifstream file(fullPath, std::ios::in | std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            Log(LogLevel::error, "Failed to open shader file: " + fullPath);
            return nullptr;
        }
        std::streamsize size = file.tellg();
        if (size <= 0) {
            Log(LogLevel::error, "Shader file is empty or unreadable: " + fullPath);
            file.close();
            return nullptr;
        }
        file.seekg(0, std::ios::beg);
        char* buffer = new char[size + 1];
        if (file.read(buffer, size)) {
            buffer[size] = '\0';
            file.close();
            return buffer;
        } else {
            Log(LogLevel::error, "Failed to read shader file: " + fullPath);
        }
        delete[] buffer;
        file.close();
        return nullptr;
    }
}