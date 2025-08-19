#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <glad/glad.h>

struct InstanceComponent {
    std::vector<glm::mat4> instances;
    GLuint instanceVBO = 0;
};