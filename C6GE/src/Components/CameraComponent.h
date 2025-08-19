#pragma once
#include "TransformComponent.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace C6GE {

    struct CameraComponent {
        TransformComponent Transform;
        float MovementSpeed = 5.0f;
        float MouseSensitivity = 0.1f;
    };

    CameraComponent* CreateCamera();

    glm::vec3 GetCameraFront(const TransformComponent& t);
    glm::mat4 GetViewMatrix(const CameraComponent& cam);
} // namespace C6GE