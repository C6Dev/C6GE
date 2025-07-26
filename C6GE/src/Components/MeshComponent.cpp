#include "../Components/MeshComponent.h"
#include <glad/glad.h>

namespace C6GE {

    MeshComponent::~MeshComponent() {
        glDeleteBuffers(1, &VBO);
        glDeleteVertexArrays(1, &VAO);
    }

    MeshComponent::MeshComponent(MeshComponent&& other) noexcept
        : VAO(other.VAO), VBO(other.VBO), vertexCount(other.vertexCount) {
        other.VAO = 0;
        other.VBO = 0;
        other.vertexCount = 0;
    }

    MeshComponent& MeshComponent::operator=(MeshComponent&& other) noexcept {
        if (this != &other) {
            // Clean up current resources
            glDeleteBuffers(1, &VBO);
            glDeleteVertexArrays(1, &VAO);
            
            // Move resources from other
            VAO = other.VAO;
            VBO = other.VBO;
            vertexCount = other.vertexCount;
            
            // Reset other
            other.VAO = 0;
            other.VBO = 0;
            other.vertexCount = 0;
        }
        return *this;
    }

    MeshComponent CreateTriangleMesh() {
        float vertices[] = {
            -0.5f, -0.5f, 0.0f,
             0.5f, -0.5f, 0.0f,
             0.0f,  0.5f, 0.0f
        };

        GLuint vao, vbo;

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        return MeshComponent(vao, vbo, 3);
    }

}
