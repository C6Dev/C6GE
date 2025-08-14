#include <glad/glad.h>
#include <cmath>
#include "../Components/MeshComponent.h"
#include "../Components/ModelComponent.h"
#include <iostream>

namespace C6GE {

// Forward declarations of your texture loading utilities
GLuint LoadTextureFromFile(const std::string& filename, const std::string& directory);

// Constructor from ModelComponent pointer
MeshComponent::MeshComponent(C6GE::ModelComponent* modelPtr) {
    if (modelPtr && !modelPtr->meshes.empty()) {
        VAO = modelPtr->meshes[0].VAO;
        VBO = modelPtr->meshes[0].VBO;
        EBO = modelPtr->meshes[0].EBO;
        vertexCount = modelPtr->meshes[0].indices.size();
    } else {
        VAO = VBO = EBO = 0;
        vertexCount = 0;
    }
}

// Allocator-aware constructor forwarding
MeshComponent::MeshComponent(std::allocator_arg_t, const std::allocator<MeshComponent>&, C6GE::ModelComponent* modelPtr)
    : MeshComponent(modelPtr) {}

MeshComponent::~MeshComponent() {
    if (VAO) glDeleteVertexArrays(1, &VAO);
    if (VBO) glDeleteBuffers(1, &VBO);
    if (EBO) glDeleteBuffers(1, &EBO);
}

// Move constructor
MeshComponent::MeshComponent(MeshComponent&& other) noexcept
    : VAO(other.VAO), VBO(other.VBO), EBO(other.EBO), vertexCount(other.vertexCount) {
    other.VAO = 0;
    other.VBO = 0;
    other.EBO = 0;
    other.vertexCount = 0;
}

// Move assignment operator
MeshComponent& MeshComponent::operator=(MeshComponent&& other) noexcept {
    if (this != &other) {
        if (VAO) glDeleteVertexArrays(1, &VAO);
        if (VBO) glDeleteBuffers(1, &VBO);
        if (EBO) glDeleteBuffers(1, &EBO);

        VAO = other.VAO;
        VBO = other.VBO;
        EBO = other.EBO;
        vertexCount = other.vertexCount;

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

    int floatCount = 3 + 3; // Position + Normal
    if (WithColor) floatCount += 3;
    if (WithTexture) floatCount += 2;
    GLsizei stride = floatCount * sizeof(float);

    // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);

    // Normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    size_t offset = 6; // After pos + normal

    // Color
    if (WithColor) {
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(offset * sizeof(float)));
        glEnableVertexAttribArray(2);
        offset += 3;
    }

    // Texture coords
    if (WithTexture) {
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (void*)(offset * sizeof(float)));
        glEnableVertexAttribArray(3);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return MeshComponent(vao, vbo, ebo, indexCount);
}

MeshComponent CreateTriangle() {
    GLfloat vertices[] = {
        // Position X, Y, Z    | Normal X, Y, Z    | Color R, G, B
        -0.5f, -0.5f * float(std::sqrt(3)) / 3, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.8f, 0.7f,
        0.5f, -0.5f * float(std::sqrt(3)) / 3, 0.0f,    0.0f, 0.0f, 1.0f,   0.3f, 0.5f, 1.0f,
        0.0f,  0.5f * float(std::sqrt(3)) * 2 / 3, 0.0f, 0.0f, 0.0f, 1.0f, 0.6f, 0.2f, 1.0f
    };
    GLuint indices[] = { 0, 1, 2 };

    return CreateMesh(vertices, sizeof(vertices), indices, 3, true, false);
}

MeshComponent CreateQuad() {
    static const GLfloat vertices[] = {
        // Front face (+Z)
        -0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   1.0f, 1.0f, 1.0f,    0.0f, 1.0f,  // 0 Bottom-left
        -0.5f,  0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   1.0f, 1.0f, 1.0f,    0.0f, 0.0f,  // 1 Top-left
         0.5f,  0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   1.0f, 1.0f, 1.0f,    1.0f, 0.0f,  // 2 Top-right
         0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   1.0f, 1.0f, 1.0f,    1.0f, 1.0f,  // 3 Bottom-right

        // Back face (-Z)
        -0.5f, -0.5f, 0.0f,   0.0f, 0.0f,-1.0f,   1.0f, 1.0f, 1.0f,    0.0f, 1.0f,  // 4 Bottom-left
        -0.5f,  0.5f, 0.0f,   0.0f, 0.0f,-1.0f,   1.0f, 1.0f, 1.0f,    0.0f, 0.0f,  // 5 Top-left
         0.5f,  0.5f, 0.0f,   0.0f, 0.0f,-1.0f,   1.0f, 1.0f, 1.0f,    1.0f, 0.0f,  // 6 Top-right
         0.5f, -0.5f, 0.0f,   0.0f, 0.0f,-1.0f,   1.0f, 1.0f, 1.0f,    1.0f, 1.0f   // 7 Bottom-right
    };

    static const GLuint indices[] = {
        // Front (+Z)
        0, 1, 2, 0, 2, 3,
        // Back (-Z) — wound so normal points -Z
        4, 6, 5, 4, 7, 6
    };

    return CreateMesh(vertices, sizeof(vertices), indices, sizeof(indices) / sizeof(GLuint), true, true);
}

MeshComponent CreateTemple() {
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
         0.5f, -0.5f,  0.5f,  0.0f, 0.0f, 1.0f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f, // 1
         0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f, // 2
        -0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f,  1.0f, 1.0f, 1.0f,  0.0f, 1.0f, // 3
        // Back face (-Z)
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,-1.0f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f, // 4
         0.5f, -0.5f, -0.5f,  0.0f, 0.0f,-1.0f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f, // 5
         0.5f,  0.5f, -0.5f,  0.0f, 0.0f,-1.0f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f, // 6
        -0.5f,  0.5f, -0.5f,  0.0f, 0.0f,-1.0f,  1.0f, 1.0f, 1.0f,  0.0f, 1.0f, // 7
        // Left face (-X)
        -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f, // 8
        -0.5f, -0.5f,  0.5f, -1.0f, 0.0f, 0.0f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f, // 9
        -0.5f,  0.5f,  0.5f, -1.0f, 0.0f, 0.0f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f, //10
        -0.5f,  0.5f, -0.5f, -1.0f, 0.0f, 0.0f,  1.0f, 1.0f, 1.0f,  0.0f, 1.0f, //11
        // Right face (+X)
         0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f, //12
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f, //13
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 0.0f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f, //14
         0.5f,  0.5f, -0.5f,  1.0f, 0.0f, 0.0f,  1.0f, 1.0f, 1.0f,  0.0f, 1.0f, //15
        // Top face (+Y)
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f, //16
         0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f, //17
         0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 0.0f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f, //18
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 0.0f,  1.0f, 1.0f, 1.0f,  0.0f, 1.0f, //19
        // Bottom face (-Y)
        -0.5f, -0.5f, -0.5f,  0.0f,-1.0f, 0.0f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f, //20
         0.5f, -0.5f, -0.5f,  0.0f,-1.0f, 0.0f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f, //21
         0.5f, -0.5f,  0.5f,  0.0f,-1.0f, 0.0f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f, //22
        -0.5f, -0.5f,  0.5f,  0.0f,-1.0f, 0.0f,  1.0f, 1.0f, 1.0f,  0.0f, 1.0f  //23
    };

    static const GLuint indices[] = {
        // Front (+Z)
        0, 1, 2,    0, 2, 3,
        // Back (-Z) — wound so normal points -Z
        4, 6, 5,    4, 7, 6,
        // Left (-X)
        8, 9, 10,   8, 10, 11,
        // Right (+X)
        12, 14, 13, 12, 15, 14,
        // Top (+Y)
        16, 18, 17, 16, 19, 18,
        // Bottom (-Y)
        20, 21, 22, 20, 22, 23
    };

    return CreateMesh(vertices, sizeof(vertices), indices, sizeof(indices) / sizeof(GLuint), true, true);
}

// ModelComponent
void SetupMesh(Mesh& mesh) {
    glGenVertexArrays(1, &mesh.VAO);
    glGenBuffers(1, &mesh.VBO);
    glGenBuffers(1, &mesh.EBO);

    glBindVertexArray(mesh.VAO);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(Vertex), mesh.vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(unsigned int), mesh.indices.data(), GL_STATIC_DRAW);

    // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);

    // Normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
    glEnableVertexAttribArray(1);

    // Color (only enable if color data exists)
    if (!mesh.vertices.empty() && mesh.vertices[0].Color != glm::vec3(0.0f)) { // Check if color data is non-zero
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Color));
        glEnableVertexAttribArray(2);
    }

    // Texture coords
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);
}

// ProcessMesh function to handle mesh
Mesh ProcessMesh(aiMesh* mesh, const aiScene* scene, const std::string& directory, std::vector<Texture>& textures_loaded) {
    Mesh newMesh;

    // Process vertices
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex;
        vertex.Position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
        vertex.Normal   = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };

        if (mesh->HasVertexColors(0)) {
            vertex.Color = { mesh->mColors[0][i].r, mesh->mColors[0][i].g, mesh->mColors[0][i].b };
        } else {
            vertex.Color = { 0.0f, 0.0f, 0.0f };
        }

        if (mesh->mTextureCoords[0]) {
            vertex.TexCoords = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
        } else {
            vertex.TexCoords = { 0.0f, 0.0f };
        }

        newMesh.vertices.push_back(vertex);
    }

    // Process indices
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            newMesh.indices.push_back(face.mIndices[j]);
        }
    }

    // **Fix winding order to CCW**
    for (size_t i = 0; i < newMesh.indices.size(); i += 3) {
        auto& v0 = newMesh.vertices[newMesh.indices[i + 0]];
        auto& v1 = newMesh.vertices[newMesh.indices[i + 1]];
        auto& v2 = newMesh.vertices[newMesh.indices[i + 2]];

        glm::vec3 edge1 = v1.Position - v0.Position;
        glm::vec3 edge2 = v2.Position - v0.Position;
        glm::vec3 normal = glm::cross(edge1, edge2);

        // If normal is pointing away from vertex normal, flip
        if (glm::dot(normal, v0.Normal) < 0.0f) {
            std::swap(newMesh.indices[i + 1], newMesh.indices[i + 2]);
        }
    }

    // Process material (same as your existing code) ...
    if (mesh->mMaterialIndex >= 0) {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        auto loadMaterialTextures = [&](aiTextureType type, const std::string& typeName) {
            std::vector<Texture> textures;
            for (unsigned int i = 0; i < material->GetTextureCount(type); i++) {
                aiString str;
                material->GetTexture(type, i, &str);
                std::string filename = str.C_Str();
                bool skip = false;
                for (const auto& loadedTex : textures_loaded) {
                    if (loadedTex.path == filename) {
                        textures.push_back(loadedTex);
                        skip = true;
                        break;
                    }
                }
                if (!skip) {
                    GLuint textureID = LoadTextureFromFile(filename, directory);
                    Texture texture = { textureID, typeName, filename };
                    textures.push_back(texture);
                    textures_loaded.push_back(texture);
                }
            }
            return textures;
        };

        std::vector<Texture> diffuseMaps = loadMaterialTextures(aiTextureType_DIFFUSE, "texture_diffuse");
        newMesh.textures.insert(newMesh.textures.end(), diffuseMaps.begin(), diffuseMaps.end());

        std::vector<Texture> specularMaps = loadMaterialTextures(aiTextureType_SPECULAR, "texture_specular");
        newMesh.textures.insert(newMesh.textures.end(), specularMaps.begin(), specularMaps.end());
    }

    SetupMesh(newMesh);
    return newMesh;
}


// ProcessNode function to recursively process nodes
void ProcessNode(aiNode* node, const aiScene* scene, ModelComponent& model, std::vector<Texture>& textures_loaded) {
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        model.meshes.push_back(ProcessMesh(mesh, scene, model.directory, textures_loaded));
    }
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        ProcessNode(node->mChildren[i], scene, model, textures_loaded);
    }
}

ModelComponent* LoadModel(const std::string& path) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
        return nullptr;
    }

    ModelComponent* model = new ModelComponent();
    model->directory = path.substr(0, path.find_last_of('/'));

    // Vector to track loaded textures and avoid duplication
    std::vector<Texture> textures_loaded;

    ProcessNode(scene->mRootNode, scene, *model, textures_loaded);

    // Save loaded textures to model
    model->textures_loaded = std::move(textures_loaded);

    return model;
}

// Texture loading
GLuint LoadTextureFromFile(const std::string& filename, const std::string& directory) {
    std::string filepath = directory + "/" + filename;
    filepath = NormalizePath(filepath);

    int width, height, channels;
    unsigned char* data = LoadTexture(filepath, width, height, channels);

    if (!data) {
        std::cerr << "Failed to load texture at path: " << filepath << std::endl;
        return 0;
    }

    GLuint textureID = CreateTexture(data, width, height, channels);

    // Free image data if needed
    // stbi_image_free(data); // if stb_image

    return textureID;
}

std::string NormalizePath(const std::string& path) {
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    return normalized;
}

}