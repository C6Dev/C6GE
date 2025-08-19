#include "CameraComponent.h"

namespace C6GE {

    CameraComponent* CreateCamera() {
        CameraComponent* cam = new CameraComponent();
        cam->Transform.Position = glm::vec3(0.0f, 0.0f, 3.0f);
        cam->Transform.Rotation = glm::vec3(0.0f);
        return cam;
    }

    glm::vec3 GetCameraFront(const TransformComponent& t) {
        float yaw = glm::radians(t.Rotation.y);
        float pitch = glm::radians(t.Rotation.x);
        glm::vec3 front;
        front.x = cos(pitch) * sin(yaw);
        front.y = sin(pitch);
        front.z = -cos(pitch) * cos(yaw);
        return glm::normalize(front);
    }

    glm::mat4 GetViewMatrix(const CameraComponent& cam) {
        glm::vec3 front = GetCameraFront(cam.Transform);
        glm::vec3 pos = cam.Transform.Position;
        return glm::lookAt(pos, pos + front, glm::vec3(0.0f, 1.0f, 0.0f));
    }
} // namespace C6GE
