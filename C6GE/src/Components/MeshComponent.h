#pragma once

#include <cstddef>  // for size_t

namespace C6GE {

    struct MeshComponent {
        GLuint VAO, VBO, EBO;
        size_t vertexCount;

        // Constructor
        MeshComponent(GLuint vao, GLuint vbo, GLuint ebo, size_t count)
            : VAO(vao), VBO(vbo), EBO(ebo), vertexCount(count) {}

        // Destructor to clean up GPU resources
        ~MeshComponent();

        // Disable copy semantics
        MeshComponent(const MeshComponent&) = delete;
        MeshComponent& operator=(const MeshComponent&) = delete;

        // Enable move semantics
        MeshComponent(MeshComponent&& other) noexcept;
        MeshComponent& operator=(MeshComponent&& other) noexcept;

        // Optional: If CreateMesh is member function, mark static.
        // Otherwise, make it a free function (recommended)
        // static MeshComponent CreateMesh(const GLfloat* vertices, size_t vertexSize,
        //                                 const GLuint* indices, size_t indexCount,
        //                                 bool WithColor, bool WithTexture);
    };

    // Free functions to create predefined meshes
    MeshComponent CreateTriangle();
    MeshComponent CreateSquare();

}
