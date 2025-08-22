#include "TransformComponent.h"
#include <glm/gtc/matrix_transform.hpp>

glm::mat4 TransformComponent::GetModelMatrix() const {
    glm::mat4 model = glm::mat4(1.0f);
    
    // Apply transformations in correct order: Scale -> Rotate -> Translate
    model = glm::scale(model, Scale);
    
    // Convert Euler angles to radians and apply rotation
    glm::vec3 rotationRadians = glm::radians(Rotation);
    model = glm::rotate(model, rotationRadians.x, glm::vec3(1.0f, 0.0f, 0.0f)); // X-axis
    model = glm::rotate(model, rotationRadians.y, glm::vec3(0.0f, 1.0f, 0.0f)); // Y-axis
    model = glm::rotate(model, rotationRadians.z, glm::vec3(0.0f, 0.0f, 1.0f)); // Z-axis
    
    model = glm::translate(model, Position);
    
    return model;
}

glm::vec3 TransformComponent::GetForward() const {
    // Calculate forward vector from rotation
    glm::vec3 rotationRadians = glm::radians(Rotation);
    
    // Create rotation matrix for Y-axis rotation (yaw)
    glm::mat4 rotationMatrix = glm::mat4(1.0f);
    rotationMatrix = glm::rotate(rotationMatrix, rotationRadians.y, glm::vec3(0.0f, 1.0f, 0.0f));
    
    // Transform the forward vector (0, 0, -1) by the rotation matrix
    glm::vec4 forward = rotationMatrix * glm::vec4(0.0f, 0.0f, -1.0f, 1.0f);
    return glm::normalize(glm::vec3(forward));
}

glm::vec3 TransformComponent::GetRight() const {
    // Right vector is perpendicular to forward and up
    glm::vec3 forward = GetForward();
    glm::vec3 up = GetUp();
    return glm::normalize(glm::cross(forward, up));
}

glm::vec3 TransformComponent::GetUp() const {
    // Calculate up vector from rotation
    glm::vec3 rotationRadians = glm::radians(Rotation);
    
    // Create rotation matrix for X-axis rotation (pitch)
    glm::mat4 rotationMatrix = glm::mat4(1.0f);
    rotationMatrix = glm::rotate(rotationMatrix, rotationRadians.x, glm::vec3(1.0f, 0.0f, 0.0f));
    
    // Transform the up vector (0, 1, 0) by the rotation matrix
    glm::vec4 up = rotationMatrix * glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
    return glm::normalize(glm::vec3(up));
}
