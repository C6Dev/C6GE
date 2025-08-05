#pragma once
#include <glm/glm.hpp>

namespace C6GE {
    struct LightComponent {
        glm::vec3 color;
        float intensity;
        LightComponent(glm::vec3 col = glm::vec3(1.0f), float intens = 1.0f) : color(col), intensity(intens) {}
    };
}