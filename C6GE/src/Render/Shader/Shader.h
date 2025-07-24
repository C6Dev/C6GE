#include <string>
#include <fstream>
#include <ios>
#include <mach-o/dyld.h>
#include <libgen.h>
#include "Logging/Log.h"


enum ShaderType {
    Vertex,
    Fragment
};

namespace C6GE {
    using GLuint = unsigned int;
    // Loads a shader file and returns its contents as a const char*
    const char* LoadShader(const std::string& path);
    GLuint CompileShader(const char* Shader, ShaderType ShaderType);
}