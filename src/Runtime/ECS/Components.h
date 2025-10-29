#pragma once

#include <cstdint>
#include "AdvancedMath.hpp"

namespace Diligent { namespace ECS {

// Basic Transform with TRS
struct Transform {
    Diligent::float3 position {0,0,0};
    Diligent::float3 rotationEuler {0,0,0}; // radians
    Diligent::float3 scale {1,1,1};

    Diligent::float4x4 WorldMatrix() const {
        using namespace Diligent;
        float4x4 T = float4x4::Translation(position.x, position.y, position.z);
        float4x4 R = float4x4::RotationX(rotationEuler.x) * float4x4::RotationY(rotationEuler.y) * float4x4::RotationZ(rotationEuler.z);
        float4x4 S = float4x4::Scale(scale.x, scale.y, scale.z);
        return R * S * T;
    }
};

// Human-readable name
struct Name {
    std::string value;
};

// Static mesh reference (for built-in meshes initially)
struct StaticMesh {
    enum class MeshType { Cube } type { MeshType::Cube };
};

// General Mesh component: can be a built-in static mesh or a dynamic imported model (glTF)
struct Mesh {
    enum class Kind { Static, Dynamic } kind { Kind::Static };
    // Static built-in type
    enum class StaticType { Cube } staticType { StaticType::Cube };
    // Dynamic asset identifier (e.g., path to glTF or cache id). Empty if none assigned.
    std::string assetId;
};

// Simple directional light tag; could be extended to per-entity
struct DirectionalLight {
    Diligent::float3 direction { -0.49f, -0.60f, 0.64f };
    float intensity {1.0f};
};

// Camera tag (optional settings)
struct Camera {
    float fovYRadians { Diligent::PI_F / 4.0f };
    float nearZ { 0.1f };
    float farZ  { 100.0f };
};

}} // namespace Diligent::ECS
