#pragma once
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/type_ptr.hpp>

namespace C6GE {

        struct TransformComponent {
        glm::vec3 Position = glm::vec3(0.0f);
        glm::vec3 Rotation = glm::vec3(0.0f); // In degrees
        glm::vec3 Scale    = glm::vec3(1.0f);
    };

}