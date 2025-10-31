#include "RenderSystem.h"
#include "Render/Render.h"
#include "Runtime/ECS/World.h"
#include "Runtime/ECS/Components.h"

using namespace Diligent;

namespace Diligent { namespace C6GE { namespace Systems {

void RenderSystem::RenderShadows(ECS::World& world, const float4x4& lightViewProj)
{
    auto& reg = world.Registry();
    // Shadow pass: only built-in cube casts into shadow via raster for now
    if (m_Renderer)
    {
        auto viewStatic = reg.view<ECS::Transform, ECS::StaticMesh>();
        for (auto e : viewStatic)
        {
            const auto& tr = viewStatic.get<ECS::Transform>(e);
            const auto& sm = viewStatic.get<ECS::StaticMesh>(e);
            if (sm.type == ECS::StaticMesh::MeshType::Cube)
            {
                m_Renderer->RenderCubeWithWorld(tr.WorldMatrix(), lightViewProj, true,
                                                RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            }
            else if (sm.type == ECS::StaticMesh::MeshType::Plane)
            {
                m_Renderer->RenderPlaneWithWorld(tr.WorldMatrix(), lightViewProj, true,
                                                 RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            }
        }
        auto viewMesh = reg.view<ECS::Transform, ECS::Mesh>();
        for (auto e : viewMesh)
        {
            const auto& tr = viewMesh.get<ECS::Transform>(e);
            const auto& mesh = viewMesh.get<ECS::Mesh>(e);
            if (mesh.kind == ECS::Mesh::Kind::Static)
            {
                if (mesh.staticType == ECS::Mesh::StaticType::Cube)
                {
                    m_Renderer->RenderCubeWithWorld(tr.WorldMatrix(), lightViewProj, true,
                                                    RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                }
                else if (mesh.staticType == ECS::Mesh::StaticType::Plane)
                {
                    m_Renderer->RenderPlaneWithWorld(tr.WorldMatrix(), lightViewProj, true,
                                                     RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                }
            }
            else if (mesh.kind == ECS::Mesh::Kind::Dynamic && !mesh.assetId.empty())
            {
                // Render dynamic glTF into shadow map (depth-only)
                m_Renderer->RenderGLTFShadowWithWorld(mesh.assetId, tr.WorldMatrix(), lightViewProj,
                                                      RESOURCE_STATE_TRANSITION_MODE_VERIFY);
            }
        }
    }
}

void RenderSystem::RenderScene(ECS::World& world, const float4x4& cameraViewProj,
                               RESOURCE_STATE_TRANSITION_MODE TransitionMode)
{
    auto& reg = world.Registry();
    if (!m_Renderer)
        return;
    {
        auto viewStatic = reg.view<ECS::Transform, ECS::StaticMesh>();
        for (auto e : viewStatic)
        {
            const auto& tr = viewStatic.get<ECS::Transform>(e);
            const auto& sm = viewStatic.get<ECS::StaticMesh>(e);
            if (sm.type == ECS::StaticMesh::MeshType::Cube)
            {
                m_Renderer->RenderCubeWithWorld(tr.WorldMatrix(), cameraViewProj, false, TransitionMode);
            }
            else if (sm.type == ECS::StaticMesh::MeshType::Plane)
            {
                m_Renderer->RenderPlaneWithWorld(tr.WorldMatrix(), cameraViewProj, false, TransitionMode);
            }
        }
    }
    {
        auto viewMesh = reg.view<ECS::Transform, ECS::Mesh>();
        for (auto e : viewMesh)
        {
            const auto& tr = viewMesh.get<ECS::Transform>(e);
            const auto& mesh = viewMesh.get<ECS::Mesh>(e);
            if (mesh.kind == ECS::Mesh::Kind::Static)
            {
                if (mesh.staticType == ECS::Mesh::StaticType::Cube)
                {
                    m_Renderer->RenderCubeWithWorld(tr.WorldMatrix(), cameraViewProj, false, TransitionMode);
                }
                else if (mesh.staticType == ECS::Mesh::StaticType::Plane)
                {
                    m_Renderer->RenderPlaneWithWorld(tr.WorldMatrix(), cameraViewProj, false, TransitionMode);
                }
            }
            else if (mesh.kind == ECS::Mesh::Kind::Dynamic && !mesh.assetId.empty())
            {
                // Delegate GLTF draw to renderer
                // Renderer will handle GLTF renderer begin state internally
                // (Transition mode verify to avoid extra barriers inside loop)
                m_Renderer->RenderGLTFWithWorld(mesh.assetId, tr.WorldMatrix(), cameraViewProj,
                                                RESOURCE_STATE_TRANSITION_MODE_VERIFY);
            }
        }
    }
}

}}} // namespace
