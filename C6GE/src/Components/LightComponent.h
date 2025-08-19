#pragma once
#include <glm/glm.hpp>

namespace C6GE {
    enum LightType { Point, Directional, Spot };

    struct LightComponent {
        LightType type;
        glm::vec3 color;
        float intensity;
        glm::vec3 direction; // For Directional and Spot
        float cutoff; // For Spot (cosine of angle)
        LightComponent(LightType t = Point, glm::vec3 col = glm::vec3(1.0f), float intens = 1.0f, glm::vec3 dir = glm::vec3(0.0f, -1.0f, 0.0f), float cut = 12.5f) 
            : type(t), color(col), intensity(intens), direction(dir), cutoff(glm::cos(glm::radians(cut))) {}
    };
} // namespace C6GE