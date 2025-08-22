#include "ModelComponent.h"
#include "../Logging/Log.h"
#include "../Render/VertexLayouts.h"

using namespace C6GE;

bool ModelComponent::LoadModel() {
    if (loaded) {
        return true; // Already loaded
    }
    
    Log(LogLevel::info, "ModelComponent: Loading model from " + modelPath);
    
    // Load the FBX file using Assimp
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(modelPath, 
        aiProcess_Triangulate | 
        aiProcess_GenNormals | 
        aiProcess_CalcTangentSpace |
        aiProcess_FlipUVs);
    
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        Log(LogLevel::error, "ModelComponent: Failed to load model: " + std::string(importer.GetErrorString()));
        return false;
    }
    
    Log(LogLevel::info, "ModelComponent: Model file loaded successfully!");
    
    // Process the first mesh in the scene
    if (scene->mNumMeshes > 0) {
        aiMesh* aiMesh = scene->mMeshes[0];
        Log(LogLevel::info, "ModelComponent: Processing mesh with " + std::to_string(aiMesh->mNumVertices) + " vertices");
        
        // Initialize vertex layout if not done
        PosNormalTexCoordVertex::init();
        
        // Convert mesh data to our vertex format
        mesh.vertices.clear();
        mesh.indices.clear();
        
        // Process vertices
        for (unsigned int i = 0; i < aiMesh->mNumVertices; i++) {
            // Position
            mesh.vertices.push_back(aiMesh->mVertices[i].x);
            mesh.vertices.push_back(aiMesh->mVertices[i].y);
            mesh.vertices.push_back(aiMesh->mVertices[i].z);
            
            // Normal
            if (aiMesh->HasNormals()) {
                mesh.vertices.push_back(aiMesh->mNormals[i].x);
                mesh.vertices.push_back(aiMesh->mNormals[i].y);
                mesh.vertices.push_back(aiMesh->mNormals[i].z);
            } else {
                mesh.vertices.push_back(0.0f);
                mesh.vertices.push_back(1.0f);
                mesh.vertices.push_back(0.0f);
            }
            
            // Texture coordinates
            if (aiMesh->HasTextureCoords(0)) {
                mesh.vertices.push_back(aiMesh->mTextureCoords[0][i].x);
                mesh.vertices.push_back(aiMesh->mTextureCoords[0][i].y);
            } else {
                mesh.vertices.push_back(0.0f);
                mesh.vertices.push_back(0.0f);
            }
        }
        
        // Process indices
        for (unsigned int i = 0; i < aiMesh->mNumFaces; i++) {
            aiFace face = aiMesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++) {
                mesh.indices.push_back(face.mIndices[j]);
            }
        }
        
        Log(LogLevel::info, "ModelComponent: Created " + std::to_string(mesh.vertices.size() / 8) + " vertices and " + std::to_string(mesh.indices.size()) + " indices");
        
        // Create vertex buffer
        const bgfx::Memory* vbMem = bgfx::alloc(mesh.vertices.size() * sizeof(float));
        memcpy(vbMem->data, mesh.vertices.data(), mesh.vertices.size() * sizeof(float));
        mesh.vb = bgfx::createVertexBuffer(vbMem, PosNormalTexCoordVertex::ms_layout);
        
        // Create index buffer
        const bgfx::Memory* ibMem = bgfx::alloc(mesh.indices.size() * sizeof(uint16_t));
        memcpy(ibMem->data, mesh.indices.data(), mesh.indices.size() * sizeof(uint16_t));
        mesh.ib = bgfx::createIndexBuffer(ibMem);
        
        mesh.loaded = true;
        loaded = true;
        Log(LogLevel::info, "ModelComponent: Model loaded and cached successfully!");
        return true;
    } else {
        Log(LogLevel::error, "ModelComponent: No meshes found in model file");
        return false;
    }
}

void ModelComponent::Cleanup() {
    if (mesh.loaded) {
        if (bgfx::isValid(mesh.vb)) {
            bgfx::destroy(mesh.vb);
            mesh.vb = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(mesh.ib)) {
            bgfx::destroy(mesh.ib);
            mesh.ib = BGFX_INVALID_HANDLE;
        }
        mesh.loaded = false;
    }
    loaded = false;
}

bool ModelComponent::BuildPrimitive(PrimitiveType type, float size, int segments) {
    // Release existing
    Cleanup();
    mesh.vertices.clear();
    mesh.indices.clear();

    switch (type) {
    case PrimitiveType::Quad:    GenerateQuad(size); break;
    case PrimitiveType::Cube:    GenerateCube(size); break;
    case PrimitiveType::Sphere:  GenerateSphere(size * 0.5f, std::max(8, segments)); break;
    }

    if (mesh.vertices.empty() || mesh.indices.empty()) return false;

    // Create BGFX buffers
    const bgfx::Memory* vbMem = bgfx::alloc(mesh.vertices.size() * sizeof(float));
    memcpy(vbMem->data, mesh.vertices.data(), mesh.vertices.size() * sizeof(float));
    mesh.vb = bgfx::createVertexBuffer(vbMem, PosNormalTexCoordVertex::ms_layout);

    const bgfx::Memory* ibMem = bgfx::alloc(mesh.indices.size() * sizeof(uint16_t));
    memcpy(ibMem->data, mesh.indices.data(), mesh.indices.size() * sizeof(uint16_t));
    mesh.ib = bgfx::createIndexBuffer(ibMem);

    mesh.loaded = true;
    loaded = true;
    return true;
}

void ModelComponent::GenerateQuad(float size) {
    PosNormalTexCoordVertex::init();
    const float hs = size * 0.5f;
    // 4 vertices: pos(3) normal(3) uv(2)
    const float v[] = {
        -hs, 0.0f, -hs,   0,1,0,   0,1,
         hs, 0.0f, -hs,   0,1,0,   1,1,
         hs, 0.0f,  hs,   0,1,0,   1,0,
        -hs, 0.0f,  hs,   0,1,0,   0,0,
    };
    const uint16_t idx[] = { 0,2,1, 0,3,2 }; // Fixed winding order
    mesh.vertices.assign(v, v + 4 * 8);
    mesh.indices.assign(idx, idx + 6);
}

void ModelComponent::GenerateCube(float size) {
    PosNormalTexCoordVertex::init();
    const float hs = size * 0.5f;
    // 24 vertices (4 per face x 6 faces)
    const float v[] = {
        // +Y
        -hs, hs, -hs,  0,1,0,  0,1,
         hs, hs, -hs,  0,1,0,  1,1,
         hs, hs,  hs,  0,1,0,  1,0,
        -hs, hs,  hs,  0,1,0,  0,0,
        // -Y
        -hs,-hs,  hs,  0,-1,0, 0,1,
         hs,-hs,  hs,  0,-1,0, 1,1,
         hs,-hs, -hs,  0,-1,0, 1,0,
        -hs,-hs, -hs,  0,-1,0, 0,0,
        // +X
         hs,-hs,-hs,  1,0,0,  0,1,
         hs, hs,-hs,  1,0,0,  1,1,
         hs, hs, hs,  1,0,0,  1,0,
         hs,-hs, hs,  1,0,0,  0,0,
        // -X
        -hs,-hs, hs, -1,0,0,  0,1,
        -hs, hs, hs, -1,0,0,  1,1,
        -hs, hs,-hs, -1,0,0,  1,0,
        -hs,-hs,-hs, -1,0,0,  0,0,
        // +Z
        -hs,-hs, hs,  0,0,1,  0,1,
         hs,-hs, hs,  0,0,1,  1,1,
         hs, hs, hs,  0,0,1,  1,0,
        -hs, hs, hs,  0,0,1,  0,0,
        // -Z
         hs,-hs,-hs, 0,0,-1,  0,1,
        -hs,-hs,-hs, 0,0,-1,  1,1,
        -hs, hs,-hs, 0,0,-1,  1,0,
         hs, hs,-hs, 0,0,-1,  0,0,
    };
    const uint16_t idx[] = { 0,2,1, 0,3,2, 4,6,5, 4,7,6, 8,10,9, 8,11,10, 12,14,13, 12,15,14, 16,18,17, 16,19,18, 20,22,21, 20,23,22 }; // Fixed winding order
    mesh.vertices.assign(v, v + 24 * 8);
    mesh.indices.assign(idx, idx + 36);
}

void ModelComponent::GenerateSphere(float radius, int segments) {
    PosNormalTexCoordVertex::init();
    const int rings = std::max(3, segments);
    const int segs  = std::max(3, segments);
    for (int r = 0; r <= rings; ++r) {
        float v = float(r) / float(rings);
        float phi = v * 3.14159265f;
        for (int s = 0; s <= segs; ++s) {
            float u = float(s) / float(segs);
            float theta = u * 2.0f * 3.14159265f;
            float nx = std::sin(phi) * std::cos(theta);
            float ny = std::cos(phi);
            float nz = std::sin(phi) * std::sin(theta);
            mesh.vertices.push_back(radius * nx);
            mesh.vertices.push_back(radius * ny);
            mesh.vertices.push_back(radius * nz);
            mesh.vertices.push_back(nx);
            mesh.vertices.push_back(ny);
            mesh.vertices.push_back(nz);
            mesh.vertices.push_back(u);
            mesh.vertices.push_back(1.0f - v);
        }
    }
            for (int r = 0; r < rings; ++r) {
            for (int s = 0; s < segs; ++s) {
                uint16_t i0 = (uint16_t)(r * (segs + 1) + s);
                uint16_t i1 = (uint16_t)(i0 + 1);
                uint16_t i2 = (uint16_t)(i0 + segs + 1);
                uint16_t i3 = (uint16_t)(i2 + 1);
                mesh.indices.push_back(i0); mesh.indices.push_back(i1); mesh.indices.push_back(i2); // Fixed winding order
                mesh.indices.push_back(i1); mesh.indices.push_back(i3); mesh.indices.push_back(i2); // Fixed winding order
            }
        }
}
