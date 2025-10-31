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
    enum class MeshType { Cube, Plane } type { MeshType::Cube };
};

// General Mesh component: can be a built-in static mesh or a dynamic imported model (glTF)
struct Mesh {
    enum class Kind { Static, Dynamic } kind { Kind::Static };
    // Static built-in type
    enum class StaticType { Cube, Plane } staticType { StaticType::Cube };
    // Dynamic asset identifier (e.g., path to glTF or cache id). Empty if none assigned.
    std::string assetId;
};

// Simple directional light tag; could be extended to per-entity
struct DirectionalLight {
    // Aim slightly backward by default so the scene is lit from above and behind the camera
    Diligent::float3 direction { -0.35f, -0.80f, -0.48f };
    float intensity {1.0f};
};

// Point light (uses entity Transform for position)
struct PointLight {
    Diligent::float3 color {1.0f, 1.0f, 1.0f};
    float intensity {1.0f};
    float range {10.0f};
};

// Spot light (uses entity Transform for position; direction field for spot aim)
struct SpotLight {
    Diligent::float3 direction {0.0f, -1.0f, 0.0f};
    float angleDegrees {30.0f};
    Diligent::float3 color {1.0f, 1.0f, 1.0f};
    float intensity {1.0f};
    float range {15.0f};
};

// Camera tag (optional settings)
struct Camera {
    float fovYRadians { Diligent::PI_F / 4.0f };
    float nearZ { 0.1f };
    float farZ  { 100.0f };
};

}} // namespace Diligent::ECS
