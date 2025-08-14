#pragma once
#include <glad/glad.h>
#include <cstddef>  // for size_t
#include <vector>
#include <algorithm>

#include "ModelComponent.h"
#include "TextureComponent.h"
#include "../Render/Texture/Texture.h"

namespace C6GE {

struct MeshComponent {
    GLuint VAO, VBO, EBO;
    size_t vertexCount;

    // Constructor: from raw GL handles
    MeshComponent(GLuint vao, GLuint vbo, GLuint ebo, size_t count)
        : VAO(vao), VBO(vbo), EBO(ebo), vertexCount(count) {}

    // Constructor: from Mesh struct (model loader)
    MeshComponent(const Mesh& mesh)
        : VAO(mesh.VAO), VBO(mesh.VBO), EBO(mesh.EBO), vertexCount(mesh.indices.size()) {}

    // New constructor: from ModelComponent pointer
    MeshComponent(C6GE::ModelComponent* modelPtr);

    // Allocator-aware constructor required by EnTT
    MeshComponent(std::allocator_arg_t, const std::allocator<MeshComponent>&, C6GE::ModelComponent* modelPtr);

    // Destructor to clean up GPU resources
    ~MeshComponent();

    // Disable copy semantics
    MeshComponent(const MeshComponent&) = delete;
    MeshComponent& operator=(const MeshComponent&) = delete;

    // Declare move semantics (define in .cpp)
    MeshComponent(MeshComponent&& other) noexcept;
    MeshComponent& operator=(MeshComponent&& other) noexcept;
};

// Free functions to create predefined meshes
MeshComponent CreateTriangle();
MeshComponent CreateQuad();
MeshComponent CreateTemple();
MeshComponent CreateCube();

// Function to create a mesh from raw vertex and index data
MeshComponent CreateMesh(const GLfloat* vertices, size_t vertexSize, const GLuint* indices, size_t indexCount, bool WithColor = true, bool WithTexture = true);
// Function to process a single mesh from Assimp
Mesh ProcessMesh(aiMesh* mesh, const aiScene* scene, const std::string& directory, std::vector<Texture>& textures_loaded);
// Function to process a node recursively
void ProcessNode(aiNode* node, const aiScene* scene, ModelComponent& model, std::vector<Texture>& textures_loaded);
// Function to load a model from file
ModelComponent* LoadModel(const std::string& path);
// Function to load a texture from file
GLuint LoadTextureFromFile(const std::string& filename, const std::string& directory);
// Function to normalize a file path (e.g., for texture loading)
std::string NormalizePath(const std::string& path);

} // namespace C6GE
