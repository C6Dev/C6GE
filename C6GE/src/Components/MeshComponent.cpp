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
        int floatCount = 3 + 3; // Position + Normal
        if (WithColor) floatCount += 3; // Color
        if (WithTexture) floatCount += 2; // Texture
        GLsizei stride = floatCount * sizeof(float);

        // Position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(0);

        // Normal attribute
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // Color attribute
        size_t offset = 6; // After pos + normal
        if (WithColor) {
            glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(offset * sizeof(float)));
            glEnableVertexAttribArray(2);
            offset += 3;
        }

        // Texture attribute
        if (WithTexture) {
            glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (void*)(offset * sizeof(float)));
            glEnableVertexAttribArray(3);
        }

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        return MeshComponent(vao, vbo, ebo, indexCount);
    }


    MeshComponent CreateTriangle() {
        GLfloat vertices[] =
        {
            // Position X, Y, Z    | Normal X, Y, Z    | Color R, G, B
            -0.5f, -0.5f * float(std::sqrt(3)) / 3, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.8f, 0.7f,
            0.5f, -0.5f * float(std::sqrt(3)) / 3, 0.0f,    0.0f, 0.0f, 1.0f,   0.3f, 0.5f, 1.0f,
            0.0f,  0.5f * float(std::sqrt(3)) * 2 / 3, 0.0f, 0.0f, 0.0f, 1.0f, 0.6f, 0.2f, 1.0f
        };
        GLuint indices[] = { 0, 1, 2 };

        return CreateMesh(vertices, sizeof(vertices), indices, 3, true, false);
    }

    MeshComponent CreateSquare() {
        static const GLfloat vertices[] = {
            // Positions         // Normals           // Colors (Green)    // Texture Coords (U, V flipped)
            -0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f,    0.0f, 1.0f,  // Bottom-left
            -0.5f,  0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f,    0.0f, 0.0f,  // Top-left
            0.5f,  0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f,    1.0f, 0.0f,  // Top-right
            0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f,    1.0f, 1.0f   // Bottom-right
        };
        static const GLuint indices[] = { 0, 1, 2, 2, 3, 0 };

        return CreateMesh(vertices, sizeof(vertices), indices, 6, true, true);
    }

    MeshComponent CreateTemple() {
        static const GLfloat vertices[] = {
            // Base
            -0.5f, -0.5f, -0.5f,  0.0f, -1.0f, 0.0f,  1.0f, 0.0f, 0.0f,  0.0f, 0.0f,  // 0
            0.5f, -0.5f, -0.5f,   0.0f, -1.0f, 0.0f,  0.0f, 1.0f, 0.0f,  1.0f, 0.0f,  // 1
            0.5f, -0.5f, 0.5f,    0.0f, -1.0f, 0.0f,  0.0f, 0.0f, 1.0f,  1.0f, 1.0f,  // 2
            -0.5f, -0.5f, 0.5f,   0.0f, -1.0f, 0.0f,  1.0f, 1.0f, 0.0f,  0.0f, 1.0f,  // 3
            // Apex
            0.0f, 0.5f, 0.0f,     0.0f, 1.0f, 0.0f,  1.0f, 1.0f, 1.0f,  0.5f, 0.5f    // 4 (apex normal upward for simplicity)
        };
        static const GLuint indices[] = {
            // Base
            0, 1, 2,  2, 3, 0,
            // Sides
            0, 1, 4,
            1, 2, 4,
            2, 3, 4,
            3, 0, 4
        };
        return CreateMesh(vertices, sizeof(vertices), indices, sizeof(indices) / sizeof(GLuint), true, true);
    }

    MeshComponent CreateCube() {
    static const GLfloat vertices[] = {
        // Positions           // Normals (averaged)    // Colors              // Texture Coords
        -0.5f, -0.5f, -0.5f,  -0.577f, -0.577f, -0.577f, 0.0f, 1.0f, 0.0f,   0.0f, 0.0f, // 0
        0.5f, -0.5f, -0.5f,    0.577f, -0.577f, -0.577f, 0.0f, 1.0f, 0.0f,   1.0f, 0.0f, // 1
        0.5f,  0.5f, -0.5f,    0.577f, 0.577f, -0.577f,  0.0f, 1.0f, 0.0f,   1.0f, 1.0f, // 2
        -0.5f,  0.5f, -0.5f,  -0.577f, 0.577f, -0.577f, 0.0f, 1.0f, 0.0f,   0.0f, 1.0f, // 3
        -0.5f, -0.5f,  0.5f,  -0.577f, -0.577f, 0.577f,  1.0f, 1.0f, 1.0f,   0.0f, 0.0f, // 4
        0.5f, -0.5f,  0.5f,    0.577f, -0.577f, 0.577f,  1.0f, 1.0f, 1.0f,   1.0f, 0.0f, // 5
        0.5f,  0.5f,  0.5f,    0.577f, 0.577f, 0.577f,   1.0f, 1.0f, 1.0f,   1.0f, 1.0f, // 6
        -0.5f,  0.5f,  0.5f,  -0.577f, 0.577f, 0.577f,  1.0f, 1.0f, 1.0f,   0.0f, 1.0f  // 7
    };
    static const GLuint indices[] = {
        // Back face
        0, 1, 2,
        2, 3, 0,

        // Front face
        4, 5, 6,
        6, 7, 4,

        // Left face
        0, 3, 7,
        7, 4, 0,

        // Right face
        1, 5, 6,
        6, 2, 1,

        // Top face
        3, 2, 6,
        6, 7, 3,

        // Bottom face
        0, 1, 5,
        5, 4, 0
    };
        return CreateMesh(vertices, sizeof(vertices), indices, sizeof(indices) / sizeof(GLuint), true, true);
    }

}
