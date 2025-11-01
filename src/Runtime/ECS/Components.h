#pragma once

#include <cstdint>
#include <string>

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
    Diligent::float3 direction { -0.49f, -0.60f, 0.64f };
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

// Image-based sky parameters (uses HDR environment maps for lighting)
struct Sky {
    bool        enabled { true };
    std::string environmentPath;
    float       intensity { 1.0f };
    float       exposure  { 1.0f };
    float       rotationDegrees { 0.0f };
    bool        showBackground { true };
};

// Simple exponential height fog
struct Fog {
    bool             enabled { false };
    Diligent::float3 color { 0.6f, 0.7f, 0.8f };
    float            density { 0.02f };
    float            startDistance { 20.0f };
    float            maxDistance { 150.0f };
    float            heightFalloff { 0.01f };
};

// Ambient sky light contribution
struct SkyLight {
    Diligent::float3 color { 0.25f, 0.28f, 0.32f };
    float            intensity { 1.0f };
    bool             enabled { true };
};

}} // namespace Diligent::ECS
