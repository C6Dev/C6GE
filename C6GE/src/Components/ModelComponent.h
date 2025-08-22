#pragma once

#include <string>
#include <vector>
#include <bgfx/bgfx.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

struct ModelMesh {
    std::vector<float> vertices;
    std::vector<uint16_t> indices;
    bgfx::VertexBufferHandle vb = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle ib = BGFX_INVALID_HANDLE;
    bool loaded = false;
};

class ModelComponent {
public:
    enum class PrimitiveType { Quad, Cube, Sphere };
    std::string modelPath;
    ModelMesh mesh;
    bool loaded = false;
    
    ModelComponent(const std::string& path) : modelPath(path) {}
    ModelComponent() = default;
    
    bool LoadModel();
    bool BuildPrimitive(PrimitiveType type, float size = 1.0f, int segments = 16);
    void Cleanup();
private:
    void GenerateQuad(float size);
    void GenerateCube(float size);
    void GenerateSphere(float radius, int segments);
};