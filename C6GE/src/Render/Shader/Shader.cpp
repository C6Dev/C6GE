#include "Shader.h"
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#endif

#include <glad/glad.h>

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

    GLuint CompileShader(const char* ShaderSource, ShaderType shaderType) {
        GLuint shaderID = 0;

        switch (shaderType) {
            case ShaderType::Vertex:
                shaderID = glCreateShader(GL_VERTEX_SHADER);
                break;
            case ShaderType::Fragment:
                shaderID = glCreateShader(GL_FRAGMENT_SHADER);
                break;
            case ShaderType::Geometry:
                shaderID = glCreateShader(GL_GEOMETRY_SHADER);
                break;
            default:
                Log(LogLevel::error, "Invalid shader type");
                return 0;
        }

        if (shaderID == 0) {
            Log(LogLevel::error, "Failed to create shader");
            return 0;
        }

        glShaderSource(shaderID, 1, &ShaderSource, nullptr);
        glCompileShader(shaderID);

        GLint success;
        glGetShaderiv(shaderID, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shaderID, 512, nullptr, infoLog);
            Log(LogLevel::error, std::string("Shader compilation failed: ") + infoLog);
            glDeleteShader(shaderID);
            return 0;
        }

        return shaderID;
    }

	GLuint CreateProgram(GLuint VertexShader, GLuint FragmentShader, GLuint GeometryShader) {
	GLuint Program = glCreateProgram();
        glAttachShader(Program, VertexShader);
        glAttachShader(Program, FragmentShader);
        if (GeometryShader != 0) {
            glAttachShader(Program, GeometryShader);
        }
        glLinkProgram(Program);

        GLint success;
        glGetProgramiv(Program, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(Program, 512, nullptr, infoLog);
            Log(LogLevel::error, std::string("Program linking failed: ") + infoLog);
            glDeleteProgram(Program);
            return 0;
        }

        glDeleteShader(VertexShader);
        glDeleteShader(FragmentShader);
        if (GeometryShader != 0) {
            glDeleteShader(GeometryShader);
        }

        return Program;
	}

    GLuint UseProgram(GLuint Program) {
        glUseProgram(Program);
        return Program;
    }

     // Sets a uniform vec3 in the shader
    void SetShaderUniformVec3(GLuint program, const std::string& name, const glm::vec3& value) {
        GLint location = glGetUniformLocation(program, name.c_str());
        if (location == -1) {
            Log(LogLevel::warning, "Uniform '" + name + "' not found in shader");
            return;
        }
        glUniform3fv(location, 1, glm::value_ptr(value));
    }

    // Sets a uniform float in the shader
    void SetShaderUniformFloat(GLuint program, const std::string& name, float value) {
        GLint location = glGetUniformLocation(program, name.c_str());
        if (location == -1) {
            Log(LogLevel::warning, "Uniform '" + name + "' not found in shader");
            return;
        }
        glUniform1f(location, value);
    }

    // Sets a uniform int in the shader
    void SetShaderUniformInt(GLuint program, const std::string& name, int value) {
        GLint location = glGetUniformLocation(program, name.c_str());
        if (location == -1) {
            Log(LogLevel::warning, "Uniform '" + name + "' not found in shader");
            return;
        }
        glUniform1i(location, value);
    }
}