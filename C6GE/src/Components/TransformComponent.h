#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class TransformComponent {
public:
    glm::vec3 Position = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 Rotation = glm::vec3(0.0f, 0.0f, 0.0f); // Euler angles in degrees
    glm::vec3 Scale = glm::vec3(1.0f, 1.0f, 1.0f);
    
    TransformComponent() = default;
    TransformComponent(const glm::vec3& position, const glm::vec3& rotation = glm::vec3(0.0f), const glm::vec3& scale = glm::vec3(1.0f))
        : Position(position), Rotation(rotation), Scale(scale) {}
    
    // Get the model matrix for this transform
    glm::mat4 GetModelMatrix() const;
    
    // Utility functions
    void SetPosition(const glm::vec3& position) { Position = position; }
    void SetRotation(const glm::vec3& rotation) { Rotation = rotation; }
    void SetScale(const glm::vec3& scale) { Scale = scale; }
    
    void Translate(const glm::vec3& offset) { Position += offset; }
    void Rotate(const glm::vec3& rotation) { Rotation += rotation; }
    void ScaleBy(const glm::vec3& scale) { Scale *= scale; }
    
    // Get forward, right, and up vectors
    glm::vec3 GetForward() const;
    glm::vec3 GetRight() const;
    glm::vec3 GetUp() const;
};