#pragma once

#include <vector>
#include <glm/glm.hpp>
// GLAD include removed - using bgfx for OpenGL context management
// OpenGL type definitions
using GLuint = unsigned int;

struct InstanceComponent {
    std::vector<glm::mat4> instances;
    GLuint instanceVBO = 0;
};