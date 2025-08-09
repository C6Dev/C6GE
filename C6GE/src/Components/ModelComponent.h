#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>

// Assimp includes (must be before using aiMesh/aiScene/aiNode)
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

#include "TextureComponent.h"

namespace C6GE {

    struct Vertex {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec3 Color;
        glm::vec2 TexCoords;
    };

    struct Mesh {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        std::vector<Texture> textures;
        unsigned int VAO, VBO, EBO;
        unsigned int textureID; // Diffuse texture
    };

    struct ModelComponent {
        std::vector<Mesh> meshes;
        std::vector<Texture> textures_loaded;
        std::string directory;
    };

    // init functions used in mesh component
    static void SetupMesh(Mesh& mesh);
    static Mesh ProcessMesh(aiMesh* mesh, const aiScene* scene, const std::string& directory, std::vector<Texture>& textures_loaded);
    static void ProcessNode(aiNode* node, const aiScene* scene, ModelComponent& model, std::vector<Texture>& textures_loaded);
    ModelComponent* LoadModel(const std::string& path);

}