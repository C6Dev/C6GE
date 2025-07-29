#include "../Components/MeshComponent.h"
#include <glad/glad.h>

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

    MeshComponent CreateTriangle() {
	// Vertices coordinates
	GLfloat vertices[] =
	{
		-0.5f, -0.5f * float(sqrt(3)) / 3, 0.0f, // Lower left corner
		0.5f, -0.5f * float(sqrt(3)) / 3, 0.0f, // Lower right corner
		0.0f, 0.5f * float(sqrt(3)) * 2 / 3, 0.0f // Upper corner
	};

	// Indices for vertices order
	GLuint indices[] =
	{
        0, 1, 2
	};
    
        GLuint vao, vbo, ebo;

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);


        return MeshComponent(vao, vbo, ebo, 9);
    }
    
    MeshComponent CreateSquare() {
        // Vertices coordinates
        GLfloat vertices[] =
        {
            -0.5f, -0.5f, 0.0f,
            -0.5f, 0.5f, 0.0f,
            0.5f, 0.5f, 0.0f,
            0.5f, -0.5f, 0.0f
        };

        // Indices for vertices order
        GLuint indices[] =
        {
            0, 1, 2,
            2, 3, 0
        };
    
        GLuint vao, vbo, ebo;

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);


        return MeshComponent(vao, vbo, ebo, 6);
    }


}
