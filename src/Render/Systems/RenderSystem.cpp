#include "RenderSystem.h"
#include "Render/Render.h"
#include "Runtime/ECS/World.h"
#include "Runtime/ECS/Components.h"

using namespace Diligent;

namespace Diligent { namespace C6GE { namespace Systems {

void RenderSystem::RenderShadows(ECS::World& world, const float4x4& lightViewProj)
{
    auto& reg = world.Registry();
    auto view = reg.view<ECS::Transform, ECS::StaticMesh>();
    for (auto e : view)
    {
        const auto& tr = view.get<ECS::Transform>(e);
        const auto& sm = view.get<ECS::StaticMesh>(e);
        if (m_Renderer && sm.type == ECS::StaticMesh::MeshType::Cube)
            m_Renderer->RenderCubeWithWorld(tr.WorldMatrix(), lightViewProj, true,
                                            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
}

void RenderSystem::RenderScene(ECS::World& world, const float4x4& cameraViewProj,
                               RESOURCE_STATE_TRANSITION_MODE TransitionMode)
{
    auto& reg = world.Registry();
    auto view = reg.view<ECS::Transform, ECS::StaticMesh>();
    for (auto e : view)
    {
        const auto& tr = view.get<ECS::Transform>(e);
        const auto& sm = view.get<ECS::StaticMesh>(e);
        if (m_Renderer && sm.type == ECS::StaticMesh::MeshType::Cube)
            m_Renderer->RenderCubeWithWorld(tr.WorldMatrix(), cameraViewProj, false, TransitionMode);
    }
}

}}} // namespace
