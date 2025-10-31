#include "RenderCommon.hpp"

namespace Diligent

{

    void C6GERender::SetHostWindow(GLFWwindow* window)
    {
        m_HostWindow = window;
    }

    using RenderInternals::Constants;
    using RenderInternals::MAX_POINT_LIGHTS;
    using RenderInternals::MAX_SPOT_LIGHTS;
    using RenderInternals::RTConstantsCPU;

    void C6GERender::RenderShadowMap()
    {
        // Compute world->light projection matrix and store to m_WorldToShadowMapUVDepthMatr
        float3 f3LightSpaceX, f3LightSpaceY, f3LightSpaceZ;
        f3LightSpaceZ = normalize(m_LightDirection);

        float min_cmp = std::min(std::min(std::abs(m_LightDirection.x), std::abs(m_LightDirection.y)), std::abs(m_LightDirection.z));
        if (min_cmp == std::abs(m_LightDirection.x))
            f3LightSpaceX = float3(1,0,0);
        else if (min_cmp == std::abs(m_LightDirection.y))
            f3LightSpaceX = float3(0,1,0);
        else
            f3LightSpaceX = float3(0,0,1);

        f3LightSpaceY = cross(f3LightSpaceZ, f3LightSpaceX);
        f3LightSpaceX = cross(f3LightSpaceY, f3LightSpaceZ);
        f3LightSpaceX = normalize(f3LightSpaceX);
        f3LightSpaceY = normalize(f3LightSpaceY);

        float4x4 WorldToLightViewSpaceMatr = float4x4::ViewFromBasis(f3LightSpaceX, f3LightSpaceY, f3LightSpaceZ);

    float3 f3SceneCenter = float3(0,0,0);
    // Increase scene radius to include the plane (plane extends to +/-5.0 in X/Z and is at Y=-2)
    float SceneRadius = 6.0f;
    float3 f3MinXYZ = f3SceneCenter - float3(SceneRadius, SceneRadius, SceneRadius);
    // Previously Z extent was scaled by 5 which skewed the light projection
    // and produced badly scaled shadows. Use symmetric extents here.
    float3 f3MaxXYZ = f3SceneCenter + float3(SceneRadius, SceneRadius, SceneRadius);
    float3 f3SceneExtent = f3MaxXYZ - f3MinXYZ;

        const RenderDeviceInfo& DevInfo = m_pDevice->GetDeviceInfo();
        const bool IsGL = DevInfo.IsGLDevice();
        float4 f4LightSpaceScale;
        f4LightSpaceScale.x = 2.f / f3SceneExtent.x;
        f4LightSpaceScale.y = 2.f / f3SceneExtent.y;
        f4LightSpaceScale.z = (IsGL ? 2.f : 1.f) / f3SceneExtent.z;

        float4 f4LightSpaceScaledBias;
        f4LightSpaceScaledBias.x = -f3MinXYZ.x * f4LightSpaceScale.x - 1.f;
        f4LightSpaceScaledBias.y = -f3MinXYZ.y * f4LightSpaceScale.y - 1.f;
        f4LightSpaceScaledBias.z = -f3MinXYZ.z * f4LightSpaceScale.z + (IsGL ? -1.f : 0.f);

        float4x4 ScaleMatrix = float4x4::Scale(f4LightSpaceScale.x, f4LightSpaceScale.y, f4LightSpaceScale.z);
        float4x4 ScaledBiasMatrix = float4x4::Translation(f4LightSpaceScaledBias.x, f4LightSpaceScaledBias.y, f4LightSpaceScaledBias.z);

        float4x4 ShadowProjMatr = ScaleMatrix * ScaledBiasMatrix;
        float4x4 WorldToLightProjSpaceMatr = WorldToLightViewSpaceMatr * ShadowProjMatr;

        const NDCAttribs& NDC = DevInfo.GetNDCAttribs();
        float4x4 ProjToUVScale = float4x4::Scale(0.5f, NDC.YtoVScale, NDC.ZtoDepthScale);
        float4x4 ProjToUVBias = float4x4::Translation(0.5f, 0.5f, NDC.GetZtoDepthBias());

        m_WorldToShadowMapUVDepthMatr = WorldToLightProjSpaceMatr * ProjToUVScale * ProjToUVBias;

        // Render shadow casters from ECS
        if (m_RenderSystem && m_World)
        {
            m_RenderSystem->RenderShadows(*m_World, WorldToLightProjSpaceMatr);
        }
    }

    void C6GERender::RenderCube(const float4x4& CameraViewProj, bool IsShadowPass, RESOURCE_STATE_TRANSITION_MODE TransitionMode)
    {
        // Update constant buffer
        {
            MapHelper<Constants> CBConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
            CBConstants->g_World         = m_CubeWorldMatrix;
            CBConstants->g_WorldViewProj = (m_CubeWorldMatrix * CameraViewProj);
            // Compute normal transform as inverse-transpose of the world matrix
            float4x4 NormalMatrix = m_CubeWorldMatrix.RemoveTranslation().Inverse().Transpose();
            CBConstants->g_NormalTranform = NormalMatrix;
            CBConstants->g_DirLight = float4{m_LightDirection.x, m_LightDirection.y, m_LightDirection.z, m_HasDirectionalLight ? m_DirLightIntensity : 0.0f};
            CBConstants->g_Ambient  = float4{0,0,0,0};
            const Uint32 pcnt = std::min<Uint32>(static_cast<Uint32>(m_FramePointLights.size()), MAX_POINT_LIGHTS);
            const Uint32 scnt = std::min<Uint32>(static_cast<Uint32>(m_FrameSpotLights.size()),  MAX_SPOT_LIGHTS);
            CBConstants->g_NumPointLights = pcnt;
            CBConstants->g_NumSpotLights  = scnt;
            for (Uint32 i = 0; i < pcnt; ++i) CBConstants->g_PointLights[i] = {
                m_FramePointLights[i].position, m_FramePointLights[i].range,
                m_FramePointLights[i].color,    m_FramePointLights[i].intensity
            };
            for (Uint32 i = pcnt; i < MAX_POINT_LIGHTS; ++i) CBConstants->g_PointLights[i] = {};
            for (Uint32 i = 0; i < scnt; ++i) CBConstants->g_SpotLights[i] = {
                m_FrameSpotLights[i].position, m_FrameSpotLights[i].range,
                m_FrameSpotLights[i].color,    m_FrameSpotLights[i].intensity,
                m_FrameSpotLights[i].direction, m_FrameSpotLights[i].spotCos
            };
            for (Uint32 i = scnt; i < MAX_SPOT_LIGHTS; ++i) CBConstants->g_SpotLights[i] = {};
        }

        IBuffer* pBuffs[] = {m_CubeVertexBuffer};
        m_pImmediateContext->SetVertexBuffers(0, 1, pBuffs, nullptr, TransitionMode, SET_VERTEX_BUFFERS_FLAG_RESET);
        m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0, TransitionMode);

        if (IsShadowPass)
        {
            if (!m_pCubeShadowPSO)
            {
                std::cerr << "[C6GE] Error: Cube shadow PSO is null; skipping cube shadow draw." << std::endl;
                return;
            }
            m_pImmediateContext->SetPipelineState(m_pCubeShadowPSO);
            if (m_CubeShadowSRB)
                m_pImmediateContext->CommitShaderResources(m_CubeShadowSRB, TransitionMode);
        }
        else
        {
            if (!m_pCubePSO)
            {
                std::cerr << "[C6GE] Error: Cube PSO is null; skipping cube draw." << std::endl;
                return;
            }
            m_pImmediateContext->SetPipelineState(m_pCubePSO);
            if (m_CubeSRB)
                m_pImmediateContext->CommitShaderResources(m_CubeSRB, TransitionMode);
        }

        DrawIndexedAttribs DrawAttrs;
        DrawAttrs.IndexType = VT_UINT32;
        DrawAttrs.NumIndices = 36;
        DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
        m_pImmediateContext->DrawIndexed(DrawAttrs);
    }

    // Helper to render with explicit world matrix (used by ECS systems), without touching member m_CubeWorldMatrix
    void C6GERender::RenderCubeWithWorld(const float4x4& World, const float4x4& CameraViewProj, bool IsShadowPass,
                                         RESOURCE_STATE_TRANSITION_MODE TransitionMode)
    {
        // Update constant buffer
        {
            MapHelper<Constants> CBConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
            CBConstants->g_World         = World;
            CBConstants->g_WorldViewProj = (World * CameraViewProj);
            float4x4 NormalMatrix        = World.RemoveTranslation().Inverse().Transpose();
            CBConstants->g_NormalTranform = NormalMatrix;
            CBConstants->g_DirLight = float4{m_LightDirection.x, m_LightDirection.y, m_LightDirection.z, m_HasDirectionalLight ? m_DirLightIntensity : 0.0f};
            CBConstants->g_Ambient  = float4{0,0,0,0};
            const Uint32 pcnt = std::min<Uint32>(static_cast<Uint32>(m_FramePointLights.size()), MAX_POINT_LIGHTS);
            const Uint32 scnt = std::min<Uint32>(static_cast<Uint32>(m_FrameSpotLights.size()),  MAX_SPOT_LIGHTS);
            CBConstants->g_NumPointLights = pcnt;
            CBConstants->g_NumSpotLights  = scnt;
            for (Uint32 i = 0; i < pcnt; ++i) CBConstants->g_PointLights[i] = {
                m_FramePointLights[i].position, m_FramePointLights[i].range,
                m_FramePointLights[i].color,    m_FramePointLights[i].intensity
            };
            for (Uint32 i = pcnt; i < MAX_POINT_LIGHTS; ++i) CBConstants->g_PointLights[i] = {};
            for (Uint32 i = 0; i < scnt; ++i) CBConstants->g_SpotLights[i] = {
                m_FrameSpotLights[i].position, m_FrameSpotLights[i].range,
                m_FrameSpotLights[i].color,    m_FrameSpotLights[i].intensity,
                m_FrameSpotLights[i].direction, m_FrameSpotLights[i].spotCos
            };
            for (Uint32 i = scnt; i < MAX_SPOT_LIGHTS; ++i) CBConstants->g_SpotLights[i] = {};
        }

        IBuffer* pBuffs[] = {m_CubeVertexBuffer};
        m_pImmediateContext->SetVertexBuffers(0, 1, pBuffs, nullptr, TransitionMode, SET_VERTEX_BUFFERS_FLAG_RESET);
        m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0, TransitionMode);

        if (IsShadowPass)
        {
            if (!m_pCubeShadowPSO)
                return;
            m_pImmediateContext->SetPipelineState(m_pCubeShadowPSO);
            if (m_CubeShadowSRB)
                m_pImmediateContext->CommitShaderResources(m_CubeShadowSRB, TransitionMode);
        }
        else
        {
            if (!m_pCubePSO)
                return;
            m_pImmediateContext->SetPipelineState(m_pCubePSO);
            if (m_CubeSRB)
                m_pImmediateContext->CommitShaderResources(m_CubeSRB, TransitionMode);
        }

        DrawIndexedAttribs DrawAttrs;
        DrawAttrs.IndexType = VT_UINT32;
        DrawAttrs.NumIndices = 36;
        DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
        m_pImmediateContext->DrawIndexed(DrawAttrs);
    }

    // Draw a static plane (quad) using the same PSOs as the cube. The plane VB includes pos/normal/uv.
    void C6GERender::RenderPlaneWithWorld(const float4x4& World, const float4x4& CameraViewProj, bool IsShadowPass,
                                          RESOURCE_STATE_TRANSITION_MODE TransitionMode)
    {
        if (!m_PlaneVertexBuffer || !m_PlaneIndexBuffer)
            return;
        {
            MapHelper<Constants> CBConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
            CBConstants->g_World         = World;
            CBConstants->g_WorldViewProj = (World * CameraViewProj);
            CBConstants->g_NormalTranform = World.RemoveTranslation().Inverse().Transpose();
            CBConstants->g_DirLight = float4{m_LightDirection.x, m_LightDirection.y, m_LightDirection.z, m_HasDirectionalLight ? m_DirLightIntensity : 0.0f};
            CBConstants->g_Ambient  = float4{0,0,0,0};
            const Uint32 pcnt = std::min<Uint32>(static_cast<Uint32>(m_FramePointLights.size()), MAX_POINT_LIGHTS);
            const Uint32 scnt = std::min<Uint32>(static_cast<Uint32>(m_FrameSpotLights.size()),  MAX_SPOT_LIGHTS);
            CBConstants->g_NumPointLights = pcnt;
            CBConstants->g_NumSpotLights  = scnt;
            for (Uint32 i = 0; i < pcnt; ++i) CBConstants->g_PointLights[i] = {
                m_FramePointLights[i].position, m_FramePointLights[i].range,
                m_FramePointLights[i].color,    m_FramePointLights[i].intensity
            };
            for (Uint32 i = pcnt; i < MAX_POINT_LIGHTS; ++i) CBConstants->g_PointLights[i] = {};
            for (Uint32 i = 0; i < scnt; ++i) CBConstants->g_SpotLights[i] = {
                m_FrameSpotLights[i].position, m_FrameSpotLights[i].range,
                m_FrameSpotLights[i].color,    m_FrameSpotLights[i].intensity,
                m_FrameSpotLights[i].direction, m_FrameSpotLights[i].spotCos
            };
            for (Uint32 i = scnt; i < MAX_SPOT_LIGHTS; ++i) CBConstants->g_SpotLights[i] = {};
        }

        IBuffer* pVB[] = {m_PlaneVertexBuffer};
        m_pImmediateContext->SetVertexBuffers(0, 1, pVB, nullptr, TransitionMode, SET_VERTEX_BUFFERS_FLAG_RESET);
        m_pImmediateContext->SetIndexBuffer(m_PlaneIndexBuffer, 0, TransitionMode);

        if (IsShadowPass)
        {
            if (!m_pCubeShadowPSO)
                return;
            m_pImmediateContext->SetPipelineState(m_pCubeShadowPSO);
            if (m_CubeShadowSRB)
                m_pImmediateContext->CommitShaderResources(m_CubeShadowSRB, TransitionMode);
        }
        else
        {
            if (!m_pCubePSO)
                return;
            m_pImmediateContext->SetPipelineState(m_pCubePSO);
            if (m_CubeSRB)
                m_pImmediateContext->CommitShaderResources(m_CubeSRB, TransitionMode);
        }

        DrawIndexedAttribs DrawAttrs;
        DrawAttrs.IndexType = VT_UINT32;
        DrawAttrs.NumIndices = 6;
        DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
        m_pImmediateContext->DrawIndexed(DrawAttrs);
    }

    void C6GERender::RenderPlane(RESOURCE_STATE_TRANSITION_MODE TransitionMode)
    {
        struct PlaneConsts
        {
            float4x4 CameraViewProj;
            float4x4 WorldToShadowMapUVDepth;
            float4   LightDirection;
        };
        MapHelper<PlaneConsts> CBConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        CBConstants->CameraViewProj = m_CameraViewProjMatrix;
        CBConstants->WorldToShadowMapUVDepth = m_WorldToShadowMapUVDepthMatr;
        CBConstants->LightDirection = m_LightDirection;

        // Choose appropriate PSO and commit resources safely
        IPipelineState* pSelectedPSO = nullptr;
        bool commitSRB = false;
        // If shadows are disabled, always use the no-shadow PSO when available
        if (!RenderShadows)
        {
            if (m_pPlaneNoShadowPSO)
            {
                pSelectedPSO = m_pPlaneNoShadowPSO;
                commitSRB = false;
            }
            else
            {
                std::cerr << "[C6GE] Warning: Shadows are disabled but no-shadow PSO is missing; skipping plane draw." << std::endl;
                return;
            }
        }
        else
        {
            if (m_pPlanePSO && m_PlaneSRB)
            {
                pSelectedPSO = m_pPlanePSO;
                commitSRB = true;
            }
            else if (m_pPlanePSO)
            {
                // Use plane PSO but will attempt to create SRB fallback later
                pSelectedPSO = m_pPlanePSO;
                commitSRB = true;
            }
            else
            {
                std::cerr << "[C6GE] Warning: Plane PSO is not available; skipping plane draw." << std::endl;
                return;
            }
        }

        if (!pSelectedPSO)
        {
            std::cerr << "[C6GE] Error: No valid Plane PSO available; skipping plane draw." << std::endl;
            return;
        }

        // If the selected PSO is the plane PSO and we don't have an SRB yet,
        // try to create a fallback SRB so CommitShaderResources can be called.
        if (pSelectedPSO == m_pPlanePSO && !m_PlaneSRB)
        {
            std::cerr << "[C6GE] Warning: Plane SRB missing at draw time; attempting to create fallback SRB." << std::endl;
            m_pPlanePSO->CreateShaderResourceBinding(&m_PlaneSRB, true);
            if (m_PlaneSRB && m_ShadowMapSRV)
            {
                auto* var = m_PlaneSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_ShadowMap");
                if (var)
                    var->Set(m_ShadowMapSRV);
            }
            if (!m_PlaneSRB)
            {
                std::cerr << "[C6GE] Error: Failed to create fallback Plane SRB; skipping plane draw to avoid device context assert." << std::endl;
                return;
            }
        }

        // If using the shadowing Plane PSO, ensure SRB exists and commit it after setting the PSO.
        if (pSelectedPSO == m_pPlanePSO)
        {
            if (!m_PlaneSRB)
            {
                // Attempt to create the SRB (it may have been created at PSO init already)
                m_pPlanePSO->CreateShaderResourceBinding(&m_PlaneSRB, true);
                if (m_PlaneSRB && m_ShadowMapSRV)
                {
                    auto* var = m_PlaneSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_ShadowMap");
                    if (var)
                        var->Set(m_ShadowMapSRV);
                }
            }

            if (!m_PlaneSRB)
            {
                std::cerr << "[C6GE] Error: Plane SRB missing; skipping plane draw to avoid device context assert." << std::endl;
                return;
            }

            m_pImmediateContext->SetPipelineState(pSelectedPSO);
            m_pImmediateContext->CommitShaderResources(m_PlaneSRB, TransitionMode);
        }
        else
        {
            // No-shadow PSO: ensure its SRB (if any) is committed to satisfy backend validation
            if (m_PlaneNoShadowSRB)
            {
                m_pImmediateContext->CommitShaderResources(m_PlaneNoShadowSRB, TransitionMode);
            }
            m_pImmediateContext->SetPipelineState(pSelectedPSO);
        }

        DrawAttribs DrawAttrs(4, DRAW_FLAG_VERIFY_ALL);
        m_pImmediateContext->Draw(DrawAttrs);
    }

    // Shadow-map visualization removed; no RenderShadowMapVis implementation.

    // Render a frame
    void C6GERender::Render()
    {
        // If a resize happened, mirror the ImGui toggle semantics at a safe point in the frame
        if (m_PendingRTRestart && m_RayTracingSupported)
        {
            if (m_EnableRayTracing)
            {
                if (m_RayTracingInitialized)
                    DestroyRayTracing();
                InitializeRayTracing();
            }
            m_PendingRTRestart = false;
        }

        // Guard: Only render if framebuffer has valid size
        if (m_FramebufferWidth == 0 || m_FramebufferHeight == 0 || !m_pFramebufferRTV || !m_pFramebufferDSV)
        {
            return;
        }

        // Optional hybrid rendering path: update TLAS and prepare RT contributions
        if (m_EnableRayTracing && m_RayTracingSupported && m_RayTracingInitialized)
        {
            UpdateTLAS();
            RenderRayTracingPath();
        }

        // Update light(s) from ECS: gather dir/point/spot; no defaults
        m_HasDirectionalLight = false;
        m_LightDirection = float3{0,0,0};
        m_DirLightIntensity = 0.0f;
        m_FramePointLights.clear();
        m_FrameSpotLights.clear();
        if (m_World)
        {
            auto& reg = m_World->Registry();
            // Directional
            {
                auto viewDir = reg.view<ECS::DirectionalLight>();
                viewDir.each([&](const entt::entity e, const ECS::DirectionalLight& dl) {
                    float3 d = dl.direction;
                    if (length(d) > 0.0001f && !m_HasDirectionalLight)
                    {
                        m_LightDirection      = normalize(d);
                        m_DirLightIntensity   = std::max(0.0f, dl.intensity);
                        m_HasDirectionalLight = true;
                    }
                });
            }

            // Point lights
            {
                auto viewPL = reg.view<ECS::Transform, ECS::PointLight>();
                viewPL.each([&](const entt::entity e, const ECS::Transform& tr, const ECS::PointLight& pl) {
                    if (m_FramePointLights.size() >= MAX_POINT_LIGHTS)
                        return; // continue
                    PointLightData L{};
                    L.position  = tr.position;
                    L.range     = pl.range;
                    L.color     = pl.color;
                    L.intensity = pl.intensity;
                    m_FramePointLights.push_back(L);
                });
            }
            // Spot lights
            {
                auto viewSL = reg.view<ECS::Transform, ECS::SpotLight>();
                viewSL.each([&](const entt::entity e, const ECS::Transform& tr, const ECS::SpotLight& sl) {
                    if (m_FrameSpotLights.size() >= MAX_SPOT_LIGHTS)
                        return; // continue
                    SpotLightData L{};
                    L.position  = tr.position;
                    L.range     = sl.range;
                    L.color     = sl.color;
                    L.intensity = sl.intensity;
                    L.direction = length(sl.direction) > 0.0001f ? normalize(sl.direction) : float3{0,-1,0};
                    const float rad = sl.angleDegrees * PI_F / 180.0f;
                    L.spotCos   = std::cos(rad);
                    m_FrameSpotLights.push_back(L);
                });
            }
        }

        // 1) Render shadow map (only when we have a directional light)
        if (RenderShadows && m_ShadowMapDSV && m_HasDirectionalLight)
        {
            m_pImmediateContext->SetRenderTargets(0, nullptr, m_ShadowMapDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            m_pImmediateContext->ClearDepthStencil(m_ShadowMapDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            RenderShadowMap();
        }

        // 2) Render scene into off-screen framebuffer for ImGui viewport
        ITextureView *pRTV = (m_SampleCount > 1 && m_pMSColorRTV) ? m_pMSColorRTV : m_pFramebufferRTV;
        ITextureView *pDSV = (m_SampleCount > 1 && m_pMSDepthDSV) ? m_pMSDepthDSV : m_pFramebufferDSV;
        float4 ClearColor = {0.0, 0.0, 0.0, 0.0f};
        if (m_ConvertPSOutputToGamma)
            ClearColor = LinearToSRGB(ClearColor);

        // Use render pass if available and supported
        if (m_UseRenderPasses && m_pMainRenderPass)
        {
            IFramebuffer* pFramebuffer = GetCurrentMainFramebuffer();
            if (pFramebuffer)
            {
                BeginRenderPassAttribs RPBeginInfo;
                RPBeginInfo.pRenderPass  = m_pMainRenderPass;
                RPBeginInfo.pFramebuffer = pFramebuffer;

                OptimizedClearValue ClearValues[2];
                ClearValues[0].Color[0] = ClearColor.r;
                ClearValues[0].Color[1] = ClearColor.g;
                ClearValues[0].Color[2] = ClearColor.b;
                ClearValues[0].Color[3] = ClearColor.a;
                ClearValues[1].DepthStencil.Depth = 1.f;

                RPBeginInfo.pClearValues        = ClearValues;
                RPBeginInfo.ClearValueCount     = 2;
                RPBeginInfo.StateTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
                
                m_pImmediateContext->BeginRenderPass(RPBeginInfo);

                // Render entities via ECS (inside render pass, use VERIFY)
                if (m_RenderSystem && m_World)
                    m_RenderSystem->RenderScene(*m_World, m_CameraViewProjMatrix, RESOURCE_STATE_TRANSITION_MODE_VERIFY);

                // Composite RT output multiplicatively for shadows
                if (m_EnableRayTracing && m_RayTracingSupported && m_RayTracingInitialized && m_pRTCompositePSO && m_RTCompositeSRB && m_pRTOutputSRV)
                {
                    if (auto* var = m_RTCompositeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_RTOutput"))
                        var->Set(m_pRTOutputSRV, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
                    m_pImmediateContext->SetPipelineState(m_pRTCompositePSO);
                    m_pImmediateContext->CommitShaderResources(m_RTCompositeSRB, RESOURCE_STATE_TRANSITION_MODE_VERIFY);
                    DrawAttribs fsTri{3, DRAW_FLAG_VERIFY_ALL};
                    m_pImmediateContext->Draw(fsTri);
                }

                // Then additively composite reflections if enabled
                if (m_EnableRayTracing && m_EnableRTReflections && m_RayTracingSupported && m_RayTracingInitialized && m_pRTAddCompositePSO && m_RTAddCompositeSRB && m_pRTOutputSRV)
                {
                    if (auto* var = m_RTAddCompositeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_RTOutput"))
                        var->Set(m_pRTOutputSRV, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
                    m_pImmediateContext->SetPipelineState(m_pRTAddCompositePSO);
                    m_pImmediateContext->CommitShaderResources(m_RTAddCompositeSRB, RESOURCE_STATE_TRANSITION_MODE_VERIFY);
                    DrawAttribs fsTri{3, DRAW_FLAG_VERIFY_ALL};
                    m_pImmediateContext->Draw(fsTri);
                }

                m_pImmediateContext->EndRenderPass();
            }
            else
            {
                // Fallback to traditional rendering
                m_pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

                if (m_RenderSystem && m_World)
                    m_RenderSystem->RenderScene(*m_World, m_CameraViewProjMatrix, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

                if (m_EnableRayTracing && m_RayTracingSupported && m_RayTracingInitialized && m_pRTCompositePSO && m_RTCompositeSRB && m_pRTOutputSRV)
                {
                    if (auto* var = m_RTCompositeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_RTOutput"))
                        var->Set(m_pRTOutputSRV, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
                    m_pImmediateContext->SetPipelineState(m_pRTCompositePSO);
                    m_pImmediateContext->CommitShaderResources(m_RTCompositeSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                    DrawAttribs fsTri{3, DRAW_FLAG_VERIFY_ALL};
                    m_pImmediateContext->Draw(fsTri);
                }

                if (m_EnableRayTracing && m_EnableRTReflections && m_RayTracingSupported && m_RayTracingInitialized && m_pRTAddCompositePSO && m_RTAddCompositeSRB && m_pRTOutputSRV)
                {
                    if (auto* var = m_RTAddCompositeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_RTOutput"))
                        var->Set(m_pRTOutputSRV, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
                    m_pImmediateContext->SetPipelineState(m_pRTAddCompositePSO);
                    m_pImmediateContext->CommitShaderResources(m_RTAddCompositeSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                    DrawAttribs fsTri{3, DRAW_FLAG_VERIFY_ALL};
                    m_pImmediateContext->Draw(fsTri);
                }
            }
        }
        else
        {
            // Traditional rendering without render passes
            m_pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            if (m_RenderSystem && m_World)
                m_RenderSystem->RenderScene(*m_World, m_CameraViewProjMatrix, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            if (m_EnableRayTracing && m_RayTracingSupported && m_RayTracingInitialized && m_pRTCompositePSO && m_RTCompositeSRB && m_pRTOutputSRV)
            {
                if (auto* var = m_RTCompositeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_RTOutput"))
                    var->Set(m_pRTOutputSRV, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
                m_pImmediateContext->SetPipelineState(m_pRTCompositePSO);
                m_pImmediateContext->CommitShaderResources(m_RTCompositeSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                DrawAttribs fsTri{3, DRAW_FLAG_VERIFY_ALL};
                m_pImmediateContext->Draw(fsTri);
            }

            if (m_EnableRayTracing && m_EnableRTReflections && m_RayTracingSupported && m_RayTracingInitialized && m_pRTAddCompositePSO && m_RTAddCompositeSRB && m_pRTOutputSRV)
            {
                if (auto* var = m_RTAddCompositeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_RTOutput"))
                    var->Set(m_pRTOutputSRV, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
                m_pImmediateContext->SetPipelineState(m_pRTAddCompositePSO);
                m_pImmediateContext->CommitShaderResources(m_RTAddCompositeSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                DrawAttribs fsTri{3, DRAW_FLAG_VERIFY_ALL};
                m_pImmediateContext->Draw(fsTri);
            }
        }

        // If using MSAA, resolve the multi-sampled render target to the framebuffer
        if (m_SampleCount > 1 && m_pMSColorRTV && m_pFramebufferTexture)
        {
            ResolveTextureSubresourceAttribs ResolveAttribs;
            ResolveAttribs.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
            ResolveAttribs.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
            m_pImmediateContext->ResolveTextureSubresource(m_pMSColorRTV->GetTexture(), m_pFramebufferTexture, ResolveAttribs);
        }

        // 3) Post-processing: simple gamma correction pass into dedicated post-fx texture
        // Before gamma, run TAA if enabled
        ITextureView* pAfterAA_SRV = m_pFramebufferSRV;
    if (m_EnableTAA && m_TAA && m_PostFXContext)
        {
            // Prepare frame desc based on framebuffer size
            PostFXContext::FrameDesc Frame{};
            Frame.Width        = m_FramebufferWidth;
            Frame.Height       = m_FramebufferHeight;
            Frame.OutputWidth  = m_FramebufferWidth;
            Frame.OutputHeight = m_FramebufferHeight;
            Frame.Index        = static_cast<Uint64>(m_TemporalFrameIndex++);

            // Prepare resources
            m_PostFXContext->PrepareResources(m_pDevice, Frame, PostFXContext::FEATURE_FLAG_NONE);
            TemporalAntiAliasing::FEATURE_FLAGS TAAFeatures = TemporalAntiAliasing::FEATURE_FLAG_BICUBIC_FILTER;
            m_TAA->PrepareResources(m_pDevice, m_pImmediateContext, m_PostFXContext.get(), TAAFeatures);

            // Execute TAA
            TemporalAntiAliasing::RenderAttributes TAAAttr{};
            TAAAttr.pDevice         = m_pDevice;
            TAAAttr.pDeviceContext  = m_pImmediateContext;
            TAAAttr.pPostFXContext  = m_PostFXContext.get();
            TAAAttr.pColorBufferSRV = m_pFramebufferSRV;
            TAAAttr.pTAAAttribs     = reinterpret_cast<TAASettings*>(&m_TAASettingsStorage);
            m_TAA->Execute(TAAAttr);

            // Use the current accumulated frame as the input for subsequent passes
            pAfterAA_SRV = m_TAA->GetAccumulatedFrameSRV(false);
            if (pAfterAA_SRV == nullptr)
                pAfterAA_SRV = m_pFramebufferSRV;
        }

        if (m_EnablePostProcessing && m_PostGammaCorrection && m_pPostRTV && m_pPostGammaPSO)
        {
            // Use or create an SRB bound to the specific input SRV to avoid updating descriptors in-flight
            RefCntAutoPtr<IShaderResourceBinding> pSRB;
            auto it = m_PostGammaSRBCache.find(pAfterAA_SRV);
            if (it != m_PostGammaSRBCache.end())
            {
                pSRB = it->second;
            }
            else
            {
                m_pPostGammaPSO->CreateShaderResourceBinding(&pSRB, true);
                if (pSRB)
                {
                    if (auto* var = pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_TextureColor"))
                        var->Set(pAfterAA_SRV);
                    m_PostGammaSRBCache.emplace(pAfterAA_SRV, pSRB);
                }
            }

            m_pImmediateContext->SetRenderTargets(1, &m_pPostRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            m_pImmediateContext->SetPipelineState(m_pPostGammaPSO);
            if (pSRB)
                m_pImmediateContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            DrawAttribs fsTri{3, DRAW_FLAG_VERIFY_ALL};
            m_pImmediateContext->Draw(fsTri);

            // Unbind to avoid hazards on some backends
            ITextureView* nullRTV = nullptr;
            m_pImmediateContext->SetRenderTargets(0, &nullRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }

        // Shadow visualization removed. No additional overlays.

        // Decide which SRV the viewport should display this frame and cache it
        {
            ITextureView* pDisplay = nullptr;
            if (m_EnablePostProcessing && m_PostGammaCorrection && m_pPostSRV)
            {
                pDisplay = m_pPostSRV;
            }
            else if (m_EnableTAA && m_TAA)
            {
                pDisplay = pAfterAA_SRV ? pAfterAA_SRV : m_pFramebufferSRV;
            }
            else
            {
                pDisplay = m_pFramebufferSRV;
            }
            m_pViewportDisplaySRV = pDisplay;
        }

        // Finally, prepare swap-chain back buffer (host UI etc.)
        pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
        pDSV = m_pSwapChain->GetDepthBufferDSV();
        float4 ClearColorSwap = {0.1f, 0.1f, 0.1f, 1.0f};
        if (m_ConvertPSOutputToGamma)
            ClearColorSwap = LinearToSRGB(ClearColorSwap);
        m_pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearRenderTarget(pRTV, ClearColorSwap.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    // --- GLTF Support ---------------------------------------------------------------
    void C6GERender::EnsureGLTFRenderer()
    {
        if (m_GLTFRenderer)
            return;

        GLTF_PBR_Renderer::CreateInfo RendererCI;
        RendererCI.NumRenderTargets = 1;
        const auto& SCDesc = m_pSwapChain->GetDesc();
        RendererCI.RTVFormats[0]    = SCDesc.ColorBufferFormat;
        RendererCI.DSVFormat        = SCDesc.DepthBufferFormat;
        RendererCI.PackMatrixRowMajor = true;
        // Most GLTF color textures are authored in sRGB but are sampled via linear SRVs by default.
        // Ask the PBR renderer to convert sRGB->linear in shader to ensure correct color.
        RendererCI.TexColorConversionMode = GLTF_PBR_Renderer::CreateInfo::TEX_COLOR_CONVERSION_MODE_SRGB_TO_LINEAR;

        // Create renderer (no state cache)
        m_GLTFRenderer = std::make_unique<GLTF_PBR_Renderer>(m_pDevice, nullptr, m_pImmediateContext, RendererCI);
        // Create frame attribs buffer
        if (m_GLTFRenderer)
        {
            CreateUniformBuffer(m_pDevice, m_GLTFRenderer->GetPRBFrameAttribsSize(), "PBR frame attribs buffer", &m_GLTFFrameAttribsCB);
            if (m_GLTFFrameAttribsCB)
            {
                StateTransitionDesc Barriers[] = {
                    {m_GLTFFrameAttribsCB, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE},
                };
                m_pImmediateContext->TransitionResourceStates(_countof(Barriers), Barriers);
            }
        }
    }

    // Resolve a model asset id: supports raw .gltf/.glb or .c6m wrappers generated by the Project system
    std::string C6GERender::ResolveModelAsset(const std::string& Id)
    {
        // Check cache
        auto itc = m_ModelResolveCache.find(Id);
        if (itc != m_ModelResolveCache.end())
            return itc->second;

        // If not a .c6m file, return as-is
        auto has_ext = [](const std::string& s, const char* ext){
            if (s.size() < strlen(ext)) return false;
            std::string e = s.substr(s.size()-strlen(ext));
            for (auto& c : e) c = (char)tolower(c);
            std::string t(ext);
            for (auto& c : t) c = (char)tolower(c);
            return e == t;
        };
        if (!has_ext(Id, ".c6m")) {
            m_ModelResolveCache[Id] = Id;
            return Id;
        }

        // Try to read small JSON and extract "source"
        std::string resolved = Id;
        try {
            std::ifstream f(Id, std::ios::in | std::ios::binary);
            if (f) {
                std::stringstream ss; ss << f.rdbuf();
                std::string s = ss.str();
                // very naive scan for source: "source" : "..."
                size_t p = s.find("\"source\"");
                if (p != std::string::npos) {
                    p = s.find('"', p+1);
                    p = s.find('"', p+1);
                    size_t q1 = s.find('"', p+1);
                    size_t q2 = q1 != std::string::npos ? s.find('"', q1+1) : std::string::npos;
                    if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1)
                        resolved = s.substr(q1+1, q2-(q1+1));
                }
            }
        } catch (...) {
            // ignore
        }
        // If path is relative and we have a project, make it absolute from project root
        if (m_Project && !resolved.empty()) {
            namespace fs = std::filesystem;
            fs::path p = resolved;
            if (p.is_relative()) {
                auto root = m_Project->GetConfig().rootDir;
                p = root / p;
                resolved = p.u8string();
            }
        }
        m_ModelResolveCache[Id] = resolved;
        return resolved;
    }

    std::string C6GERender::LoadGLTFAsset(const std::string& Path)
    {
        EnsureGLTFRenderer();
        if (!m_GLTFRenderer)
            return {};

        // Normalize path for use as key
        std::string Key = Path;
        // Trim quotes if any
        if (!Key.empty() && (Key.front() == '"' || Key.front() == '\''))
            Key.erase(0, 1);
        if (!Key.empty() && (Key.back() == '"' || Key.back() == '\''))
            Key.pop_back();

        // If it's a .c6m wrapper, resolve to real source path
        Key = ResolveModelAsset(Key);

        // If already loaded, return
        if (m_GltfAssets.find(Key) != m_GltfAssets.end())
            return Key;

        GLTF::ModelCreateInfo CI;
        CI.FileName             = Key.c_str();
        CI.ComputeBoundingBoxes = true;

        GltfAsset Asset;
        Asset.SceneIndex = 0;
        Asset.Model = std::make_unique<GLTF::Model>(m_pDevice, m_pImmediateContext, CI);
        if (!Asset.Model)
            return {};
        // Create SRBs per material bound to our frame attribs
        Asset.Bindings = m_GLTFRenderer->CreateResourceBindings(*Asset.Model, m_GLTFFrameAttribsCB);
    // Default transforms for scene 0
    Asset.Model->ComputeTransforms(Asset.SceneIndex, Asset.Transforms);
    // Compute scene-space bounding box (model space relative to asset root)
    Asset.Bounds = Asset.Model->ComputeBoundingBox(Asset.SceneIndex, Asset.Transforms);

        m_GltfAssets.emplace(Key, std::move(Asset));
        return Key;
    }

    void C6GERender::EnsureGLTFBLAS(const std::string& AssetId)
    {
        auto it = m_GltfAssets.find(AssetId);
        if (it == m_GltfAssets.end())
            return;
        auto& Asset = it->second;
        if (Asset.BLAS)
            return; // already built

        // Prepare RT-capable copies of VB0 (positions interleaved) and index buffer
        IBuffer* pVB0 = Asset.Model->GetVertexBuffer(0, m_pDevice, m_pImmediateContext);
        IBuffer* pIB  = Asset.Model->GetIndexBuffer(m_pDevice, m_pImmediateContext);
        if (pVB0 == nullptr || pIB == nullptr)
            return;

        const auto& VBDesc = pVB0->GetDesc();
        const auto& IBDesc = pIB->GetDesc();

        // Create RT vertex buffer copy (default usage) and copy data
        {
            BufferDesc Desc;
            Desc.Name               = "GLTF RT Vertex Buffer0";
            Desc.Usage              = USAGE_DEFAULT;
            Desc.BindFlags          = BIND_VERTEX_BUFFER | BIND_RAY_TRACING;
            Desc.Size               = VBDesc.Size;
            Desc.ElementByteStride  = VBDesc.ElementByteStride != 0 ? VBDesc.ElementByteStride : 32u;
            m_pDevice->CreateBuffer(Desc, nullptr, &Asset.RTVertexBuffer0);
            if (Asset.RTVertexBuffer0)
            {
                m_pImmediateContext->CopyBuffer(pVB0, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                                Asset.RTVertexBuffer0, 0, Desc.Size,
                                                RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            }
        }

        // Create RT index buffer copy (default usage) and copy data
        {
            BufferDesc Desc;
            Desc.Name               = "GLTF RT Index Buffer";
            Desc.Usage              = USAGE_DEFAULT;
            Desc.BindFlags          = BIND_INDEX_BUFFER | BIND_RAY_TRACING;
            Desc.Size               = IBDesc.Size;
            Desc.ElementByteStride  = IBDesc.ElementByteStride != 0 ? IBDesc.ElementByteStride : 4u;
            m_pDevice->CreateBuffer(Desc, nullptr, &Asset.RTIndexBuffer);
            if (Asset.RTIndexBuffer)
            {
                m_pImmediateContext->CopyBuffer(pIB, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                                Asset.RTIndexBuffer, 0, Desc.Size,
                                                RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            }
        }

        if (!Asset.RTVertexBuffer0 || !Asset.RTIndexBuffer)
            return;

        // Compute counts for BLAS triangle desc
        const Uint32 VertexStride = Asset.RTVertexBuffer0->GetDesc().ElementByteStride != 0 ? Asset.RTVertexBuffer0->GetDesc().ElementByteStride : 32u;
        const Uint64 VBSize       = Asset.RTVertexBuffer0->GetDesc().Size;
        const Uint32 NumVertices  = VertexStride != 0 ? static_cast<Uint32>(VBSize / VertexStride) : 0u;
        const Uint32 IndexStride  = Asset.RTIndexBuffer->GetDesc().ElementByteStride != 0 ? Asset.RTIndexBuffer->GetDesc().ElementByteStride : 4u;
        const Uint64 IBSize       = Asset.RTIndexBuffer->GetDesc().Size;
        const Uint32 NumIndices   = IndexStride != 0 ? static_cast<Uint32>(IBSize / IndexStride) : 0u;
        if (NumVertices == 0 || NumIndices < 3)
            return;

        BLASTriangleDesc Triangles{};
        Triangles.GeometryName         = "GLTFModel";
        Triangles.MaxVertexCount       = NumVertices;
        Triangles.VertexValueType      = VT_FLOAT32;
        Triangles.VertexComponentCount = 3; // position only
        Triangles.MaxPrimitiveCount    = NumIndices / 3;
        Triangles.IndexType            = (IndexStride == 2 ? VT_UINT16 : VT_UINT32);

        BottomLevelASDesc ASDesc;
        ASDesc.Name          = "GLTF BLAS";
        ASDesc.Flags         = RAYTRACING_BUILD_AS_PREFER_FAST_TRACE;
        ASDesc.pTriangles    = &Triangles;
        ASDesc.TriangleCount = 1;
        m_pDevice->CreateBLAS(ASDesc, &Asset.BLAS);
        if (!Asset.BLAS)
            return;

        // Build BLAS
        BufferDesc ScratchDesc;
        ScratchDesc.Name      = "GLTF BLAS Scratch";
        ScratchDesc.Usage     = USAGE_DEFAULT;
        ScratchDesc.BindFlags = BIND_RAY_TRACING;
        ScratchDesc.Size      = Asset.BLAS->GetScratchBufferSizes().Build;
        RefCntAutoPtr<IBuffer> pScratch;
        m_pDevice->CreateBuffer(ScratchDesc, nullptr, &pScratch);

        BLASBuildTriangleData TriData{};
        TriData.GeometryName         = Triangles.GeometryName;
        TriData.pVertexBuffer        = Asset.RTVertexBuffer0;
        TriData.VertexStride         = VertexStride;
        TriData.VertexOffset         = 0;
        TriData.VertexCount          = NumVertices;
        TriData.VertexValueType      = Triangles.VertexValueType;
        TriData.VertexComponentCount = Triangles.VertexComponentCount;
        TriData.pIndexBuffer         = Asset.RTIndexBuffer;
        TriData.IndexOffset          = 0;
        TriData.PrimitiveCount       = NumIndices / 3;
        TriData.IndexType            = Triangles.IndexType;
        TriData.Flags                = RAYTRACING_GEOMETRY_FLAG_OPAQUE;

        BuildBLASAttribs Build{};
        Build.pBLAS                       = Asset.BLAS;
        Build.pTriangleData               = &TriData;
        Build.TriangleDataCount           = 1;
        Build.pScratchBuffer              = pScratch;
        Build.BLASTransitionMode          = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        Build.GeometryTransitionMode      = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        Build.ScratchBufferTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        m_pImmediateContext->BuildBLAS(Build);
    }

    void C6GERender::RenderGLTFWithWorld(const std::string& AssetId, const float4x4& World, const float4x4& CameraViewProj,
                                         RESOURCE_STATE_TRANSITION_MODE TransitionMode)
    {
        auto it = m_GltfAssets.find(AssetId);
        if (it == m_GltfAssets.end())
        {
            // Try to load lazily if not present
            auto LoadedId = LoadGLTFAsset(AssetId);
            it = m_GltfAssets.find(LoadedId);
            if (it == m_GltfAssets.end())
                return;
        }
        EnsureGLTFRenderer();
        if (!m_GLTFRenderer || !m_GLTFFrameAttribsCB)
            return;

        // Update frame camera attribs minimally
        {
            MapHelper<HLSL::PBRFrameAttribs> Frame{m_pImmediateContext, m_GLTFFrameAttribsCB, MAP_WRITE, MAP_FLAG_DISCARD};
            // Zero initialize
            memset(static_cast<void*>(Frame), 0, m_GLTFRenderer->GetPRBFrameAttribsSize());
            // Fill camera basics
            auto& Cam = Frame->Camera;
            Cam.f4ViewportSize = float4{static_cast<float>(m_FramebufferWidth), static_cast<float>(m_FramebufferHeight),
                                        m_FramebufferWidth > 0 ? 1.0f / static_cast<float>(m_FramebufferWidth) : 0.0f,
                                        m_FramebufferHeight > 0 ? 1.0f / static_cast<float>(m_FramebufferHeight) : 0.0f};
            Cam.fNearPlaneZ    = 0.1f;
            Cam.fFarPlaneZ     = 100.0f;
            Cam.fNearPlaneDepth = 0.0f;
            Cam.fFarPlaneDepth  = 1.0f;
            Cam.fHandness      = 1.0f; // right-handed
            Cam.mViewProj      = CameraViewProj;
            Cam.mView          = float4x4::Identity(); // not strictly needed by PBR if ViewProj is set
            Cam.mProj          = float4x4::Identity();
            Cam.mViewInv       = Cam.mView.Inverse();
            Cam.mProjInv       = Cam.mProj.Inverse();
            Cam.mViewProjInv   = Cam.mViewProj.Inverse();

            // Renderer parameters defaults
            auto& RendererParams = Frame->Renderer;
            m_GLTFRenderer->SetInternalShaderParameters(RendererParams);
            RendererParams.OcclusionStrength = 1.0f;
            RendererParams.EmissionScale     = 1.0f;
            RendererParams.AverageLogLum     = 0.3f;
            RendererParams.MiddleGray        = 0.18f;
            RendererParams.WhitePoint        = 3.0f;
            // Only provide a directional light to PBR renderer if ECS has one; otherwise keep dark
            {
                HLSL::PBRLightAttribs* Lights = reinterpret_cast<HLSL::PBRLightAttribs*>(Frame + 1);
                RendererParams.LightCount = 0;
                if (m_HasDirectionalLight)
                {
                    float3 Dir = normalize(m_LightDirection);
                    GLTF::Light DirLight;
                    DirLight.Type      = GLTF::Light::TYPE::DIRECTIONAL;
                    DirLight.Color     = float3{1, 1, 1};
                    DirLight.Intensity = std::max(0.0f, m_DirLightIntensity);
                    GLTF_PBR_Renderer::WritePBRLightShaderAttribs({&DirLight, nullptr, &Dir, /*DistanceScale*/ 1.0f}, Lights);
                    RendererParams.LightCount = 1;
                }
            }
        }

        m_GLTFRenderer->Begin(m_pImmediateContext);

        // Ensure IBL textures produced by PBR renderer are in SHADER_RESOURCE state before use
        {
            std::vector<StateTransitionDesc> Barriers;
            if (auto* IrrSRV = m_GLTFRenderer->GetIrradianceCubeSRV())
            {
                if (auto* Tex = IrrSRV->GetTexture())
                    Barriers.emplace_back(StateTransitionDesc{Tex, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_SHADER_RESOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE});
            }
            if (auto* PrefSRV = m_GLTFRenderer->GetPrefilteredEnvMapSRV())
            {
                if (auto* Tex = PrefSRV->GetTexture())
                    Barriers.emplace_back(StateTransitionDesc{Tex, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_SHADER_RESOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE});
            }
            if (!Barriers.empty())
                m_pImmediateContext->TransitionResourceStates(static_cast<Uint32>(Barriers.size()), Barriers.data());
        }

        auto& Asset = it->second;
        // Recompute transforms for this world root
        Asset.Model->ComputeTransforms(Asset.SceneIndex, Asset.Transforms, World);

        GLTF_PBR_Renderer::RenderInfo Info;
        Info.SceneIndex    = Asset.SceneIndex;
        Info.ModelTransform = float4x4::Identity(); // World already applied as root
        // Enable full PBR texturing and lighting. Renderer will automatically ignore
        // features that aren't present in a given material/mesh.
        Info.Flags = GLTF_PBR_Renderer::PSO_FLAG_DEFAULT |
                     GLTF_PBR_Renderer::PSO_FLAG_ALL_TEXTURES |
                     GLTF_PBR_Renderer::PSO_FLAG_USE_TEXCOORD0 |
                     GLTF_PBR_Renderer::PSO_FLAG_USE_TEXCOORD1 |
                     GLTF_PBR_Renderer::PSO_FLAG_USE_VERTEX_NORMALS |
                     GLTF_PBR_Renderer::PSO_FLAG_USE_VERTEX_TANGENTS |
                     GLTF_PBR_Renderer::PSO_FLAG_ENABLE_TEXCOORD_TRANSFORM |
                     GLTF_PBR_Renderer::PSO_FLAG_USE_LIGHTS;

    m_GLTFRenderer->Render(m_pImmediateContext, *Asset.Model, Asset.Transforms, nullptr, Info, &Asset.Bindings);
    }

    void C6GERender::RenderGLTFShadowWithWorld(const std::string& AssetId, const float4x4& World, const float4x4& LightViewProj,
                                               RESOURCE_STATE_TRANSITION_MODE TransitionMode)
    {
        auto it = m_GltfAssets.find(AssetId);
        if (it == m_GltfAssets.end())
        {
            auto LoadedId = LoadGLTFAsset(AssetId);
            it = m_GltfAssets.find(LoadedId);
            if (it == m_GltfAssets.end())
                return;
        }
        CreateGLTFShadowPSO();
        CreateGLTFShadowSkinnedPSO();
        if (!m_pGLTFShadowPSO)
            return;

        auto& Asset = it->second;
        // Compute transforms with root World
        Asset.Model->ComputeTransforms(Asset.SceneIndex, Asset.Transforms, World);

        // Bind all GLTF VBs like the PBR renderer does to ensure strides/slots match
        const Uint32 NumVBs = static_cast<Uint32>(Asset.Model->GetVertexBufferCount());
        std::vector<IBuffer*> VBs(NumVBs);
        for (Uint32 i = 0; i < NumVBs; ++i)
            VBs[i] = Asset.Model->GetVertexBuffer(i, m_pDevice, m_pImmediateContext);
        if (VBs.empty() || VBs[0] == nullptr)
            return;
        m_pImmediateContext->SetVertexBuffers(0, NumVBs, VBs.data(), nullptr, TransitionMode, SET_VERTEX_BUFFERS_FLAG_RESET);

        IBuffer* pIB = Asset.Model->GetIndexBuffer(m_pDevice, m_pImmediateContext);
        if (pIB)
            m_pImmediateContext->SetIndexBuffer(pIB, 0, TransitionMode);
        else
            return; // require indices for now

    // We'll pick PSO per node (skinned vs static) below

    const Uint32 BaseVertex = Asset.Model->GetBaseVertex();
    const Uint32 FirstIndexBase = Asset.Model->GetFirstIndexLocation();

        // Iterate scene nodes with meshes
        const auto& Scene = Asset.Model->Scenes[Asset.SceneIndex];
        for (const auto* pNode : Scene.LinearNodes)
        {
            if (pNode == nullptr || pNode->pMesh == nullptr)
                continue;

            const float4x4 NodeGlobal = Asset.Transforms.NodeGlobalMatrices[pNode->Index];
            const float4x4 WVP = NodeGlobal * LightViewProj;

            // Update constants (WorldViewProj)
            struct ShadowVSConstants { float4x4 g_WorldViewProj; };
            { MapHelper<ShadowVSConstants> C(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD); C->g_WorldViewProj = WVP; }

            // Choose skinned or static shadow PSO
            const bool IsSkinned = (pNode->SkinTransformsIndex >= 0);
            if (IsSkinned && m_pGLTFShadowSkinnedPSO && m_GLTFShadowSkinnedSRB && m_GLTFShadowSkinCB)
            {
                // Upload joint matrices for this skin
                const int SkinIdx = pNode->SkinTransformsIndex;
                const auto& Skin = Asset.Transforms.Skins[static_cast<size_t>(SkinIdx)];
                struct ShadowSkinCB
                {
                    float4x4 JointMatrices[256];
                    int      JointCount;
                    float3   _pad0;
                };
                {
                    MapHelper<ShadowSkinCB> M(m_pImmediateContext, m_GLTFShadowSkinCB, MAP_WRITE, MAP_FLAG_DISCARD);
                    const size_t Count = std::min<size_t>(Skin.JointMatrices.size(), 256);
                    // Zero out to be safe
                    for (size_t i = 0; i < 256; ++i)
                        M->JointMatrices[i] = float4x4::Identity();
                    for (size_t i = 0; i < Count; ++i)
                        M->JointMatrices[i] = Skin.JointMatrices[i];
                    M->JointCount = static_cast<int>(Count);
                    M->_pad0 = float3{0,0,0};
                }
                // Bind PSO and SRB for skinned shadow
                m_pImmediateContext->SetPipelineState(m_pGLTFShadowSkinnedPSO);
                m_pImmediateContext->CommitShaderResources(m_GLTFShadowSkinnedSRB, TransitionMode);
            }
            else
            {
                m_pImmediateContext->SetPipelineState(m_pGLTFShadowPSO);
                if (m_GLTFShadowSRB)
                    m_pImmediateContext->CommitShaderResources(m_GLTFShadowSRB, TransitionMode);
            }

            const auto& Mesh = *pNode->pMesh;
            for (const auto& Prim : Mesh.Primitives)
            {
                if (Prim.IndexCount == 0)
                    continue;
                DrawIndexedAttribs Draw{};
                Draw.IndexType = VT_UINT32; // GLTF resource manager stores indices in 32-bit buffer
                Draw.NumIndices = Prim.IndexCount;
                Draw.FirstIndexLocation = FirstIndexBase + Prim.FirstIndex;
                Draw.BaseVertex = BaseVertex;
                Draw.Flags = DRAW_FLAG_VERIFY_ALL;
                m_pImmediateContext->DrawIndexed(Draw);
            }
        }
    }

} // namespace Diligent