/*
 *  Copyright 2019-2025 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence),
 *  contract, or otherwise, unless required by applicable law (such as deliberate
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental,
 *  or consequential damages of any character arising as a result of this License or
 *  out of the use or inability to use the software (including but not limited to damages
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
 *  all other commercial damages or losses), even if such Contributor has been advised
 *  of the possibility of such damages.
 */

#pragma once

#include <atomic>
#include <vector>
#include <thread>
#include <mutex>
#include <unordered_map>
#include "SampleBase.hpp"
#include "FirstPersonCamera.hpp"
#include "DiligentTools/AssetLoader/interface/DXSDKMeshLoader.hpp"
#include "AdvancedMath.hpp"
#include "BasicMath.hpp"
#include "ThreadSignal.hpp"
#include "DiligentTools/ThirdParty/imgui/imgui.h"
#include <entt/entt.hpp>

// Forward declarations for DiligentFX PostFX
namespace Diligent
{
class PostFXContext;
class TemporalAntiAliasing;
}

namespace Diligent
{
    // FWD declarations to avoid including headers here
    namespace ECS { class World; }
    namespace C6GE { namespace Systems { class RenderSystem; } }

    class C6GERender final : public SampleBase
    {
        // Play/pause icon textures
        RefCntAutoPtr<ITextureView> m_PlayIconSRV;
        RefCntAutoPtr<ITextureView> m_PauseIconSRV;

    public:
        // Play/pause state for editor runtime
        enum class PlayState
        {
            Paused,
            Playing
        };
        static PlayState playState;

        static void TogglePlayState();
        static bool IsPlaying() { return playState == PlayState::Playing; }
        static bool IsPaused() { return playState == PlayState::Paused; }
        virtual void Initialize(const SampleInitInfo &InitInfo) override final;

        void ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs &Attribs) override final;

        virtual ~C6GERender();

    void UpdateUI() override;

        void UpdateViewportUI();

        void DXSDKMESH_VERTEX_ELEMENTtoInputLayoutDesc(const DXSDKMESH_VERTEX_ELEMENT *VertexElement,
                                                       Uint32 Stride,
                                                       InputLayoutDesc &Layout,
                                                       std::vector<LayoutElement> &Elements);

        void CreateFramebuffer();

        void CreateMSAARenderTarget();

        void ResizeFramebuffer(Uint32 Width, Uint32 Height);

        void WindowResize(Uint32 Width, Uint32 Height) override final;

        virtual void Render() override final;
        virtual void Update(double CurrTime, double ElapsedTime, bool DoUpdateUI) override final;

        virtual const Char *GetSampleName() const override final { return "C6GERender"; }

        // Get framebuffer texture for ImGui viewport
        ITextureView *GetFramebufferSRV() const { return m_pFramebufferSRV; }
        Uint32 GetFramebufferWidth() const { return m_FramebufferWidth; }
        Uint32 GetFramebufferHeight() const { return m_FramebufferHeight; }

        static bool IsRuntime;

    private:
    // Small helper to draw a cube with an explicit world transform (for ECS)
    public:
        void RenderCubeWithWorld(const float4x4& World, const float4x4& CameraViewProj, bool IsShadowPass,
                                 RESOURCE_STATE_TRANSITION_MODE TransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        // ECS accessors for external object creation
        class ECS::World* GetWorld() { return m_World.get(); }
        void EnsureWorld();
    private:
    void CreateCubePSO();
    void CreatePlanePSO();
    // Shadow map visualization creation removed
    void CreateShadowMap();
    void RenderShadowMap();
    void RenderCube(const float4x4& CameraViewProj, bool IsShadowPass, RESOURCE_STATE_TRANSITION_MODE TransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    void RenderPlane(RESOURCE_STATE_TRANSITION_MODE TransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    // Shadow map visualization removed
    
    // Render pass methods
    void CreateMainRenderPass();
    RefCntAutoPtr<IFramebuffer> CreateMainFramebuffer();
    IFramebuffer* GetCurrentMainFramebuffer();

    RefCntAutoPtr<IPipelineState>         m_pCubePSO;
    RefCntAutoPtr<IPipelineState>         m_pCubeShadowPSO;
    RefCntAutoPtr<IPipelineState>         m_pPlanePSO;
    RefCntAutoPtr<IPipelineState>         m_pPlaneNoShadowPSO;
    // Shadow map visualization PSO removed
    RefCntAutoPtr<IBuffer>                m_CubeVertexBuffer;
    RefCntAutoPtr<IBuffer>                m_CubeIndexBuffer;
    RefCntAutoPtr<IBuffer>                m_VSConstants;
    float                                  m_LineWidth = 1.0f;
    RefCntAutoPtr<ITextureView>           m_TextureSRV;
    RefCntAutoPtr<IShaderResourceBinding> m_CubeSRB;
    RefCntAutoPtr<IShaderResourceBinding> m_CubeShadowSRB;
    RefCntAutoPtr<IShaderResourceBinding> m_PlaneSRB;
    RefCntAutoPtr<IShaderResourceBinding> m_PlaneNoShadowSRB;
    // Visualization SRB removed
    RefCntAutoPtr<ITextureView>           m_ShadowMapDSV;
    RefCntAutoPtr<ITextureView>           m_ShadowMapSRV;
    FirstPersonCamera m_Camera;

    // Render pass support
    RefCntAutoPtr<IRenderPass>  m_pMainRenderPass;
    std::unordered_map<ITextureView*, RefCntAutoPtr<IFramebuffer>> m_FramebufferCache;
    bool m_UseRenderPasses = false; // Enable render passes when supported

    // Ray tracing (hybrid) support flags
    bool m_RayTracingSupported = false; // Device capability check
    bool m_EnableRayTracing    = false; // User toggle (default off)
    bool m_RayTracingInitialized = false; // Internal init state
    bool m_PendingRTRestart   = false;   // Defer RT destroy/reinit to the next frame after resize

    // Ray tracing UI settings
    bool  m_SoftShadowsEnabled   = true;   // Toggle hard vs soft shadows for RT path
    float m_SoftShadowAngularRad  = 0.06f; // ~3.4 degrees
    int   m_SoftShadowSamples     = 8;     // Number of shadow rays per pixel
    bool  m_EnableRTReflections   = true;  // Toggle additive reflection composite

    // Post-processing
    bool  m_EnablePostProcessing  = false; // Master toggle for post-processing chain
    bool  m_PostGammaCorrection   = true;  // If true, run gamma-correction pass (simple example)
    void  CreatePostFXTargets();
    void  DestroyPostFXTargets();
    void  CreatePostFXPSOs();
    RefCntAutoPtr<ITexture>           m_pPostTexture;    // Post-processed color buffer (same size as framebuffer)
    RefCntAutoPtr<ITextureView>       m_pPostRTV;
    RefCntAutoPtr<ITextureView>       m_pPostSRV;
    RefCntAutoPtr<IPipelineState>     m_pPostGammaPSO;   // Simple gamma-correction PSO (fullscreen)
    RefCntAutoPtr<IShaderResourceBinding> m_PostGammaSRB;

    // Temporal Anti-Aliasing (TAA)
    bool m_EnableTAA = false; // UI toggle
    std::unique_ptr<PostFXContext>        m_PostFXContext; // Frame graph helper used by TAA
    std::unique_ptr<TemporalAntiAliasing> m_TAA;           // TAA module
    // TAA settings struct comes from HLSL includes in Render.cpp; keep opaque here
    struct TAASettingsOpaque { alignas(16) unsigned char _pad[64]; };
    TAASettingsOpaque m_TAASettingsStorage{}; // Storage; real type defined in Render.cpp via alias
    uint64_t m_TemporalFrameIndex = 0;        // For jitter sequencing and history

    // Ray tracing (hybrid) stubbed API
    void InitializeRayTracing();
    void DestroyRayTracing();
    void RenderRayTracingPath();
    void CreateRayTracingOutputTexture(Uint32 Width, Uint32 Height);
    void CreateRTDebugPSOs();
    void CreateRayTracingAS();
    void DestroyRayTracingAS();
    void UpdateTLAS();

    // Ray tracing resources (incremental)
    RefCntAutoPtr<ITexture>     m_pRTOutputTex;
    RefCntAutoPtr<ITextureView> m_pRTOutputUAV;
    RefCntAutoPtr<ITextureView> m_pRTOutputSRV;

    // Acceleration structures (placeholders)
    RefCntAutoPtr<IBottomLevelAS> m_pBLAS_Cube;
    RefCntAutoPtr<IBottomLevelAS> m_pBLAS_Plane;
    RefCntAutoPtr<ITopLevelAS>    m_pTLAS;
    RefCntAutoPtr<IBuffer>        m_pTLASInstances;
    RefCntAutoPtr<IBuffer>        m_pTLASScratch;

    // Ray tracing geometry copies (created with BIND_RAY_TRACING)
    RefCntAutoPtr<IBuffer>        m_CubeRTVertexBuffer;
    RefCntAutoPtr<IBuffer>        m_CubeRTIndexBuffer;
    RefCntAutoPtr<IBuffer>        m_PlaneRTVertexBuffer;
    RefCntAutoPtr<IBuffer>        m_PlaneRTIndexBuffer;

    // RT composite overlay PSO
    RefCntAutoPtr<IPipelineState>         m_pRTCompositePSO;      // multiplicative (shadows)
    RefCntAutoPtr<IShaderResourceBinding> m_RTCompositeSRB;
    RefCntAutoPtr<IPipelineState>         m_pRTAddCompositePSO;   // additive (reflections)
    RefCntAutoPtr<IShaderResourceBinding> m_RTAddCompositeSRB;
    // Ray query (shadows) compute PSO
    RefCntAutoPtr<IPipelineState>         m_pRTShadowPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_RTShadowSRB;
    RefCntAutoPtr<IBuffer>                m_RTConstants;
    // TLAS capacity hint
    uint32_t                              m_MaxTLASInstances = 1024;

    float4x4       m_CubeWorldMatrix;
    float4x4       m_SecondCubeWorldMatrix;
    float4x4       m_CameraViewProjMatrix;
    float4x4       m_WorldToShadowMapUVDepthMatr;
    float3         m_LightDirection  = normalize(float3(-0.49f, -0.60f, 0.64f));
    Uint32         m_ShadowMapSize   = 2048;
    TEXTURE_FORMAT m_ShadowMapFormat = TEX_FORMAT_D16_UNORM;
    // Shadow visualization removed. Use RenderShadows to enable/disable shadowing.

        // Framebuffer for off-screen rendering
        RefCntAutoPtr<ITexture> m_pFramebufferTexture;
        RefCntAutoPtr<ITextureView> m_pFramebufferRTV;
        RefCntAutoPtr<ITextureView> m_pFramebufferSRV;
        RefCntAutoPtr<ITexture> m_pFramebufferDepth;
        RefCntAutoPtr<ITextureView> m_pFramebufferDSV;
        Uint32 m_FramebufferWidth = 800;
        Uint32 m_FramebufferHeight = 600;
        ImTextureID m_ViewportTextureID = 0;
    RefCntAutoPtr<ITextureView> m_pViewportDisplaySRV; // What the viewport draws this frame

        // MSAA settings
        Uint8  m_SampleCount = 1;
        std::vector<Uint8> m_SupportedSampleCounts;
        RefCntAutoPtr<ITextureView> m_pMSColorRTV;
        RefCntAutoPtr<ITextureView> m_pMSDepthDSV;

        // ECS world and render system
        std::unique_ptr<ECS::World> m_World;
        std::unique_ptr<C6GE::Systems::RenderSystem> m_RenderSystem;
        // Editor selection
        entt::entity m_SelectedEntity { entt::null };
    };

} // namespace Diligent