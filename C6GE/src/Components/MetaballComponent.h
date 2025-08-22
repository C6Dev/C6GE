#pragma once
#include <glm/glm.hpp>
#include <vector>

namespace C6GE {
    struct MetaballComponent {
        glm::vec3 position;
        glm::vec4 color;
        float radius;
        float strength;
        glm::vec3 velocity;
        
        MetaballComponent() 
            : position(0.0f), color(1.0f, 0.0f, 0.0f, 1.0f), radius(1.0f), strength(1.0f), velocity(0.0f) {}
        
        MetaballComponent(const glm::vec3& pos, const glm::vec4& col, float r = 1.0f, float s = 1.0f)
            : position(pos), color(col), radius(r), strength(s), velocity(0.0f) {}
        
        void Update(float deltaTime) {
            // Simple animation - move in a circular pattern
            position += velocity * deltaTime;
            
            // Bounce off boundaries
            if (position.x > 10.0f || position.x < -10.0f) velocity.x *= -1.0f;
            if (position.y > 10.0f || position.y < -10.0f) velocity.y *= -1.0f;
            if (position.z > 10.0f || position.z < -10.0f) velocity.z *= -1.0f;
        }
        
        glm::mat4 GetTransformMatrix() const {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, position);
            model = glm::scale(model, glm::vec3(radius));
            return model;
        }
    };
}
