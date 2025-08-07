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
            // Front face (normal +Z)
            -0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   1.0f, 1.0f, 1.0f,    0.0f, 1.0f,  // 0 Bottom-left
            -0.5f,  0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   1.0f, 1.0f, 1.0f,    0.0f, 0.0f,  // 1 Top-left
            0.5f,  0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   1.0f, 1.0f, 1.0f,    1.0f, 0.0f,  // 2 Top-right
            0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   1.0f, 1.0f, 1.0f,    1.0f, 1.0f,  // 3 Bottom-right
            // Back face (normal -Z)
            -0.5f, -0.5f, 0.0f,   0.0f, 0.0f, -1.0f,  1.0f, 1.0f, 1.0f,    0.0f, 1.0f,  // 4 Bottom-left
            -0.5f,  0.5f, 0.0f,   0.0f, 0.0f, -1.0f,  1.0f, 1.0f, 1.0f,    0.0f, 0.0f,  // 5 Top-left
            0.5f,  0.5f, 0.0f,   0.0f, 0.0f, -1.0f,  1.0f, 1.0f, 1.0f,    1.0f, 0.0f,  // 6 Top-right
            0.5f, -0.5f, 0.0f,   0.0f, 0.0f, -1.0f,  1.0f, 1.0f, 1.0f,    1.0f, 1.0f   // 7 Bottom-right
        };
        static const GLuint indices[] = {
            // Front face
            0, 3, 2,
            0, 2, 1,
            // Back face (reversed winding)
            4, 5, 6,
            4, 6, 7
        };
        return CreateMesh(vertices, sizeof(vertices), indices, 12, true, true);
    }

    MeshComponent CreateTemple() {
        // Define vertices with corrected normals
        static const GLfloat vertices[] = {
            // Base - bottom face
            -0.5f, -0.5f, -0.5f,  0.0f, -1.0f, 0.0f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f,  // 0
            0.5f, -0.5f, -0.5f,   0.0f, -1.0f, 0.0f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f,  // 1
            0.5f, -0.5f, 0.5f,    0.0f, -1.0f, 0.0f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f,  // 2
            -0.5f, -0.5f, 0.5f,   0.0f, -1.0f, 0.0f,  1.0f, 1.0f, 1.0f,  0.0f, 1.0f,  // 3
            
            // Front face vertices with correct normals
            -0.5f, -0.5f, 0.5f,   0.0f, 0.4472f, 0.8944f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f,  // 4
            0.5f, -0.5f, 0.5f,    0.0f, 0.4472f, 0.8944f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f,  // 5
            0.0f, 0.5f, 0.0f,     0.0f, 0.4472f, 0.8944f,  1.0f, 1.0f, 1.0f,  0.5f, 1.0f,  // 6
            
            // Right face vertices with correct normals
            0.5f, -0.5f, 0.5f,    0.8944f, 0.4472f, 0.0f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f,  // 7
            0.5f, -0.5f, -0.5f,   0.8944f, 0.4472f, 0.0f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f,  // 8
            0.0f, 0.5f, 0.0f,     0.8944f, 0.4472f, 0.0f,  1.0f, 1.0f, 1.0f,  0.5f, 1.0f,  // 9
            
            // Back face vertices with flipped normals
            0.5f, -0.5f, -0.5f,   0.0f, -0.4472f, 0.8944f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f,  // 10
            -0.5f, -0.5f, -0.5f,  0.0f, -0.4472f, 0.8944f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f,  // 11
            0.0f, 0.5f, 0.0f,     0.0f, -0.4472f, 0.8944f,  1.0f, 1.0f, 1.0f,  0.5f, 1.0f,  // 12
            
            // Left face vertices with correct normals
            -0.5f, -0.5f, -0.5f,  -0.8944f, 0.4472f, 0.0f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f,  // 13
            -0.5f, -0.5f, 0.5f,   -0.8944f, 0.4472f, 0.0f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f,  // 14
            0.0f, 0.5f, 0.0f,     -0.8944f, 0.4472f, 0.0f,  1.0f, 1.0f, 1.0f,  0.5f, 1.0f   // 15
        };
        
        // Define indices with corrected winding for outward CCW
        static const GLuint indices[] = {
            // Base (bottom face, CCW from outside)
            0, 1, 2,
            0, 2, 3,
            // Front face (+Z, CCW)
            4, 5, 6,
            // Right face (+X, CCW)
            7, 8, 9,
            // Back face (-Z, CCW with flipped normals)
            10, 11, 12,
            // Left face (-X, CCW)
            13, 14, 15
        };
        
        return CreateMesh(vertices, sizeof(vertices), indices, sizeof(indices) / sizeof(GLuint), true, true);
    }

    MeshComponent CreateCube() {
        static const GLfloat vertices[] = {
            // Front face (+Z)
            -0.5f, -0.5f,  0.5f,  0.0f, 0.0f, 1.0f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f, // 0
            0.5f, -0.5f,  0.5f,   0.0f, 0.0f, 1.0f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f, // 1
            0.5f,  0.5f,  0.5f,   0.0f, 0.0f, 1.0f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f, // 2
            -0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f,  1.0f, 1.0f, 1.0f,  0.0f, 1.0f, // 3
            // Back face (-Z)
            -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, -1.0f, 1.0f, 1.0f, 1.0f,  0.0f, 0.0f, // 4
            0.5f, -0.5f, -0.5f,   0.0f, 0.0f, -1.0f, 1.0f, 1.0f, 1.0f,  1.0f, 0.0f, // 5
            0.5f,  0.5f, -0.5f,   0.0f, 0.0f, -1.0f, 1.0f, 1.0f, 1.0f,  1.0f, 1.0f, // 6
            -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, -1.0f, 1.0f, 1.0f, 1.0f,  0.0f, 1.0f, // 7
            // Left face (-X)
            -0.5f, -0.5f, -0.5f,  -1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,  0.0f, 0.0f, // 8
            -0.5f, -0.5f,  0.5f,  -1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,  1.0f, 0.0f, // 9
            -0.5f,  0.5f,  0.5f,  -1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,  1.0f, 1.0f, // 10
            -0.5f,  0.5f, -0.5f,  -1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,  0.0f, 1.0f, // 11
            // Right face (+X)
            0.5f, -0.5f, -0.5f,   1.0f, 0.0f, 0.0f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f, // 12
            0.5f, -0.5f,  0.5f,   1.0f, 0.0f, 0.0f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f, // 13
            0.5f,  0.5f,  0.5f,   1.0f, 0.0f, 0.0f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f, // 14
            0.5f,  0.5f, -0.5f,   1.0f, 0.0f, 0.0f,  1.0f, 1.0f, 1.0f,  0.0f, 1.0f, // 15
            // Top face (+Y)
            -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f, // 16
            0.5f,  0.5f, -0.5f,   0.0f, 1.0f, 0.0f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f, // 17
            0.5f,  0.5f,  0.5f,   0.0f, 1.0f, 0.0f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f, // 18
            -0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 0.0f,  1.0f, 1.0f, 1.0f,  0.0f, 1.0f, // 19
            // Bottom face (-Y)
            -0.5f, -0.5f, -0.5f,  0.0f, -1.0f, 0.0f, 1.0f, 1.0f, 1.0f,  0.0f, 0.0f, // 20
            0.5f, -0.5f, -0.5f,   0.0f, -1.0f, 0.0f, 1.0f, 1.0f, 1.0f,  1.0f, 0.0f, // 21
            0.5f, -0.5f,  0.5f,   0.0f, -1.0f, 0.0f, 1.0f, 1.0f, 1.0f,  1.0f, 1.0f, // 22
            -0.5f, -0.5f,  0.5f,  0.0f, -1.0f, 0.0f, 1.0f, 1.0f, 1.0f,  0.0f, 1.0f  // 23
        };
        static const GLuint indices[] = {
            0, 1, 2, 0, 2, 3,    // Front (CCW)
            4, 5, 6, 4, 6, 7,    // Back (CCW from outside)
            8, 9, 10, 8, 10, 11, // Left (CCW)
            12, 13, 14, 12, 14, 15, // Right (CCW)
            16, 17, 18, 16, 18, 19, // Top (CCW)
            20, 23, 22, 20, 22, 21  // Bottom (CCW from outside)
        };
        return CreateMesh(vertices, sizeof(vertices), indices, sizeof(indices) / sizeof(GLuint), true, true);
    }

}
