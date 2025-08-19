#include <string>
#include <fstream>
#include <ios>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <libgen.h>
#elif defined(__linux__)
#include <unistd.h>
#include <libgen.h>
#elif defined(_WIN32)
#include <windows.h>
#endif
#include "Logging/Log.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>


enum ShaderType {
    Vertex,
    Fragment,
    Geometry
};

namespace C6GE {
    using GLuint = unsigned int;
    // Loads a shader file and returns its contents as a const char*
    const char* LoadShader(const std::string& path);
    GLuint CompileShader(const char* Shader, ShaderType ShaderType);
    GLuint CreateProgram(GLuint VertexShader, GLuint FragmentShader, GLuint GeometryShader = 0);
    GLuint UseProgram(GLuint Program);
    void SetShaderUniformVec3(GLuint Program, const std::string& name, const glm::vec3& value);
    void SetShaderUniformFloat(GLuint Program, const std::string& name, float value);
    void SetShaderUniformInt(GLuint Program, const std::string& name, int value);
}