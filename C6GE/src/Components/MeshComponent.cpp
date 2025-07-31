#include <glad/glad.h>
#include <cmath>
#include "../Components/MeshComponent.h"

namespace C6GE {

    MeshComponent::~MeshComponent() {
        glDeleteBuffers(1, &VBO);
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &EBO);
    }

    MeshComponent::MeshComponent(MeshComponent&& other) noexcept
        : VAO(other.VAO), VBO(other.VBO), EBO(other.EBO), vertexCount(other.vertexCount) {
        other.VAO = 0;
        other.VBO = 0;
        other.EBO = 0;
        other.vertexCount = 0;
    }

    MeshComponent& MeshComponent::operator=(MeshComponent&& other) noexcept {
        if (this != &other) {
            // Clean up current resources
            glDeleteBuffers(1, &VBO);
            glDeleteVertexArrays(1, &VAO);
            glDeleteBuffers(1, &EBO);
            
            // Move resources from other
            VAO = other.VAO;
            VBO = other.VBO;
            EBO = other.EBO;
            vertexCount = other.vertexCount;
            
            // Reset other
            other.VAO = 0;
            other.VBO = 0;
            other.EBO = 0;
            other.vertexCount = 0;
        }
        return *this;
    }

    MeshComponent CreateMesh(const GLfloat* vertices, size_t vertexSize, const GLuint* indices, size_t indexCount, bool WithColor, bool WithTexture) {
        GLuint vao, vbo, ebo;

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);

        glBindVertexArray(vao);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertexSize, vertices, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(GLuint), indices, GL_STATIC_DRAW);

        // Calculate float count per vertex
        int floatCount = 3; // Position
        if (WithColor) floatCount += 3; // Color
        if (WithTexture) floatCount += 2; // Texture
        GLsizei stride = floatCount * sizeof(float);

        // Position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(0);

        // Color attribute
        if (WithColor) {
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(1);
        }

        // Texture attribute
        if (WithTexture) {
            size_t offset = 3; // Position offset
            if (WithColor) offset += 3; // Add color offset if present
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(offset * sizeof(float)));
            glEnableVertexAttribArray(2);
        }

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        return MeshComponent(vao, vbo, ebo, indexCount);
    }


    MeshComponent CreateTriangle() {
        GLfloat vertices[] =
        {
            // Position X, Y, Z    | Color R, G, B
            -0.5f, -0.5f * float(std::sqrt(3)) / 3, 0.0f,   0.0f, 0.8f, 0.7f,
            0.5f, -0.5f * float(std::sqrt(3)) / 3, 0.0f,    0.3f, 0.5f, 1.0f,
            0.0f,  0.5f * float(std::sqrt(3)) * 2 / 3, 0.0f, 0.6f, 0.2f, 1.0f
        };
        GLuint indices[] = { 0, 1, 2 };

        return CreateMesh(vertices, sizeof(vertices), indices, 3, true, false);
    }

    MeshComponent CreateSquare() {
        static const GLfloat vertices[] = {
            // Positions         // Colors (Green)    // Texture Coords (U, V flipped)
            -0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f,    0.0f, 1.0f,  // Bottom-left
            -0.5f,  0.5f, 0.0f,   0.0f, 1.0f, 0.0f,    0.0f, 0.0f,  // Top-left
            0.5f,  0.5f, 0.0f,   0.0f, 1.0f, 0.0f,    1.0f, 0.0f,  // Top-right
            0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f,    1.0f, 1.0f   // Bottom-right
        };
        static const GLuint indices[] = { 0, 1, 2, 2, 3, 0 };

        return CreateMesh(vertices, sizeof(vertices), indices, 6, true, true);
    }

}
