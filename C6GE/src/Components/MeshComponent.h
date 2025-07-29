#pragma once

#include <cstddef>
#include <memory>

typedef unsigned int GLuint;

namespace C6GE {

    struct MeshComponent {
        GLuint VAO, VBO, EBO;
        size_t vertexCount;

        MeshComponent(GLuint vao, GLuint vbo, GLuint ebo, size_t count)
            : VAO(vao), VBO(vbo), EBO(ebo), vertexCount(count) {}

        // Automatically clean up GPU resources
        ~MeshComponent();

        MeshComponent(const MeshComponent&) = delete;
        MeshComponent& operator=(const MeshComponent&) = delete;
        MeshComponent(MeshComponent&& other) noexcept;
        MeshComponent& operator=(MeshComponent&& other) noexcept;
    };

    MeshComponent CreateTriangle();
    MeshComponent CreateSquare();

}
