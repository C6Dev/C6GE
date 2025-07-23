#include "Shader.h"
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#endif

namespace C6GE {

    // Loads a shader file and returns its contents as a const char*

    // Helper to get executable directory
    std::string GetExecutableDir() {
#ifdef __APPLE__
        char path[1024];
        uint32_t size = sizeof(path);
        if (_NSGetExecutablePath(path, &size) == 0) {
            return std::string(dirname(path));
        }
        return "";
#elif defined(__linux__)
        char path[1024];
        ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
        if (len != -1) {
            path[len] = '\0';
            return std::string(dirname(path));
        }
        return "";
#elif defined(_WIN32)
        char path[MAX_PATH];
        GetModuleFileNameA(NULL, path, MAX_PATH);
        
        // Find the last backslash
        char* lastSlash = std::strrchr(path, '\\');
        if (lastSlash) {
            *lastSlash = '\0'; // Truncate at the last backslash
        }
        
        return std::string(path);
#else
        return ""; // Fallback for other platforms
#endif
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