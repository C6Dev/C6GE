#pragma once

#include <entt/entt.hpp>
#include "AdvancedMath.hpp"

namespace Diligent { class C6GERender; }
namespace Diligent { namespace ECS { class World; struct Transform; struct StaticMesh; struct Mesh; } }

namespace Diligent { namespace C6GE { namespace Systems {

class RenderSystem {
public:
    explicit RenderSystem(C6GERender* renderer) : m_Renderer(renderer) {}

    // Draw shadow pass for renderables
    void RenderShadows(ECS::World& world, const Diligent::float4x4& lightViewProj);
    // Draw regular pass
    void RenderScene(ECS::World& world, const Diligent::float4x4& cameraViewProj,
                     Diligent::RESOURCE_STATE_TRANSITION_MODE TransitionMode);

private:
    C6GERender* m_Renderer = nullptr; // not owned
};

}}} // namespace Diligent::C6GE::Systems
