#include <atomic>

namespace Diligent
{
    bool RenderSettingsOpen = false;
}
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

#include "Render.h"
#include "MapHelper.hpp"
#include "FileSystem.hpp"
#include "ShaderMacroHelper.hpp"
#include "CommonlyUsedStates.h"
#include "StringTools.hpp"
#include "GraphicsUtilities.h"
#include "GraphicsAccessories.hpp"
#include "AdvancedMath.hpp"
#include "DiligentTools/ThirdParty/imgui/imgui.h"
#include "DiligentTools/ThirdParty/imgui/imgui_internal.h"
#include "ImGuizmo.h"
#include "DiligentTools/Imgui/interface/ImGuiImplDiligent.hpp"

#include "CallbackWrapper.hpp"
#include "Utilities/interface/DiligentFXShaderSourceStreamFactory.hpp"
#include "ShaderSourceFactoryUtils.hpp"
#include "DiligentSamples/Tutorials/Common/src/TexturedCube.hpp"
#include "BasicMath.hpp"
#include "ColorConversion.h"
#include "TextureUtilities.h"
#include <random>
#include <string>
#include <algorithm>
#include <cstring>
// STL containers
#include <array>
// File IO for model resolution
#include <fstream>
#include <sstream>
// STL containers
#include <vector>

// DiligentFX PostFX headers
#include "PostFXContext.hpp"
#include "TemporalAntiAliasing.hpp"

// ECS
#include "Runtime/ECS/World.h"
#include "Runtime/ECS/Components.h"
#include "Render/Systems/RenderSystem.h"
// File dialog helper
#include "Platform/FileDialog.h"
// PBR frame structures for C++ access: include HLSL shader structs into C++ under Diligent::HLSL namespace
namespace Diligent { namespace HLSL
{
#include "DiligentFX/Shaders/Common/public/BasicStructures.fxh"
#include "DiligentFX/Shaders/PBR/public/PBR_Structures.fxh"
#include "DiligentFX/Shaders/PBR/private/RenderPBR_Structures.fxh"
} } // namespace Diligent::HLSL

// Helper alias to map opaque storage to the settings type declared by the TAA module
using TAASettings = Diligent::HLSL::TemporalAntiAliasingAttribs;

namespace Diligent
{
    bool RenderShadows = false;
    bool Diligent::C6GERender::IsRuntime = false;
    Diligent::C6GERender::PlayState Diligent::C6GERender::playState = Diligent::C6GERender::PlayState::Paused;

    void Diligent::C6GERender::TogglePlayState()
    {
        if (playState == PlayState::Paused)
            playState = PlayState::Playing;
        else
            playState = PlayState::Paused;
    }
    extern bool RenderSettingsOpen;

    SampleBase *CreateSample()
    {
        return new C6GERender();
    }

    namespace
    {

        struct Constants
        {
            // Match HLSL cbuffer 'Constants' in cube.vsh:
            // cbuffer Constants
            // {
            //     float4x4 g_WorldViewProj;
            //     float4x4 g_NormalTranform;
            //     float4   g_LightDirection;
            // };
            float4x4 g_WorldViewProj;
            float4x4 g_NormalTranform;
            float4   g_LightDirection;
        };

        // Constants layout for the ray-query compute shader
        struct RTConstantsCPU
        {
            float4x4 InvViewProj;        // inverse of camera view-projection
            float4   ViewSize_Plane;     // (width, height, PlaneY, PlaneExtent)
            float4   LightDir_Shadow;    // (Lx, Ly, Lz, ShadowStrength)
            float4   ShadowSoftParams;   // (AngularRadiusRad, SampleCount, unused, unused)
        };

    } // namespace

    C6GERender::~C6GERender()
    {
        // Release SRBs first so their internal caches drop references before views/textures are torn down
        for (auto& srb : m_PostGammaSRBs)
            srb.Release();
        m_RTCompositeSRB.Release();
        m_RTAddCompositeSRB.Release();
        m_RTShadowSRB.Release();
        m_GLTFShadowSkinnedSRB.Release();
        m_GLTFShadowSRB.Release();
        m_PlaneNoShadowSRB.Release();
        m_PlaneSRB.Release();
        m_CubeShadowSRB.Release();
        m_CubeSRB.Release();

        // Clear SRB-related caches before releasing attachments
        m_PostGammaSRBCache.clear();
        m_FramebufferCache.clear();

        // Explicitly release GPU views/resources in a safe order to avoid dangling ImGui references
        auto release_view = [](const char* name, RefCntAutoPtr<ITextureView>& v) {
            if (v)
            {
                printf("[C6GE][~C6GERender] Releasing view %s = %p\n", name, (void*)v.RawPtr());
                v.Release();
            }
        };
        auto release_tex = [](const char* name, RefCntAutoPtr<ITexture>& t) {
            if (t)
            {
                printf("[C6GE][~C6GERender] Releasing texture %s = %p\n", name, (void*)t.RawPtr());
                t.Release();
            }
        };

        // Views that ImGui may reference directly
        release_view("m_pViewportDisplaySRV", m_pViewportDisplaySRV);
        release_view("m_PlayIconSRV", m_PlayIconSRV);
        release_view("m_PauseIconSRV", m_PauseIconSRV);

        // Post-processing targets
        release_view("m_pPostSRV", m_pPostSRV);
        release_view("m_pPostRTV", m_pPostRTV);

        // Ray tracing outputs
        release_view("m_pRTOutputSRV", m_pRTOutputSRV);
        release_view("m_pRTOutputUAV", m_pRTOutputUAV);

        // Framebuffer views
        release_view("m_pMSDepthDSV", m_pMSDepthDSV);
        release_view("m_pMSColorRTV", m_pMSColorRTV);
        release_view("m_pFramebufferDSV", m_pFramebufferDSV);
        release_view("m_pFramebufferRTV", m_pFramebufferRTV);
        release_view("m_pFramebufferSRV", m_pFramebufferSRV);

        // Shadow map views
        release_view("m_ShadowMapSRV", m_ShadowMapSRV);
        release_view("m_ShadowMapDSV", m_ShadowMapDSV);

        // Cube texture SRV
        release_view("m_TextureSRV", m_TextureSRV);

        // Release textures (parents) after views
        release_tex("m_pMSDepthTex", m_pMSDepthTex);
        release_tex("m_pMSColorTex", m_pMSColorTex);
        release_tex("m_pFramebufferDepth", m_pFramebufferDepth);
        release_tex("m_pFramebufferTexture", m_pFramebufferTexture);
        release_tex("m_ShadowMapTex", m_ShadowMapTex);
        release_tex("m_FallbackWhiteTex", m_FallbackWhiteTex);
        release_tex("m_CubeTexture", m_CubeTexture);
        release_tex("m_pRTOutputTex", m_pRTOutputTex);
        release_tex("m_pPostTexture", m_pPostTexture);
        release_tex("m_PlayIconTex", m_PlayIconTex);
        release_tex("m_PauseIconTex", m_PauseIconTex);
    }

    void C6GERender::ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs &Attribs)
    {
        SampleBase::ModifyEngineInitInfo(Attribs);

        Attribs.EngineCI.Features.DepthClamp = DEVICE_FEATURE_STATE_OPTIONAL;

#if D3D12_SUPPORTED
        if (Attribs.DeviceType == RENDER_DEVICE_TYPE_D3D12)
        {
            EngineD3D12CreateInfo &D3D12CI = static_cast<EngineD3D12CreateInfo &>(Attribs.EngineCI);
            D3D12CI.GPUDescriptorHeapSize[1] = 1024; // Sampler descriptors
            D3D12CI.GPUDescriptorHeapDynamicSize[1] = 1024;
        }
#endif
    }

    void C6GERender::Initialize(const SampleInitInfo &InitInfo)
    {
        SampleBase::Initialize(InitInfo);

        // Initialize MSAA settings
        const TextureFormatInfoExt& ColorFmtInfo = m_pDevice->GetTextureFormatInfoExt(m_pSwapChain->GetDesc().ColorBufferFormat);
        const TextureFormatInfoExt& DepthFmtInfo = m_pDevice->GetTextureFormatInfoExt(m_pSwapChain->GetDesc().DepthBufferFormat);
        Uint32 supportedSampleCounts = ColorFmtInfo.SampleCounts & DepthFmtInfo.SampleCounts;
        
        // Populate supported sample counts vector
        m_SupportedSampleCounts.clear();
        m_SupportedSampleCounts.push_back(1); // Always support 1x (no MSAA)
        if (supportedSampleCounts & SAMPLE_COUNT_2)
            m_SupportedSampleCounts.push_back(2);
        if (supportedSampleCounts & SAMPLE_COUNT_4)
            m_SupportedSampleCounts.push_back(4);
        if (supportedSampleCounts & SAMPLE_COUNT_8)
            m_SupportedSampleCounts.push_back(8);
        if (supportedSampleCounts & SAMPLE_COUNT_16)
            m_SupportedSampleCounts.push_back(16);
        
        // Set default sample count
        if (supportedSampleCounts & SAMPLE_COUNT_4)
            m_SampleCount = 4;
        else if (supportedSampleCounts & SAMPLE_COUNT_2)
            m_SampleCount = 2;
        else
        {
            LOG_WARNING_MESSAGE(ColorFmtInfo.Name, " + ", DepthFmtInfo.Name, " pair does not allow multisampling on this device");
            m_SampleCount = 1;
        }

        // Load play/pause icons (after m_pDevice is valid)
        TextureLoadInfo loadInfo;
        loadInfo.IsSRGB = true;
        try {
            CreateTextureFromFile("Editor/PlayIcon.png", loadInfo, m_pDevice, &m_PlayIconTex);
            if (m_PlayIconTex)
            {
                auto srv = m_PlayIconTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
                if (srv)
                    m_PlayIconSRV = srv;
                else
                    std::cerr << "[C6GE] Failed to get SRV for PlayIcon.png" << std::endl;
            }
            else
            {
                std::cerr << "[C6GE] Failed to load Editor/PlayIcon.png" << std::endl;
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "[C6GE] Exception loading PlayIcon.png: " << e.what() << std::endl;
        }

        try {
            CreateTextureFromFile("Editor/PauseIcon.png", loadInfo, m_pDevice, &m_PauseIconTex);
            if (m_PauseIconTex)
            {
                auto srv = m_PauseIconTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
                if (srv)
                    m_PauseIconSRV = srv;
                else
                    std::cerr << "[C6GE] Failed to get SRV for PauseIcon.png" << std::endl;
            }
            else
            {
                std::cerr << "[C6GE] Failed to load Editor/PauseIcon.png" << std::endl;
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "[C6GE] Exception loading PauseIcon.png: " << e.what() << std::endl;
        }

        // Initialize camera position to look at the cube
        m_Camera.SetPos(float3(0.f, 0.f, 5.f));
        m_Camera.SetRotation(0.f, 0.f);

        // Query ray tracing support (DXR/Vulkan-RT)
        {
            const auto& DevInfo = m_pDevice->GetDeviceInfo();
            // Ray tracing feature may be optional; mark supported only when enabled
            m_RayTracingSupported = (DevInfo.Features.RayTracing == DEVICE_FEATURE_STATE_ENABLED);
            if (!m_RayTracingSupported)
            {
                std::cout << "[C6GE] Ray tracing not supported by this device/driver or not enabled; hybrid rendering will be unavailable." << std::endl;
            }
            else
            {
                std::cout << "[C6GE] Ray tracing is supported by this device (feature enabled)." << std::endl;
            }
        }

        CreateCubePSO();

        // Create plane and shadow-related PSOs
        CreatePlanePSO();

        // Load textured cube with normals (required for lighting)
        m_CubeVertexBuffer = TexturedCube::CreateVertexBuffer(m_pDevice, GEOMETRY_PRIMITIVE_VERTEX_FLAG_ALL);
        m_CubeIndexBuffer = TexturedCube::CreateIndexBuffer(m_pDevice);
        
        // Safely load texture with error handling
        try {
            m_CubeTexture = TexturedCube::LoadTexture(m_pDevice, "C6GELogo.png");
            if (m_CubeTexture)
            {
                m_TextureSRV = m_CubeTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
                if (!m_TextureSRV)
                {
                    std::cerr << "[C6GE] Failed to get SRV for C6GELogo.png" << std::endl;
                }
            }
            else
            {
                std::cerr << "[C6GE] Failed to load texture: C6GELogo.png" << std::endl;
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "[C6GE] Exception loading texture: " << e.what() << std::endl;
        }
        
        // Only bind texture if both SRB and texture are valid
        if (m_CubeSRB && m_TextureSRV)
        {
            auto textureVar = m_CubeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture");
            if (textureVar)
                textureVar->Set(m_TextureSRV);
            else
                std::cerr << "[C6GE] Failed to find g_Texture variable in SRB" << std::endl;
        }
        else if (m_CubeSRB && !m_TextureSRV)
        {
            // Create a 1x1 white fallback texture to satisfy g_Texture binding
            try
            {
                TextureDesc Desc;
                Desc.Name      = "Fallback White Texture";
                Desc.Type      = RESOURCE_DIM_TEX_2D;
                Desc.Width     = 1;
                Desc.Height    = 1;
                Desc.MipLevels = 1;
                Desc.Format    = TEX_FORMAT_RGBA8_UNORM;
                Desc.BindFlags = BIND_SHADER_RESOURCE;
                const Uint8 White[4] = {255, 255, 255, 255};
                TextureSubResData SubRes;
                SubRes.pData   = White;
                SubRes.Stride  = 4;
                TextureData InitData;
                InitData.pSubResources = &SubRes;
                InitData.NumSubresources = 1;
                m_pDevice->CreateTexture(Desc, &InitData, &m_FallbackWhiteTex);
                if (m_FallbackWhiteTex)
                {
                    m_TextureSRV = m_FallbackWhiteTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
                    if (auto* textureVar = m_CubeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture"))
                        textureVar->Set(m_TextureSRV);
                }
            }
            catch (...)
            {
                // Silent fallback failure; draw will log missing resource
            }
        }

        // Ensure framebuffer is created after swap chain and device are ready.
        if (m_pSwapChain)
        {
            const auto &scDesc = m_pSwapChain->GetDesc();
            if (scDesc.Width > 0 && scDesc.Height > 0)
            {
                m_FramebufferWidth = scDesc.Width;
                m_FramebufferHeight = scDesc.Height;
            }
            else
            {
                // Fallback to default size
                m_FramebufferWidth = std::max(1u, m_FramebufferWidth);
                m_FramebufferHeight = std::max(1u, m_FramebufferHeight);
            }
        }
        else
        {
            std::cerr << "[C6GE] Warning: SwapChain is null during initialization" << std::endl;
            m_FramebufferWidth = std::max(1u, m_FramebufferWidth);
            m_FramebufferHeight = std::max(1u, m_FramebufferHeight);
        }

        try {
            CreateFramebuffer();
        }
        catch (const std::exception& e)
        {
            std::cerr << "[C6GE] Exception creating framebuffer: " << e.what() << std::endl;
            throw; // Re-throw as this is critical
        }

        // Create initial post-processing render target and PSOs
        try {
            CreatePostFXTargets();
            CreatePostFXPSOs();
        }
        catch (const std::exception& e)
        {
            std::cerr << "[C6GE] Exception creating post-processing pipeline: " << e.what() << std::endl;
            // Non-critical: allow app to run without post FX
        }

        // Initialize PostFXContext + TAA (optional)
        try
        {
            m_PostFXContext = std::make_unique<PostFXContext>(m_pDevice, PostFXContext::CreateInfo{true, true});
            m_TAA            = std::make_unique<TemporalAntiAliasing>(m_pDevice, TemporalAntiAliasing::CreateInfo{true});

            // Default TAA settings (from HLSL::TemporalAntiAliasingAttribs)
            auto& TAASettingsRef = *reinterpret_cast<TAASettings*>(&m_TAASettingsStorage);
            TAASettingsRef.TemporalStabilityFactor = 0.9375f; // tutorial default
            TAASettingsRef.ResetAccumulation       = FALSE;
            TAASettingsRef.SkipRejection           = FALSE;
            // m_EnableTAA remains false by default; enable via UI
        }
        catch (const std::exception& e)
        {
            std::cerr << "[C6GE] Exception creating TAA: " << e.what() << std::endl;
            m_PostFXContext.reset();
            m_TAA.reset();
        }

        try {
            CreateMSAARenderTarget();
        }
        catch (const std::exception& e)
        {
            std::cerr << "[C6GE] Exception creating MSAA render target: " << e.what() << std::endl;
            throw; // Re-throw as this is critical
        }

        // Create shadow map and bind it to plane SRB / shadow-visualization SRB
        try {
            CreateShadowMap();
        }
        catch (const std::exception& e)
        {
            std::cerr << "[C6GE] Exception creating shadow map: " << e.what() << std::endl;
            throw; // Re-throw as this is critical
        }

        // Create main render pass (for Vulkan/Metal optimization)
        try {
            CreateMainRenderPass();
        }
        catch (const std::exception& e)
        {
            std::cerr << "[C6GE] Exception creating main render pass: " << e.what() << std::endl;
            // Non-critical - continue without render passes
            m_UseRenderPasses = false;
        }

        // Transition all resources to their required states for rendering
        // This is critical when using render passes, as transitions are not allowed inside
        StateTransitionDesc Barriers[] = {
            {m_VSConstants, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE},
            {m_CubeVertexBuffer, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_VERTEX_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE},
            {m_CubeIndexBuffer, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_INDEX_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE}
        };
        m_pImmediateContext->TransitionResourceStates(_countof(Barriers), Barriers);

    // Cube vertex/index buffers were already created with normals above. Keep them.
    printf("[Initialize] Cube VB=%p IB=%p\n", (void *)m_CubeVertexBuffer.RawPtr(), (void *)m_CubeIndexBuffer.RawPtr());

    // Initialize ECS world and systems, but do not create any objects here
    EnsureWorld();

    // Initialize project system: try to find nearest .c6proj, else create default in CWD
    try {
        m_Project = std::make_unique<ProjectSystem::ProjectManager>();
        namespace fs = std::filesystem;
        fs::path start = fs::current_path();
        auto proj = ProjectSystem::ProjectManager::FindNearestProject(start);
        if (!proj.empty()) {
            if (m_Project->Load(proj)) {
                std::cout << "[C6GE] Loaded project: " << m_Project->GetConfig().projectName << "\n";
            }
        } else {
            fs::path def = start / "C6GEProject.c6proj";
            if (m_Project->CreateDefault(def, "C6GE Project")) {
                std::cout << "[C6GE] Created default project at " << def.string() << "\n";
            }
        }
        // Auto-load startup world if present
        if (m_Project) {
            auto sw = m_Project->GetConfig().startupWorld;
            if (!sw.empty() && fs::exists(sw) && m_World) {
                // Local adapter to bridge to WorldIO
                class Adapter : public ProjectSystem::ECSWorldLike {
                    Diligent::ECS::World& W;
                public:
                    Adapter(Diligent::ECS::World& w):W(w){}
                    void Clear() override {
                        auto& reg = W.Registry();
                        std::vector<entt::entity> to_destroy;
                        auto view = reg.view<ECS::Name>();
                        for (auto e : view) to_destroy.push_back(e);
                        for (auto e : to_destroy) W.DestroyEntity(e);
                    }
                    void* CreateObject(const std::string& name) override {
                        auto obj = W.CreateObject(name);
                        return reinterpret_cast<void*>(static_cast<uintptr_t>(obj.Handle()));
                    }
                    void SetTransform(void* handle, const ProjectSystem::ECSWorldLike::TransformData& t) override {
                        auto e = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(handle));
                        auto& reg = W.Registry();
                        ECS::Transform tr; tr.position = float3{t.position[0],t.position[1],t.position[2]};
                        tr.rotationEuler = float3{t.rotationEuler[0],t.rotationEuler[1],t.rotationEuler[2]};
                        tr.scale = float3{t.scale[0],t.scale[1],t.scale[2]};
                        reg.emplace_or_replace<ECS::Transform>(e, tr);
                    }
                    void SetMesh(void* handle, ProjectSystem::ECSWorldLike::MeshKind kind, const std::string& assetId) override {
                        auto e = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(handle));
                        auto& reg = W.Registry();
                        if (kind == ProjectSystem::ECSWorldLike::MeshKind::StaticCube) {
                            reg.emplace_or_replace<ECS::StaticMesh>(e, ECS::StaticMesh{ECS::StaticMesh::MeshType::Cube});
                        } else if (kind == ProjectSystem::ECSWorldLike::MeshKind::DynamicGLTF) {
                            ECS::Mesh m; m.kind = ECS::Mesh::Kind::Dynamic; m.assetId = assetId; reg.emplace_or_replace<ECS::Mesh>(e, m);
                        }
                    }
                    std::vector<ProjectSystem::ECSWorldLike::ObjectViewItem> EnumerateObjects() const override {
                        std::vector<ProjectSystem::ECSWorldLike::ObjectViewItem> out; return out; /* not used for load */
                    }
                } adapter(*m_World);
                ProjectSystem::WorldIO::Load(sw, adapter);
            }
        }
    } catch (...) {
        std::cerr << "[C6GE] Project initialization failed (non-fatal)." << std::endl;
    }
    }

    void C6GERender::UpdateUI()
    {
        if (IsRuntime == true)
            return;

        try
        {
            if (!ImGui::GetCurrentContext())
            {
                std::cerr << "[C6GE] ImGui context is null in UpdateUI, skipping play/pause bar." << std::endl;
                return;
            }
            // Play/Pause bar at the top inside the viewport
            ImGui::SetNextWindowBgAlpha(0.85f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
            // Calculate position for play/pause bar (centered in viewport)
            float x = 0.0f, y = 0.0f;
            if (ImGui::FindWindowByName("Viewport"))
            {
                ImGuiWindow *vpWin = ImGui::FindWindowByName("Viewport");
                ImVec2 vpPos = vpWin->Pos;
                ImVec2 vpSize = vpWin->Size;
                const float iconSize = 40.0f;
                const float windowPad = 12.0f;
                float barWidth = iconSize + 2 * windowPad;
                x = vpPos.x + (vpSize.x - barWidth) * 0.5f;
                // Clamp y to always be at least at the top of the viewport, plus menu bar height if present
                float menuBarHeight = ImGui::GetFrameHeight();
                y = vpPos.y + menuBarHeight + 4.0f; // 4px padding below menu bar
                if (y < vpPos.y) y = vpPos.y;
            }
            ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
            // Note: DisplaySize/FramebufferScale are now set once per-frame in main.cpp.
            // Avoid overriding them here to prevent DPI-related clipping on macOS.
            ImGui::Begin("PlayPauseBar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);
            // Ensure play/pause bar is always above the viewport
            if (ImGui::FindWindowByName("PlayPauseBar"))
            {
                ImGui::BringWindowToDisplayFront(ImGui::FindWindowByName("PlayPauseBar"));
            }

            bool usedIcon = false;
            const float iconSize = 40.0f;
            if (m_PlayIconSRV && m_PauseIconSRV)
            {
                auto playSRV = m_PlayIconSRV.RawPtr();
                auto pauseSRV = m_PauseIconSRV.RawPtr();
                if (playSRV && pauseSRV)
                {
                    ImTextureRef icon = IsPaused() ? ImTextureRef(playSRV) : ImTextureRef(pauseSRV);
                    if (ImGui::ImageButton("##playpause", icon, ImVec2(iconSize, iconSize)))
                    {
                        TogglePlayState();
                    }
                    usedIcon = true;
                }
            }
            if (!usedIcon)
            {
                // Fallback: use text button
                if (ImGui::Button(IsPaused() ? "Play" : "Pause", ImVec2(iconSize * 2, iconSize)))
                {
                    TogglePlayState();
                }
            }
            ImGui::End();
            ImGui::PopStyleVar(3);
        }
        catch (const std::exception &e)
        {
            std::cerr << "[C6GE] Exception in UpdateUI play/pause bar: " << e.what() << std::endl;
        }
        catch (...)
        {
            std::cerr << "[C6GE] Unknown exception in UpdateUI play/pause bar." << std::endl;
        }

        // Remove fixed positioning - let the window be freely movable and resizable
        // Project window
        if (ImGui::Begin("Project", nullptr, ImGuiWindowFlags_None))
        {
            if (m_Project)
            {
                const auto& cfg = m_Project->GetConfig();
                ImGui::Text("Project: %s", cfg.projectName.c_str());
                ImGui::TextDisabled("Engine %s", cfg.engineVersion.c_str());
                if (ImGui::Button("Save Project")) { m_Project->Save(); }
                ImGui::SameLine();
                if (ImGui::Button("Save World"))
                {
                    if (m_World)
                    {
                        // Adapter for save
                        class Adapter : public ProjectSystem::ECSWorldLike {
                            Diligent::ECS::World& W;
                        public:
                            Adapter(Diligent::ECS::World& w):W(w){}
                            void Clear() override {}
                            void* CreateObject(const std::string&) override { return nullptr; }
                            void SetTransform(void*, const TransformData&) override {}
                            void SetMesh(void*, MeshKind, const std::string&) override {}
                            std::vector<ObjectViewItem> EnumerateObjects() const override {
                                std::vector<ObjectViewItem> out; auto& reg = W.Registry(); auto view = reg.view<ECS::Name>();
                                for (auto e : view) {
                                    ObjectViewItem it; it.handle = reinterpret_cast<void*>(static_cast<uintptr_t>(e)); it.name = view.get<ECS::Name>(e).value;
                                    it.hasTransform = reg.any_of<ECS::Transform>(e);
                                    if (it.hasTransform) {
                                        const auto& tr = reg.get<ECS::Transform>(e);
                                        it.tr.position[0]=tr.position.x; it.tr.position[1]=tr.position.y; it.tr.position[2]=tr.position.z;
                                        it.tr.rotationEuler[0]=tr.rotationEuler.x; it.tr.rotationEuler[1]=tr.rotationEuler.y; it.tr.rotationEuler[2]=tr.rotationEuler.z;
                                        it.tr.scale[0]=tr.scale.x; it.tr.scale[1]=tr.scale.y; it.tr.scale[2]=tr.scale.z;
                                    }
                                    it.meshKind = MeshKind::None; it.assetId.clear();
                                    if (reg.any_of<ECS::StaticMesh>(e)) it.meshKind = MeshKind::StaticCube;
                                    if (reg.any_of<ECS::Mesh>(e)) {
                                        const auto& m = reg.get<ECS::Mesh>(e);
                                        if (m.kind == ECS::Mesh::Kind::Static && m.staticType == ECS::Mesh::StaticType::Cube) it.meshKind = MeshKind::StaticCube;
                                        else if (m.kind == ECS::Mesh::Kind::Dynamic) { it.meshKind = MeshKind::DynamicGLTF; it.assetId = m.assetId; }
                                    }
                                    out.push_back(std::move(it));
                                }
                                return out;
                            }
                        } adapter(*m_World);
                        ProjectSystem::WorldIO::Save(cfg.startupWorld, adapter);
                    }
                }
                ImGui::Separator();
                // Worlds list
                if (ImGui::CollapsingHeader("Worlds", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto worlds = m_Project->ListWorldFiles();
                    for (auto& w : worlds)
                    {
                        auto label = w.filename().string();
                        if (ImGui::Selectable(label.c_str(), false))
                        {
                            if (m_World)
                            {
                                class Adapter : public ProjectSystem::ECSWorldLike {
                                    Diligent::ECS::World& W;
                                public: Adapter(Diligent::ECS::World& w):W(w){}
                                    void Clear() override { auto& reg=W.Registry(); std::vector<entt::entity> v; auto view = reg.view<ECS::Name>(); for (auto e: view) v.push_back(e); for (auto e: v) W.DestroyEntity(e);} 
                                    void* CreateObject(const std::string& name) override { auto obj=W.CreateObject(name); return reinterpret_cast<void*>(static_cast<uintptr_t>(obj.Handle())); }
                                    void SetTransform(void* h, const ProjectSystem::ECSWorldLike::TransformData& t) override { auto e=static_cast<entt::entity>(reinterpret_cast<uintptr_t>(h)); auto& reg=W.Registry(); ECS::Transform tr; tr.position=float3{t.position[0],t.position[1],t.position[2]}; tr.rotationEuler=float3{t.rotationEuler[0],t.rotationEuler[1],t.rotationEuler[2]}; tr.scale=float3{t.scale[0],t.scale[1],t.scale[2]}; reg.emplace_or_replace<ECS::Transform>(e,tr);} 
                                    void SetMesh(void* h, ProjectSystem::ECSWorldLike::MeshKind kind, const std::string& assetId) override { auto e=static_cast<entt::entity>(reinterpret_cast<uintptr_t>(h)); auto& reg=W.Registry(); if (kind==ProjectSystem::ECSWorldLike::MeshKind::StaticCube) reg.emplace_or_replace<ECS::StaticMesh>(e, ECS::StaticMesh{ECS::StaticMesh::MeshType::Cube}); else if (kind==ProjectSystem::ECSWorldLike::MeshKind::DynamicGLTF) { ECS::Mesh m; m.kind=ECS::Mesh::Kind::Dynamic; m.assetId=assetId; reg.emplace_or_replace<ECS::Mesh>(e,m);} }
                                    std::vector<ProjectSystem::ECSWorldLike::ObjectViewItem> EnumerateObjects() const override { return {}; }
                                } adapter(*m_World);
                                ProjectSystem::WorldIO::Load(w, adapter);
                                m_Project->GetConfig().startupWorld = w;
                            }
                        }
                    }
                    // New world quick-create
                    static char worldName[128] = "NewWorld";
                    ImGui::InputText("##WorldName", worldName, sizeof(worldName));
                    ImGui::SameLine();
                    if (ImGui::Button("New World"))
                    {
                        auto path = cfg.worldsDir / (std::string(worldName) + ".world");
                        if (m_World)
                        {
                            class Adapter : public ProjectSystem::ECSWorldLike { public: Adapter(Diligent::ECS::World&){} void Clear() override {} void* CreateObject(const std::string&) override { return nullptr;} void SetTransform(void*, const TransformData&) override {} void SetMesh(void*, MeshKind, const std::string&) override {} std::vector<ObjectViewItem> EnumerateObjects() const override { return {}; } } adapter(*m_World);
                            // Save current (maybe empty) as template
                            ProjectSystem::WorldIO::Save(path, adapter);
                            m_Project->GetConfig().startupWorld = path;
                        }
                    }
                }
                // Models list
                if (ImGui::CollapsingHeader("Models", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    if (ImGui::Button("Import glTF..."))
                    {
                        std::string picked;
                        if (Platform::OpenFileDialogGLTF(picked))
                        {
                            auto out = m_Project->ConvertGLTFToC6M(picked);
                            (void)out;
                        }
                    }
                    auto models = m_Project->ListModelFiles();
                    for (auto& m : models)
                    {
                        bool sel = false;
                        ImGui::Selectable(m.filename().string().c_str(), &sel);
                        if (ImGui::BeginPopupContextItem())
                        {
                            if (ImGui::MenuItem("Reveal in Explorer"))
                            {
                                // Not implemented cross-platform here
                            }
                            ImGui::EndPopup();
                        }
                    }
                }
            }
            else
            {
                ImGui::TextDisabled("No project loaded");
            }
        }
        ImGui::End();

        if (RenderSettingsOpen)
        {
            if (ImGui::Begin("Render Settings", &RenderSettingsOpen, ImGuiWindowFlags_NoCollapse))
            {
                if (ImGui::BeginTabBar("RenderSettingsTabs"))
                {
                    if (ImGui::BeginTabItem("Raster"))
                    {
                        ImGui::SliderFloat("Line Width", &m_LineWidth, 1.f, 10.f);

                        // MSAA Settings
                        if (ImGui::CollapsingHeader("Anti-Aliasing", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            if (!m_SupportedSampleCounts.empty())
                            {
                                // Create a list of sample count options
                                std::vector<const char*> sampleCountNames;
                                std::vector<Uint8> sampleCountValues;

                                for (Uint8 sampleCount : m_SupportedSampleCounts)
                                {
                                    static std::string names[8]; // Static storage for strings
                                    int index = static_cast<int>(sampleCountNames.size());
                                    if (sampleCount == 1)
                                        names[index] = "Disabled (1x)";
                                    else
                                        names[index] = std::to_string(sampleCount) + "x MSAA";

                                    sampleCountNames.push_back(names[index].c_str());
                                    sampleCountValues.push_back(sampleCount);
                                }

                                // Find current selection
                                int currentSelection = 0;
                                for (size_t i = 0; i < sampleCountValues.size(); ++i)
                                {
                                    if (sampleCountValues[i] == m_SampleCount)
                                    {
                                        currentSelection = static_cast<int>(i);
                                        break;
                                    }
                                }

                                if (ImGui::Combo("Sample Count", &currentSelection, sampleCountNames.data(), static_cast<int>(sampleCountNames.size())))
                                {
                                    Uint8 newSampleCount = sampleCountValues[currentSelection];
                                    if (newSampleCount != m_SampleCount)
                                    {
                                        m_SampleCount = newSampleCount;
                                        // Recreate MSAA render targets with new sample count
                                        CreateMSAARenderTarget();
                                    }
                                }
                            }
                            else
                            {
                                ImGui::Text("MSAA not supported on this device");
                            }
                        }

                        if (ImGui::CollapsingHeader("Shadows", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            ImGui::Checkbox("Shadow Maps", &RenderShadows);
                            static int shadowRes = (int)m_ShadowMapSize;
                            if (ImGui::SliderInt("Shadow Resolution", &shadowRes, 512, 8192, "%d"))
                            {
                                if (shadowRes != (int)m_ShadowMapSize)
                                {
                                    m_ShadowMapSize = shadowRes;
                                    CreateShadowMap();
                                }
                            }
                        }

                        if (ImGui::CollapsingHeader("Advanced", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            if (m_pMainRenderPass)
                            {
                                if (ImGui::Checkbox("Use Render Passes", &m_UseRenderPasses))
                                {
                                    // Clear cache when toggling
                                    m_FramebufferCache.clear();
                                }
                                ImGui::SameLine();
                                ImGui::TextDisabled("(?)");
                                if (ImGui::IsItemHovered())
                                {
                                    ImGui::BeginTooltip();
                                    ImGui::Text("Render passes optimize rendering on Vulkan/Metal");
                                    ImGui::Text("by reducing memory bandwidth usage");
                                    ImGui::EndTooltip();
                                }
                            }
                            else
                            {
                                ImGui::TextDisabled("Render Passes: Not supported");
                            }
                        }
                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem("Ray Tracing"))
                    {
                        ImGui::Text("Device supports ray tracing: %s", m_RayTracingSupported ? "Yes" : "No");
                        bool enabled = m_EnableRayTracing;
                        if (!m_RayTracingSupported)
                            ImGui::BeginDisabled();
                        if (ImGui::Checkbox("Enable Ray Tracing (DXR/VKRT)", &enabled))
                        {
                            m_EnableRayTracing = enabled;
                            if (m_EnableRayTracing)
                            {
                                if (!m_RayTracingInitialized)
                                    InitializeRayTracing();
                                std::cout << "[C6GE] Ray tracing enabled (experimental)." << std::endl;
                            }
                            else
                            {
                                if (m_RayTracingInitialized)
                                    DestroyRayTracing();
                                std::cout << "[C6GE] Ray tracing disabled." << std::endl;
                            }
                        }
                        if (!m_RayTracingSupported)
                            ImGui::EndDisabled();

                        ImGui::Separator();
                        ImGui::Text("Shadows");
                        ImGui::Checkbox("Soft Shadows", &m_SoftShadowsEnabled);
                        ImGui::SameLine();
                        ImGui::TextDisabled("(?)");
                        if (ImGui::IsItemHovered())
                        {
                            ImGui::BeginTooltip();
                            ImGui::Text("Soft shadows sample multiple jittered shadow rays within an angular cone.");
                            ImGui::EndTooltip();
                        }
                        if (m_SoftShadowsEnabled)
                        {
                            ImGui::SliderFloat("Light Angular Radius (rad)", &m_SoftShadowAngularRad, 0.0f, 0.2f, "%.3f");
                            ImGui::SliderInt("Soft Shadow Samples", &m_SoftShadowSamples, 1, 64);
                        }
                        else
                        {
                            ImGui::TextDisabled("Hard shadows active (1 sample, 0 cone radius)");
                        }

                        ImGui::SameLine();
                        ImGui::TextDisabled("(?)");
                        if (ImGui::IsItemHovered())
                        {
                            ImGui::BeginTooltip();
                            ImGui::Text("Set samples to 1 and radius to 0 for hard shadows. Higher samples improve quality at a cost.");
                            ImGui::EndTooltip();
                        }

                        ImGui::Separator();
                        ImGui::Text("Reflections");
                        ImGui::Checkbox("Enable Reflections (Plane)", &m_EnableRTReflections);

                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem("Post Processing"))
                    {
                        ImGui::Checkbox("Enable Post Processing", &m_EnablePostProcessing);
                        if (!m_pPostGammaPSO)
                            ImGui::TextDisabled("Gamma PSO not available (using fallback)");
                        ImGui::Checkbox("Gamma Correction", &m_PostGammaCorrection);
                        if (m_TAA)
                        {
                            ImGui::Separator();
                            ImGui::Checkbox("Enable TAA (Temporal AA)", &m_EnableTAA);
                            auto& TAA = *reinterpret_cast<TAASettings*>(&m_TAASettingsStorage);
                            ImGui::SliderFloat("Temporal Stability", &TAA.TemporalStabilityFactor, 0.0f, 0.999f, "%.4f");
                            bool reset = (TAA.ResetAccumulation != 0);
                            if (ImGui::Checkbox("Reset Accumulation", &reset))
                                TAA.ResetAccumulation = reset ? TRUE : FALSE;
                            bool skip = (TAA.SkipRejection != 0);
                            if (ImGui::Checkbox("Skip Rejection", &skip))
                                TAA.SkipRejection = skip ? TRUE : FALSE;
                        }
                        ImGui::TextDisabled("This sample applies a fullscreen gamma-correction pass to the framebuffer.\nMore effects (tone mapping, bloom) can be added next.");
                        ImGui::EndTabItem();
                    }

                    ImGui::EndTabBar();
                }
            }
            ImGui::End();
        }
    }

    void C6GERender::UpdateViewportUI()
    {
        {
            // Safety: ensure ImGui context exists and docking is enabled before using DockBuilder APIs
            if (ImGui::GetCurrentContext() == nullptr)
            {
                // ImGui context not ready yet; skip UI update this frame
                std::cerr << "[C6GE] ImGui context not ready, skipping viewport UI update." << std::endl;
                return;
            }
            if (!(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable))
            {
                // Docking not enabled; skip dockspace setup
                std::cerr << "[C6GE] ImGui docking not enabled, skipping viewport UI update." << std::endl;
                return;
            }

            // Create a fullscreen dockspace occupying the main viewport work area
            // Use WorkPos/WorkSize so OS title bars and ImGui main menu bar are accounted for.
            auto *viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);

            ImGuiWindowFlags dockspace_window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::Begin("DockSpaceWindow", nullptr, dockspace_window_flags);
            ImGui::PopStyleVar(3);

            // Main menu bar and settings tab logic
            static bool open_settings = false;
            static float font_size = 16.0f;
            static ImVec4 bg_color = ImVec4(0.07f, 0.07f, 0.07f, 1.0f);
            static ImVec4 text_color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

            if (ImGui::BeginMainMenuBar())
            {
                if (ImGui::BeginMenu("File"))
                {
                    if (ImGui::MenuItem("Settings"))
                    {
                        open_settings = true;
                    }
                    if (ImGui::MenuItem("Exit"))
                    {
                        // TODO: Implement exit logic if needed
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMainMenuBar();
            }

            if (open_settings)
            {
                ImGui::SetNextWindowDockID(ImGui::GetID("MainDockSpace"), ImGuiCond_Once);
                ImGui::SetNextWindowSize(ImVec2(480, 360), ImGuiCond_FirstUseEver);
                if (ImGui::Begin("Settings", &open_settings, ImGuiWindowFlags_NoCollapse))
                {
                    ImGui::SameLine();
                    ImGui::Text("Settings");
                    ImGui::Separator();
                    if (ImGui::BeginTabBar("SettingsTabBar"))
                    {
                        if (ImGui::BeginTabItem("Appearance"))
                        {
                            ImGui::Text("Appearance Settings");
                            ImGui::SliderFloat("Font Size", &font_size, 10.0f, 32.0f);
                            ImGui::ColorEdit4("Background Color", (float *)&bg_color);
                            ImGui::ColorEdit4("Text Color", (float *)&text_color);
                            if (ImGui::Button("Apply", ImVec2(0, 0)))
                            {
                                ImGui::GetStyle().Colors[ImGuiCol_WindowBg] = bg_color;
                                ImGui::GetStyle().Colors[ImGuiCol_Text] = text_color;
                                auto &io = ImGui::GetIO();
                                io.FontGlobalScale = font_size / 16.0f;
                            }
                            if (ImGui::Button("Open Render Settings", ImVec2(0, 0)))
                            {
                                RenderSettingsOpen = true;
                            }
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("Shortcuts"))
                        {
                            ImGui::Text("Shortcut settings coming soon...");
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("About"))
                        {
                            ImGui::Text("C6GE Century6 Game Engine");
                            ImGui::Separator();
                            ImGui::Text("License: Apache 2.0");
                            ImGui::Text("Version: 2026.1");
                            ImGui::Text("Century6.com/C6GE");
                            ImGui::EndTabItem();
                        }
                        ImGui::EndTabBar();
                    }
                }
                ImGui::End();
            }

            // Create dockspace
            ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
            // Force tab bars to always show, even for single window
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
            ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspace_id);
            if (node)
                node->LocalFlags &= ~ImGuiDockNodeFlags_AutoHideTabBar;

            // Setup default docking layout on first run
            static bool first_time = true;
            if (first_time)
            {
                first_time = false;
                // Validate viewport and dockspace id before using DockBuilder APIs
                if (viewport == nullptr)
                {
                    std::cerr << "[C6GE] UpdateViewportUI: main viewport is null, skipping dock setup." << std::endl;
                }
                else if (viewport->WorkSize.x <= 0.0f || viewport->WorkSize.y <= 0.0f)
                {
                    std::cerr << "[C6GE] UpdateViewportUI: invalid viewport WorkSize=" << viewport->WorkSize.x << "," << viewport->WorkSize.y << ", skipping dock setup." << std::endl;
                }
                else if (dockspace_id == 0)
                {
                    std::cerr << "[C6GE] UpdateViewportUI: dockspace id is 0, skipping dock setup." << std::endl;
                }
                else
                {
                    ImGui::DockBuilderRemoveNode(dockspace_id);
                    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
                    ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

                    // Split the dockspace
                    auto dock_left = ImGui::DockBuilderSplitNode(dockspace_id, (ImGuiDir)ImGuiDir_Left, 0.2f, nullptr, &dockspace_id);
                    auto dock_right = ImGui::DockBuilderSplitNode(dockspace_id, (ImGuiDir)ImGuiDir_Right, 0.25f, nullptr, &dockspace_id);
                    auto dock_down = ImGui::DockBuilderSplitNode(dockspace_id, (ImGuiDir)ImGuiDir_Down, 0.3f, nullptr, &dockspace_id);

                    // Dock windows to specific nodes
                    ImGui::DockBuilderDockWindow("Viewport", dockspace_id);
                    ImGui::DockBuilderDockWindow("Scene", dock_left);
                    ImGui::DockBuilderDockWindow("Properties", dock_right);
                    ImGui::DockBuilderDockWindow("Console", dock_down);
                    ImGui::DockBuilderDockWindow("Settings", dock_right);

                    ImGui::DockBuilderFinish(dockspace_id);
                }
            }

            ImGui::End();

            // Create the viewport window (now docked but moveable)
            // Use ImGuiWindowFlags_None for all docked windows so their title/menu bars are visible
            if (ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_None))
            {
                if (m_pFramebufferSRV)
                {
                    auto contentSize = ImGui::GetContentRegionAvail();

                    // Resize framebuffer if viewport size changed
                    Uint32 newWidth = static_cast<Uint32>(std::max(1.0f, contentSize.x));
                    Uint32 newHeight = static_cast<Uint32>(std::max(1.0f, contentSize.y));

                    if (newWidth != m_FramebufferWidth || newHeight != m_FramebufferHeight)
                    {
                        ResizeFramebuffer(newWidth, newHeight);
                        std::cout << "[C6GE] Resized framebuffer to " << newWidth << "x" << newHeight << std::endl;
                    }

                    // Choose which texture to display: post-processed, TAA, or raw framebuffer
                        // Use cached SRV decided at the end of Render()
                        ITextureView* pDisplaySRV = m_pViewportDisplaySRV ? m_pViewportDisplaySRV.RawPtr() : m_pFramebufferSRV.RawPtr();
                    // Draw the viewport image and remember its rectangle
                    ImGui::Image(reinterpret_cast<void*>(pDisplaySRV), contentSize);

                    // Viewport interaction: click-to-select (basic picking for cubes) and gizmo overlay
                    const ImVec2 imgMin = ImGui::GetItemRectMin();
                    const ImVec2 imgMax = ImGui::GetItemRectMax();
                    const ImVec2 imgSize = ImVec2(imgMax.x - imgMin.x, imgMax.y - imgMin.y);

                    // Mouse inside image?
                    const ImVec2 mousePos = ImGui::GetIO().MousePos;
                    const bool mouseOverImage = (mousePos.x >= imgMin.x && mousePos.x <= imgMax.x && mousePos.y >= imgMin.y && mousePos.y <= imgMax.y);

                    // Click to select objects (StaticMesh::Cube and glTF Mesh via AABB)
                    if (mouseOverImage && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    {
                        // Build picking ray in world space using inverse ViewProj
                        const float ndcX = (mousePos.x - imgMin.x) / imgSize.x * 2.0f - 1.0f;
                        const float ndcY = 1.0f - (mousePos.y - imgMin.y) / imgSize.y * 2.0f; // flip Y
                        const float4 ndcNear = float4{ndcX, ndcY, 0.0f, 1.0f};
                        const float4 ndcFar  = float4{ndcX, ndcY, 1.0f, 1.0f};
                        const auto InvVP = m_CameraViewProjMatrix.Inverse();
                        float4 wNear = ndcNear * InvVP;
                        float4 wFar  = ndcFar * InvVP;
                        if (wNear.w != 0) wNear = wNear / wNear.w;
                        if (wFar.w  != 0) wFar  = wFar / wFar.w;
                        const float3 RayOrigin = float3{wNear.x, wNear.y, wNear.z};
                        const float3 RayDir    = normalize(float3{wFar.x - wNear.x, wFar.y - wNear.y, wFar.z - wNear.z});

                        EnsureWorld();
                        entt::entity best = entt::null;
                        float bestT = +FLT_MAX;
                        if (m_World)
                        {
                            auto& reg = m_World->Registry();
                            // Test static cubes: unit cube in object space [-1,1]^3 transformed by entity Transform
                            auto view = reg.view<ECS::Transform, ECS::StaticMesh>();
                            for (auto e : view)
                            {
                                const auto& sm = view.get<ECS::StaticMesh>(e);
                                if (sm.type != ECS::StaticMesh::MeshType::Cube)
                                    continue;
                                const auto& tr = view.get<ECS::Transform>(e);
                                const float4x4 World = tr.WorldMatrix();
                                const float4x4 InvWorld = World.Inverse();
                                // Transform ray to object space
                                const float4 ro4 = float4{RayOrigin.x, RayOrigin.y, RayOrigin.z, 1.0f} * InvWorld;
                                const float4 rd4 = float4{RayDir.x, RayDir.y, RayDir.z, 0.0f} * InvWorld;
                                const float3 rayOObj = float3{ro4.x, ro4.y, ro4.z};
                                const float3 rayDObj = normalize(float3{rd4.x, rd4.y, rd4.z});

                                // Axis-aligned unit cube AABB in object space
                                const BoundBox AABB{float3{-1.f, -1.f, -1.f}, float3{+1.f, +1.f, +1.f}};
                                float tEnter = 0, tExit = 0;
                                if (IntersectRayAABB(rayOObj, rayDObj, AABB, tEnter, tExit))
                                {
                                    if (tEnter >= 0 && tEnter < bestT)
                                    {
                                        bestT = tEnter;
                                        best = e;
                                    }
                                }
                            }

                            // Test dynamic glTF meshes using the asset's model-space AABB
                            auto viewGltf = reg.view<ECS::Transform, ECS::Mesh>();
                            for (auto e : viewGltf)
                            {
                                const auto& m = viewGltf.get<ECS::Mesh>(e);
                                if (m.kind != ECS::Mesh::Kind::Dynamic || m.assetId.empty())
                                    continue;
                                auto itAsset = m_GltfAssets.find(m.assetId);
                                if (itAsset == m_GltfAssets.end())
                                    continue;
                                const BoundBox& AABB = itAsset->second.Bounds;
                                // Some models may have invalid BB if empty
                                if (AABB.Min.x > AABB.Max.x || AABB.Min.y > AABB.Max.y || AABB.Min.z > AABB.Max.z)
                                    continue;
                                const auto& tr = viewGltf.get<ECS::Transform>(e);
                                const float4x4 World = tr.WorldMatrix();
                                const float4x4 InvWorld = World.Inverse();
                                const float4 ro4 = float4{RayOrigin.x, RayOrigin.y, RayOrigin.z, 1.0f} * InvWorld;
                                const float4 rd4 = float4{RayDir.x, RayDir.y, RayDir.z, 0.0f} * InvWorld;
                                const float3 rayOObj = float3{ro4.x, ro4.y, ro4.z};
                                const float3 rayDObj = normalize(float3{rd4.x, rd4.y, rd4.z});
                                float tEnter = 0, tExit = 0;
                                if (IntersectRayAABB(rayOObj, rayDObj, AABB, tEnter, tExit))
                                {
                                    if (tEnter >= 0 && tEnter < bestT)
                                    {
                                        bestT = tEnter;
                                        best = e;
                                    }
                                }
                            }
                        }
                        if (best != entt::null)
                        {
                            m_SelectedEntity = best;
                        }
                    }

                    // Initialize ImGuizmo drawing area to match the viewport image
                    ImGuizmo::BeginFrame();
                    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
                    ImGuizmo::SetOrthographic(false);
                    ImGuizmo::SetRect(imgMin.x, imgMin.y, imgSize.x, imgSize.y);

                    // Hotkeys for gizmo op when mouse is over the image
                    if (mouseOverImage && ImGui::IsWindowFocused())
                    {
                        if (ImGui::IsKeyPressed(ImGuiKey_W)) m_GizmoOperation = GizmoOperation::Translate;
                        if (ImGui::IsKeyPressed(ImGuiKey_E)) m_GizmoOperation = GizmoOperation::Rotate;
                        if (ImGui::IsKeyPressed(ImGuiKey_R)) m_GizmoOperation = GizmoOperation::Scale;
                    }

                    // Gizmo overlay: translate gizmo centered on object (no boxed widget)
                    EnsureWorld();
                    if (m_World)
                    {
                        auto& reg = m_World->Registry();
                        if (m_SelectedEntity != entt::null && reg.valid(m_SelectedEntity) && reg.any_of<ECS::Transform>(m_SelectedEntity))
                        {
                            auto& tr = reg.get<ECS::Transform>(m_SelectedEntity);
                            // Tiny toolbar in the top-left of the image: T / R / S
                            ImDrawList* dl = ImGui::GetWindowDrawList();
                            const float pad = 6.0f;
                            ImVec2 tbPos = ImVec2(imgMin.x + pad, imgMin.y + pad);
                            ImGui::SetCursorScreenPos(tbPos);
                            ImGui::BeginGroup();
                            {
                                auto button = [&](const char* label, bool active){
                                    if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.05f,0.80f,0.75f,0.35f));
                                    bool pressed = ImGui::Button(label);
                                    if (active) ImGui::PopStyleColor();
                                    ImGui::SameLine(0, 4);
                                    return pressed;
                                };
                                bool t = (m_GizmoOperation == GizmoOperation::Translate);
                                bool r = (m_GizmoOperation == GizmoOperation::Rotate);
                                bool s = (m_GizmoOperation == GizmoOperation::Scale);
                                if (button("T", t)) m_GizmoOperation = GizmoOperation::Translate;
                                if (button("R", r)) m_GizmoOperation = GizmoOperation::Rotate;
                                if (button("S", s)) m_GizmoOperation = GizmoOperation::Scale;
                            }
                            ImGui::EndGroup();
                            
                            // If switching into Rotate, (re)initialize axis-angle from current transform
                            if (m_GizmoOperation == GizmoOperation::Rotate && m_LastGizmoOperation != GizmoOperation::Rotate)
                            {
                                const QuaternionF qx = QuaternionF::RotationFromAxisAngle(float3{1,0,0}, tr.rotationEuler.x);
                                const QuaternionF qy = QuaternionF::RotationFromAxisAngle(float3{0,1,0}, tr.rotationEuler.y);
                                const QuaternionF qz = QuaternionF::RotationFromAxisAngle(float3{0,0,1}, tr.rotationEuler.z);
                                const QuaternionF q  = qx * qy * qz;
                                float3 axis{}; float angle = 0;
                                QuaternionF(q).GetAxisAngle(axis, angle);
                                if (length(axis) == 0) axis = float3{1,0,0};
                                m_GizmoAxisAngle = float4{axis.x, axis.y, axis.z, angle};
                                m_GizmoEntityLast = m_SelectedEntity;
                            }

                            // Build local axis basis (unit vectors) in world space
                            auto BuildRotationOnly = [](const float3& euler)->float3x3{
                                const float3x3 Rx = float3x3::RotationX(euler.x);
                                const float3x3 Ry = float3x3::RotationY(euler.y);
                                const float3x3 Rz = float3x3::RotationZ(euler.z);
                                return Rx * Ry * Rz; // matches Transform::WorldMatrix composition order
                            };

                            const float3x3 R = BuildRotationOnly(tr.rotationEuler);
                            // Diligent's Matrix3x3 * Vector3 operator requires the vector as a non-const lvalue
                            float3 ex{1,0,0};
                            float3 ey{0,1,0};
                            float3 ez{0,0,1};
                            const float3 axesLocalW[3] = {
                                normalize(m_GizmoLocal ? (R * ex) : ex),
                                normalize(m_GizmoLocal ? (R * ey) : ey),
                                normalize(m_GizmoLocal ? (R * ez) : ez)
                            };

                            // Helpers to project world->screen (in pixels within the image rect)
                            auto WorldToScreen = [&](const float3& p)->ImVec2{
                                const float4 hp = float4{p.x, p.y, p.z, 1.0f} * m_CameraViewProjMatrix;
                                float2 ndc = float2{hp.x, hp.y} / (hp.w != 0.0f ? hp.w : 1.0f);
                                ImVec2 uv = ImVec2(0.5f * ndc.x + 0.5f, 0.5f * -ndc.y + 0.5f);
                                return ImVec2(imgMin.x + uv.x * imgSize.x, imgMin.y + uv.y * imgSize.y);
                            };

                            // Compute screen position of object origin
                            const float3 objPosW = tr.position;
                            const ImVec2 originSS = WorldToScreen(objPosW);

                            // Axis screen directions and endpoints with constant on-screen length
                            const float axisLenPx = 90.0f;
                            ImU32 axisCol[3] = { IM_COL32(235,70,70,255), IM_COL32(70,235,90,255), IM_COL32(70,140,235,255) };
                            ImU32 axisColHL[3] = { IM_COL32(255,140,140,255), IM_COL32(140,255,170,255), IM_COL32(140,200,255,255) };

                            // Build rays for mouse position (for dragging) and compute camera position
                            auto BuildRay = [&](const ImVec2& mp){
                                const float ndcX = (mp.x - imgMin.x) / imgSize.x * 2.0f - 1.0f;
                                const float ndcY = 1.0f - (mp.y - imgMin.y) / imgSize.y * 2.0f;
                                const float4 ndcNear = float4{ndcX, ndcY, 0.0f, 1.0f};
                                const float4 ndcFar  = float4{ndcX, ndcY, 1.0f, 1.0f};
                                const auto InvVP = m_CameraViewProjMatrix.Inverse();
                                float4 wNear = ndcNear * InvVP;
                                float4 wFar  = ndcFar  * InvVP;
                                if (wNear.w != 0) wNear = wNear / wNear.w;
                                if (wFar.w  != 0) wFar  = wFar  / wFar.w;
                                float3 ro = float3{wNear.x, wNear.y, wNear.z};
                                float3 rd = normalize(float3{wFar.x - wNear.x, wFar.y - wNear.y, wFar.z - wNear.z});
                                return std::pair<float3,float3>{ro, rd};
                            };

                            // Camera world position from inverse view
                            const float4x4 InvView = m_ViewMatrix.Inverse();
                            float4 CamOrigin4 = float4{0,0,0,1} * InvView;
                            const float3 CamPos = float3{CamOrigin4.x, CamOrigin4.y, CamOrigin4.z};
                            const float3 CamDir = normalize(objPosW - CamPos);
                            // Camera right vector in world space (used for blue axis special-case movement)
                            float4 CamRight4 = float4{1,0,0,0} * InvView;
                            const float3 CamRight = normalize(float3{CamRight4.x, CamRight4.y, CamRight4.z});
                            // Camera forward vector in world (used for red axis special-case)
                            float4 CamFwd4 = float4{0,0,-1,0} * InvView; // RH: -Z is forward
                            const float3 CamForward = normalize(float3{CamFwd4.x, CamFwd4.y, CamFwd4.z});

                            // Determine hover axis and draw axes
                            int hoveredAxis = -1;
                            float bestDist = 1e9f;
                            struct AxisVis { ImVec2 a, b; ImU32 col; } vis[3]{};
                            for (int i=0;i<3;++i)
                            {
                                // Project a point slightly along the axis to get screen direction
                                const ImVec2 p0 = originSS;
                                ImVec2 end;
                                if (i == 2)
                                {
                                    // Blue axis: fixed screen-right arrow (do not reorient to camera)
                                    end = ImVec2(p0.x + axisLenPx, p0.y);
                                }
                                else if (i == 0)
                                {
                                    // Red axis: use camera-forward projected direction
                                    const ImVec2 p1 = WorldToScreen(objPosW + CamForward);
                                    ImVec2 dir = ImVec2(p1.x - p0.x, p1.y - p0.y);
                                    const float len = std::sqrt(dir.x*dir.x + dir.y*dir.y);
                                    if (len < 1e-3f) dir = ImVec2(0,-1); else { dir.x/=len; dir.y/=len; }
                                    end = ImVec2(p0.x + dir.x*axisLenPx, p0.y + dir.y*axisLenPx);
                                }
                                else
                                {
                                    const ImVec2 p1 = WorldToScreen(objPosW + axesLocalW[i]);
                                    ImVec2 dir = ImVec2(p1.x - p0.x, p1.y - p0.y);
                                    const float len = std::sqrt(dir.x*dir.x + dir.y*dir.y);
                                    if (len < 1e-3f) dir = ImVec2(1,0); else { dir.x/=len; dir.y/=len; }
                                    end = ImVec2(p0.x + dir.x*axisLenPx, p0.y + dir.y*axisLenPx);
                                }
                                vis[i] = {p0, end, axisCol[i]};
                                // Hover test: distance from mouse to segment
                                auto distPtSeg = [](ImVec2 p, ImVec2 a, ImVec2 b){
                                    ImVec2 ab{b.x-a.x, b.y-a.y};
                                    ImVec2 ap{p.x-a.x, p.y-a.y};
                                    float t = (ab.x*ap.x + ab.y*ap.y) / (ab.x*ab.x + ab.y*ab.y + 1e-5f);
                                    t = std::max(0.0f, std::min(1.0f, t));
                                    ImVec2 c{a.x + ab.x*t, a.y + ab.y*t};
                                    ImVec2 d{p.x - c.x, p.y - c.y};
                                    return std::sqrt(d.x*d.x + d.y*d.y);
                                };
                                const float d = distPtSeg(mousePos, p0, end);
                                if (mouseOverImage && d < 12.0f && d < bestDist)
                                {
                                    bestDist = d;
                                    hoveredAxis = i;
                                }
                            }

                            // Begin dragging on click over an axis
                            if (!m_GizmoDragging && mouseOverImage && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hoveredAxis != -1)
                            {
                                m_GizmoDragging  = true;
                                m_GizmoAxis      = hoveredAxis;
                                // Blue axis uses camera-right; red uses camera-forward; green uses local/world Y
                                if (hoveredAxis == 2)      m_GizmoAxisDirW = CamRight;
                                else if (hoveredAxis == 0) m_GizmoAxisDirW = CamForward;
                                else                       m_GizmoAxisDirW = axesLocalW[hoveredAxis];
                                m_GizmoStartPosW = tr.position;
                                m_GizmoStartScale= tr.scale;
                                // Define drag plane that contains the axis and is most facing the camera at drag start
                                const float3 CamDir0 = normalize(objPosW - CamPos);
                                m_GizmoDragPlaneNormal = normalize(cross(m_GizmoAxisDirW, cross(CamDir0, m_GizmoAxisDirW)));
                                m_GizmoDragPlanePoint  = m_GizmoStartPosW;
                                auto [ro0, rd0] = BuildRay(mousePos);
                                // Ray-plane intersection at start
                                float denom = dot(rd0, m_GizmoDragPlaneNormal);
                                float t = 0.0f;
                                if (std::abs(denom) > 1e-4f)
                                {
                                    t = dot((m_GizmoDragPlanePoint - ro0), m_GizmoDragPlaneNormal) / denom;
                                }
                                const float3 hit = ro0 + rd0 * t;
                                m_GizmoStartT = dot(hit - m_GizmoDragPlanePoint, m_GizmoAxisDirW); // param along axis at start
                            }

                            // Update dragging
                            if (m_GizmoDragging)
                            {
                                // End drag when mouse released or mouse leaves image while released
                                if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                                {
                                    m_GizmoDragging = false;
                                }
                                else
                                {
                                    // Recompute intersection on the saved drag plane (constant during drag)
                                    const float3 axis = m_GizmoAxisDirW;
                                    auto [ro, rd] = BuildRay(mousePos);
                                    float denom = dot(rd, m_GizmoDragPlaneNormal);
                                    float t = 0.0f;
                                    if (std::abs(denom) > 1e-4f)
                                    {
                                        t = dot((m_GizmoDragPlanePoint - ro), m_GizmoDragPlaneNormal) / denom;
                                    }
                                    const float3 hit = ro + rd * t;
                                    const float currT = dot(hit - m_GizmoDragPlanePoint, axis);
                                    const float deltaT = currT - m_GizmoStartT;
                                    if (m_GizmoOperation == GizmoOperation::Translate)
                                    {
                                        tr.position = m_GizmoStartPosW + axis * deltaT;
                                    }
                                    else if (m_GizmoOperation == GizmoOperation::Scale)
                                    {
                                        // Scale sensitivity: 1 world unit along axis => +1 scale on that axis
                                        float3 newScale = m_GizmoStartScale;
                                        if (std::abs(axis.x) > 0.5f) newScale.x = std::max(0.0001f, m_GizmoStartScale.x + deltaT);
                                        if (std::abs(axis.y) > 0.5f) newScale.y = std::max(0.0001f, m_GizmoStartScale.y + deltaT);
                                        if (std::abs(axis.z) > 0.5f) newScale.z = std::max(0.0001f, m_GizmoStartScale.z + deltaT);
                                        tr.scale = newScale;
                                    }
                                }
                            }

                            if (m_GizmoOperation == GizmoOperation::Translate || m_GizmoOperation == GizmoOperation::Scale)
                            {
                                // Draw axes (highlight active/hovered)
                                for (int i=0;i<3;++i)
                                {
                                    const bool active = (m_GizmoDragging && i == m_GizmoAxis) || (!m_GizmoDragging && i == hoveredAxis);
                                    const ImU32 col = active ? axisColHL[i] : axisCol[i];
                                    // Drop shadow
                                    dl->AddLine(vis[i].a, vis[i].b, IM_COL32(0,0,0,120), 7.0f);
                                    // Main colored line
                                    dl->AddLine(vis[i].a, vis[i].b, col, 5.0f);
                                    // Arrow head
                                    ImVec2 ab{vis[i].b.x - vis[i].a.x, vis[i].b.y - vis[i].a.y};
                                    float L = std::sqrt(ab.x*ab.x + ab.y*ab.y);
                                    if (L > 1.0f)
                                    {
                                        ImVec2 dir{ab.x/L, ab.y/L};
                                        ImVec2 ort{-dir.y, dir.x};
                                        float ah = 14.0f; // arrow size
                                        ImVec2 p0 = ImVec2(vis[i].b.x, vis[i].b.y);
                                        ImVec2 p1 = ImVec2(vis[i].b.x - dir.x*ah + ort.x*ah*0.5f, vis[i].b.y - dir.y*ah + ort.y*ah*0.5f);
                                        ImVec2 p2 = ImVec2(vis[i].b.x - dir.x*ah - ort.x*ah*0.5f, vis[i].b.y - dir.y*ah - ort.y*ah*0.5f);
                                        // Shadow then fill
                                        ImU32 shadow = IM_COL32(0,0,0,120);
                                        dl->AddTriangleFilled(p0, p1, p2, shadow);
                                        dl->AddTriangleFilled(p0, p1, p2, col);
                                    }
                                }
                            }
                            else if (m_GizmoOperation == GizmoOperation::Rotate || m_GizmoOperation == GizmoOperation::Scale)
                            {
                                // Use ImGuizmo for rotate/scale manipulators
                                // Prepare matrices as float arrays (row-major as stored by Diligent's float4x4)
                                auto ToFloatArray = [](const float4x4& M)
                                {
                                    std::array<float,16> a{};
                                    std::memcpy(a.data(), &M, sizeof(float) * 16);
                                    return a;
                                };

                                auto viewM = ToFloatArray(m_ViewMatrix);
                                auto projM = ToFloatArray(m_ProjMatrix);
                                float4x4 world = tr.WorldMatrix();
                                auto modelM = ToFloatArray(world);

                                ImGuizmo::OPERATION op = (m_GizmoOperation == GizmoOperation::Rotate)
                                                            ? ImGuizmo::ROTATE
                                                            : ImGuizmo::SCALE;
                                ImGuizmo::MODE mode = m_GizmoLocal ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

                                // Manipulate updates modelM in-place when active
                                ImGuizmo::Manipulate(viewM.data(), projM.data(), op, mode, modelM.data());

                                if (ImGuizmo::IsUsing())
                                {
                                    float translation[3], rotationDeg[3], scale[3];
                                    ImGuizmo::DecomposeMatrixToComponents(modelM.data(), translation, rotationDeg, scale);
                                    tr.position = float3{translation[0], translation[1], translation[2]};
                                    tr.scale    = float3{std::max(0.0001f, scale[0]), std::max(0.0001f, scale[1]), std::max(0.0001f, scale[2])};
                                    // Convert degrees to radians for our Euler storage
                                    const float deg2rad = PI_F / 180.0f;
                                    tr.rotationEuler = float3{rotationDeg[0] * deg2rad, rotationDeg[1] * deg2rad, rotationDeg[2] * deg2rad};
                                }
                            }

                            // Remember last op
                            m_LastGizmoOperation = m_GizmoOperation;
                        }
                    }
                }
                else
                {
                    ImGui::Text("Framebuffer not ready");
                }
            }
            ImGui::End();

            // Create additional docked windows
            if (ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_None))
            {
                ImGui::Text("Scene Hierarchy");
                ImGui::Separator();

                // List all objects with Name component
                EnsureWorld();
                if (m_World)
                {
                    // Toolbar: create objects
                    static char newNameBuf[128] = "NewObject";
                    ImGui::InputText("##NewName", newNameBuf, sizeof(newNameBuf));
                    ImGui::SameLine();
                    if (ImGui::Button("+ Empty"))
                    {
                        // Generate unique name if needed
                        std::string base = (strlen(newNameBuf) > 0) ? std::string(newNameBuf) : std::string("Object");
                        std::string name = base;
                        int suffix = 1;
                        while (m_World->HasObject(name)) {
                            name = base + " (" + std::to_string(suffix++) + ")";
                        }
                        try {
                            auto obj = m_World->CreateObject(name);
                            auto& reg = m_World->Registry();
                            // Ensure it has a Transform by default
                            reg.emplace_or_replace<ECS::Transform>(obj.Handle(), ECS::Transform{});
                            m_SelectedEntity = obj.Handle();
                        } catch(...) {}
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("+ Cube"))
                    {
                        std::string base = (strlen(newNameBuf) > 0) ? std::string(newNameBuf) : std::string("Cube");
                        std::string name = base;
                        int suffix = 1;
                        while (m_World->HasObject(name)) {
                            name = base + " (" + std::to_string(suffix++) + ")";
                        }
                        try {
                            auto obj = m_World->CreateObject(name);
                            auto& reg = m_World->Registry();
                            reg.emplace_or_replace<ECS::Transform>(obj.Handle(), ECS::Transform{});
                            reg.emplace_or_replace<ECS::StaticMesh>(obj.Handle(), ECS::StaticMesh{ECS::StaticMesh::MeshType::Cube});
                            m_SelectedEntity = obj.Handle();
                        } catch(...) {}
                    }

                    auto& reg = m_World->Registry();
                    // Build a view of all entities with Name
                    auto view = reg.view<ECS::Name>();

                    for (auto e : view)
                    {
                        const auto& name = view.get<ECS::Name>(e).value;
                        bool selected = (m_SelectedEntity == e);
                        if (ImGui::Selectable(name.c_str(), selected))
                        {
                            m_SelectedEntity = e;
                        }
                        // Context menu to deselect
                        if (ImGui::BeginPopupContextItem())
                        {
                            if (ImGui::MenuItem("Deselect"))
                                m_SelectedEntity = entt::null;
                            if (ImGui::MenuItem("Delete"))
                            {
                                // Clear selection if deleting selected
                                if (m_SelectedEntity == e)
                                    m_SelectedEntity = entt::null;
                                // Prefer to delete by entity to keep consistent with maps
                                m_World->DestroyEntity(e);
                            }
                            ImGui::EndPopup();
                        }
                    }
                }
                else
                {
                    ImGui::TextDisabled("No world");
                }
            }
            ImGui::End();

            if (ImGui::Begin("Properties", nullptr, ImGuiWindowFlags_None))
            {
                ImGui::Text("Inspector");
                ImGui::Separator();
                EnsureWorld();
                if (!m_World)
                {
                    ImGui::TextDisabled("No world");
                }
                else
                {
                    auto& reg = m_World->Registry();
                    if (m_SelectedEntity == entt::null || !reg.valid(m_SelectedEntity))
                    {
                        ImGui::TextDisabled("No selection");
                    }
                    else
                    {
                        // Name component
                        if (reg.any_of<ECS::Name>(m_SelectedEntity))
                        {
                            auto& name = reg.get<ECS::Name>(m_SelectedEntity);
                            char buf[256];
                            strncpy(buf, name.value.c_str(), sizeof(buf));
                            buf[sizeof(buf)-1] = '\0';
                            if (ImGui::InputText("Name", buf, sizeof(buf)))
                            {
                                // Attempt to rename through world to keep maps in sync
                                if (!m_World->RenameEntity(m_SelectedEntity, std::string(buf)))
                                {
                                    // Failed rename: keep existing name and show hint
                                    ImGui::SameLine();
                                    ImGui::TextDisabled("(name taken)");
                                }
                            }
                        }

                        // Add/Remove and Delete controls
                        if (ImGui::Button("Delete Object"))
                        {
                            m_World->DestroyEntity(m_SelectedEntity);
                            m_SelectedEntity = entt::null;
                            ImGui::End();
                            // Early exit to avoid using invalid entity
                            goto end_properties_window;
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Add Component"))
                            ImGui::OpenPopup("AddComponentPopup");
                        if (ImGui::BeginPopup("AddComponentPopup"))
                        {
                            if (!reg.any_of<ECS::Transform>(m_SelectedEntity))
                            {
                                if (ImGui::MenuItem("Transform"))
                                {
                                    reg.emplace<ECS::Transform>(m_SelectedEntity, ECS::Transform{});
                                    ImGui::CloseCurrentPopup();
                                }
                            }
                            if (!reg.any_of<ECS::StaticMesh>(m_SelectedEntity))
                            {
                                if (ImGui::BeginMenu("Static Mesh"))
                                {
                                    if (ImGui::MenuItem("Cube"))
                                    {
                                        reg.emplace<ECS::StaticMesh>(m_SelectedEntity, ECS::StaticMesh{ECS::StaticMesh::MeshType::Cube});
                                        if (!reg.any_of<ECS::Transform>(m_SelectedEntity))
                                            reg.emplace<ECS::Transform>(m_SelectedEntity, ECS::Transform{});
                                        ImGui::CloseCurrentPopup();
                                    }
                                    ImGui::EndMenu();
                                }
                            }
                            if (!reg.any_of<ECS::Mesh>(m_SelectedEntity))
                            {
                                if (ImGui::MenuItem("Mesh"))
                                {
                                    reg.emplace<ECS::Mesh>(m_SelectedEntity, ECS::Mesh{});
                                    if (!reg.any_of<ECS::Transform>(m_SelectedEntity))
                                        reg.emplace<ECS::Transform>(m_SelectedEntity, ECS::Transform{});
                                    ImGui::CloseCurrentPopup();
                                }
                            }
                            ImGui::EndPopup();
                        }

                        // Transform editor
                        if (reg.any_of<ECS::Transform>(m_SelectedEntity))
                        {
                            if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
                            {
                                auto& tr = reg.get<ECS::Transform>(m_SelectedEntity);
                                float pos[3] = { tr.position.x, tr.position.y, tr.position.z };
                                float rotDeg[3] = { tr.rotationEuler.x * 180.0f/PI_F, tr.rotationEuler.y * 180.0f/PI_F, tr.rotationEuler.z * 180.0f/PI_F };
                                float scl[3] = { tr.scale.x, tr.scale.y, tr.scale.z };

                                if (ImGui::DragFloat3("Position", pos, 0.1f))
                                {
                                    tr.position = float3{pos[0], pos[1], pos[2]};
                                }
                                if (ImGui::DragFloat3("Rotation (deg)", rotDeg, 0.5f))
                                {
                                    tr.rotationEuler = float3{rotDeg[0] * PI_F/180.0f, rotDeg[1] * PI_F/180.0f, rotDeg[2] * PI_F/180.0f};
                                }
                                if (ImGui::DragFloat3("Scale", scl, 0.05f))
                                {
                                    // Prevent zero scale
                                    for (int i=0;i<3;++i) if (scl[i] == 0.0f) scl[i] = 0.0001f;
                                    tr.scale = float3{scl[0], scl[1], scl[2]};
                                }
                            }
                        }

                        // StaticMesh info
                        if (reg.any_of<ECS::StaticMesh>(m_SelectedEntity))
                        {
                            if (ImGui::CollapsingHeader("Static Mesh", ImGuiTreeNodeFlags_DefaultOpen))
                            {
                                const auto& sm = reg.get<ECS::StaticMesh>(m_SelectedEntity);
                                const char* meshType = (sm.type == ECS::StaticMesh::MeshType::Cube) ? "Cube" : "Unknown";
                                ImGui::Text("Type: %s", meshType);
                            }
                        }

                        // Mesh component (built-in or glTF)
                        if (reg.any_of<ECS::Mesh>(m_SelectedEntity))
                        {
                            auto& mesh = reg.get<ECS::Mesh>(m_SelectedEntity);
                            if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen))
                            {
                                // Kind selector
                                int kind = mesh.kind == ECS::Mesh::Kind::Static ? 0 : 1;
                                if (ImGui::Combo("Type", &kind, "Static\0Dynamic (glTF)\0"))
                                {
                                    mesh.kind = (kind == 0) ? ECS::Mesh::Kind::Static : ECS::Mesh::Kind::Dynamic;
                                }
                                if (mesh.kind == ECS::Mesh::Kind::Static)
                                {
                                    // Static built-in selection
                                    int staticType = (mesh.staticType == ECS::Mesh::StaticType::Cube) ? 0 : 0;
                                    if (ImGui::Combo("Static Mesh", &staticType, "Cube\0"))
                                    {
                                        mesh.staticType = ECS::Mesh::StaticType::Cube;
                                    }
                                }
                                else
                                {
                                    // Dynamic: show asset id and controls to assign
                                    char pathBuf[512];
                                    strncpy(pathBuf, mesh.assetId.c_str(), sizeof(pathBuf));
                                    pathBuf[sizeof(pathBuf)-1] = '\0';
                                    ImGui::InputText("Asset Id / Path", pathBuf, sizeof(pathBuf));
                                    if (ImGui::Button("Load from Path"))
                                    {
                                        mesh.assetId = pathBuf;
                                        // Attempt to load immediately
                                        LoadGLTFAsset(mesh.assetId);
                                    }
                                    ImGui::SameLine();
                                    if (ImGui::Button("Browse..."))
                                    {
                                        std::string picked;
                                        if (Platform::OpenFileDialogGLTF(picked))
                                        {
                                            mesh.assetId = picked;
                                            LoadGLTFAsset(mesh.assetId);
                                        }
                                    }
                                    ImGui::SameLine();
                                    if (ImGui::Button("Engine Models"))
                                    {
                                        ImGui::OpenPopup("EngineModelPopup");
                                    }
                                    if (ImGui::BeginPopup("EngineModelPopup"))
                                    {
                                        // Show a few curated sample models from external/gltfassets/Models
                                        struct BuiltinEntry { const char* Label; const char* RelPath; };
                                        static BuiltinEntry entries[] = {
                                            {"DamagedHelmet", "external/gltfassets/Models/DamagedHelmet/glTF/DamagedHelmet.gltf"},
                                            {"Duck",          "external/gltfassets/Models/Duck/glTF/Duck.gltf"},
                                            {"Sponza",        "external/gltfassets/Models/Sponza/glTF/Sponza.gltf"},
                                            {"Suzanne",       "external/gltfassets/Models/Suzanne/glTF/Suzanne.gltf"},
                                            {"BoomBox",       "external/gltfassets/Models/BoomBox/glTF/BoomBox.gltf"},
                                        };
                                        for (auto& e : entries)
                                        {
                                            if (ImGui::Selectable(e.Label))
                                            {
                                                mesh.assetId = e.RelPath;
                                                LoadGLTFAsset(mesh.assetId);
                                                ImGui::CloseCurrentPopup();
                                            }
                                        }
                                        ImGui::EndPopup();
                                    }
                                }
                            }
                        }

                    }
                }
            }
end_properties_window:
            ImGui::End();

            if (ImGui::Begin("Console", nullptr, ImGuiWindowFlags_None))
            {
                ImGui::Text("Console Output");
                // Add console content here
            }
            ImGui::End();
        }
    }

    void C6GERender::DXSDKMESH_VERTEX_ELEMENTtoInputLayoutDesc(const DXSDKMESH_VERTEX_ELEMENT *VertexElement,
                                                               Uint32 Stride,
                                                               InputLayoutDesc &Layout,
                                                               std::vector<LayoutElement> &Elements)
    {
    }

    void C6GERender::CreateCubePSO()
    {
        // Pipeline state object encompasses configuration of all GPU stages

        GraphicsPipelineStateCreateInfo PSOCreateInfo;

        // Pipeline state name is used by the engine to report issues.
        // It is always a good idea to give objects descriptive names.
        PSOCreateInfo.PSODesc.Name = "Cube PSO";

        // This is a graphics pipeline
        PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

        // clang-format off
    // This tutorial will render to a single render target
        PSOCreateInfo.GraphicsPipeline.NumRenderTargets             = 1;
        // Set render target format which is the format of the swap chain's color buffer
        PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                = m_pSwapChain->GetDesc().ColorBufferFormat;
        // Set depth buffer format which is the format of the swap chain's back buffer
        PSOCreateInfo.GraphicsPipeline.DSVFormat                    = m_pSwapChain->GetDesc().DepthBufferFormat;
    // Match pipeline sample count with current offscreen attachments (MSAA resolve happens later)
    PSOCreateInfo.GraphicsPipeline.SmplDesc.Count               = m_SampleCount;
        // Primitive topology defines what kind of primitives will be rendered by this pipeline state
        PSOCreateInfo.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        // Cull back faces
        PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_BACK;
        // Enable depth testing
        PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;
        // clang-format on

        // Create dynamic uniform buffer that will store shader constants
        CreateUniformBuffer(m_pDevice, sizeof(Constants), "Shader constants CB", &m_VSConstants);

        ShaderCreateInfo ShaderCI;
        // Tell the system that the shader source code is in HLSL.
        // For OpenGL, the engine will convert this into GLSL under the hood.
        ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

        // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
        ShaderCI.Desc.UseCombinedTextureSamplers = true;

        // Pack matrices in row-major order
        ShaderCI.CompileFlags = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

        // Presentation engine always expects input in gamma space. Normally, pixel shader output is
        // converted from linear to gamma space by the GPU. However, some platforms (e.g. Android in GLES mode,
        // or Emscripten in WebGL mode) do not support gamma-correction. In this case the application
        // has to do the conversion manually.
        ShaderMacro Macros[] = {{"CONVERT_PS_OUTPUT_TO_GAMMA", m_ConvertPSOutputToGamma ? "1" : "0"}};
    ShaderCI.Macros = {Macros, sizeof(Macros)/sizeof(Macros[0])};

        // Create a shader source stream factory to load shaders from files.
        RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
        m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

        // Create a vertex shader
        RefCntAutoPtr<IShader> pVS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
            ShaderCI.EntryPoint = "main";
            ShaderCI.Desc.Name = "Cube VS";
            ShaderCI.FilePath = "assets/cube.vsh";
            m_pDevice->CreateShader(ShaderCI, &pVS);
        }

        // Create a geometry shader only if the device supports geometry shaders.
        RefCntAutoPtr<IShader> pGS;
        if (m_pDevice->GetDeviceInfo().Features.GeometryShaders)
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_GEOMETRY;
            ShaderCI.EntryPoint = "main";
            ShaderCI.Desc.Name = "Cube GS";
            ShaderCI.FilePath = "assets/cube.gsh";
            m_pDevice->CreateShader(ShaderCI, &pGS);
        }

        // Create a pixel shader
        RefCntAutoPtr<IShader> pPS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
            ShaderCI.EntryPoint = "main";
            ShaderCI.Desc.Name = "Cube PS";
            ShaderCI.FilePath = "assets/cube.psh";
            m_pDevice->CreateShader(ShaderCI, &pPS);
        }

        // clang-format off
        // Define vertex shader input layout: match structures.fxh (ATTRIB0 Pos, ATTRIB1 Normal, ATTRIB2 UV)
        LayoutElement LayoutElems[] =
        {
            // Attribute 0 - vertex position
            LayoutElement{0, 0, 3, VT_FLOAT32, False},
            // Attribute 1 - vertex normal
            LayoutElement{1, 0, 3, VT_FLOAT32, False},
            // Attribute 2 - texture coordinates
            LayoutElement{2, 0, 2, VT_FLOAT32, False}
        };
        // clang-format on

        PSOCreateInfo.pVS = pVS;
        // Assign geometry shader only if it was created (device supports geometry shaders)
        if (pGS)
            PSOCreateInfo.pGS = pGS;
        PSOCreateInfo.pPS = pPS;

        PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = sizeof(LayoutElems)/sizeof(LayoutElems[0]);

        // Define variable type that will be used by default
        PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

        // Shader variables should typically be mutable, which means they are expected
        // to change on a per-instance basis
        // clang-format off
    ShaderResourceVariableDesc Vars[] = 
    {
        {SHADER_TYPE_PIXEL, "g_Texture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
    };
        // clang-format on
        PSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars;
    PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = sizeof(Vars)/sizeof(Vars[0]);

        // Define immutable sampler for g_Texture. Immutable samplers should be used whenever possible
        // clang-format off
    SamplerDesc SamLinearClampDesc
    {
        FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, 
        TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP
    };
    ImmutableSamplerDesc ImtblSamplers[] = 
    {
        {SHADER_TYPE_PIXEL, "g_Texture", SamLinearClampDesc}
    };
        // clang-format on
        PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers = ImtblSamplers;
    PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = sizeof(ImtblSamplers)/sizeof(ImtblSamplers[0]);

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pCubePSO);
    VERIFY_EXPR(m_pCubePSO);

        // clang-format off
        // Since we did not explicitly specify the type for 'VSConstants', 'GSConstants', 
        // and 'PSConstants' variables, default type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used.
        // Static variables never change and are bound directly to the pipeline state object.
        auto* pVarVS = m_pCubePSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "VSConstants");
        if (!pVarVS)
            pVarVS = m_pCubePSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants");
        if (pVarVS)
        {
            pVarVS->Set(m_VSConstants);
            std::cout << "[C6GE] Bound VS static variable for Constants" << std::endl;
        }
        else
        {
            std::cout << "[C6GE] VS static variable for Constants not found in PSO" << std::endl;
        }

        // Set GS constants only if geometry shader is supported and the PSO exposes the variable
        if (m_pDevice->GetDeviceInfo().Features.GeometryShaders)
        {
            auto* pVarGS = m_pCubePSO->GetStaticVariableByName(SHADER_TYPE_GEOMETRY, "GSConstants");
            if (!pVarGS)
                pVarGS = m_pCubePSO->GetStaticVariableByName(SHADER_TYPE_GEOMETRY, "Constants");
            if (pVarGS)
            {
                pVarGS->Set(m_VSConstants);
                std::cout << "[C6GE] Bound GS static variable for Constants" << std::endl;
            }
            else
            {
                std::cout << "[C6GE] GS static variable for Constants not found in PSO" << std::endl;
            }
        }

        auto* pVarPS = m_pCubePSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "PSConstants");
        if (!pVarPS)
            pVarPS = m_pCubePSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "Constants");
        if (pVarPS)
        {
            pVarPS->Set(m_VSConstants);
            std::cout << "[C6GE] Bound PS static variable for Constants" << std::endl;
        }
        else
        {
            std::cout << "[C6GE] PS static variable for Constants not found in PSO" << std::endl;
        }
        // clang-format on

        // Since we are using mutable variable, we must create a shader resource binding object
        // http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/
    m_pCubePSO->CreateShaderResourceBinding(&m_CubeSRB, true);

    // Create shadow pass PSO (depth-only)
    {
        GraphicsPipelineStateCreateInfo ShadowPSOCI;
        ShadowPSOCI.PSODesc.Name = "Cube shadow PSO";
        ShadowPSOCI.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
        ShadowPSOCI.GraphicsPipeline.NumRenderTargets = 0;
        ShadowPSOCI.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_UNKNOWN;
        ShadowPSOCI.GraphicsPipeline.DSVFormat = m_ShadowMapFormat;
        ShadowPSOCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        ShadowPSOCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
        ShadowPSOCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;

        ShaderCreateInfo ShadowShaderCI;
        ShadowShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
        ShadowShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        ShadowShaderCI.Desc.UseCombinedTextureSamplers = true;
        ShadowShaderCI.CompileFlags = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

        RefCntAutoPtr<IShader> pShadowVS;
        ShadowShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShadowShaderCI.EntryPoint = "main";
        ShadowShaderCI.Desc.Name = "Cube Shadow VS";
    ShadowShaderCI.FilePath = "assets/cube_shadow.vsh";
        m_pDevice->CreateShader(ShadowShaderCI, &pShadowVS);

        ShadowPSOCI.pVS = pShadowVS;
        ShadowPSOCI.pPS = nullptr;

        LayoutElement ShadowLayoutElems[] =
        {
            // Match structures.fxh: ATTRIB0 = Pos (float3), ATTRIB1 = Normal (float3), ATTRIB2 = UV (float2)
            LayoutElement{0, 0, 3, VT_FLOAT32, False}, // Position
            LayoutElement{1, 0, 3, VT_FLOAT32, False}, // Normal
            LayoutElement{2, 0, 2, VT_FLOAT32, False}  // UV
        };
        ShadowPSOCI.GraphicsPipeline.InputLayout.LayoutElements = ShadowLayoutElems;
    ShadowPSOCI.GraphicsPipeline.InputLayout.NumElements = sizeof(ShadowLayoutElems)/sizeof(ShadowLayoutElems[0]);

        ShadowPSOCI.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

        if (m_pDevice->GetDeviceInfo().Features.DepthClamp)
            ShadowPSOCI.GraphicsPipeline.RasterizerDesc.DepthClipEnable = False;

        // Add slope-scaled depth bias to reduce shadow acne. These values
        // are conservative; adjust if you see peter-panning or acne.
        ShadowPSOCI.GraphicsPipeline.RasterizerDesc.DepthBias = 0;
        ShadowPSOCI.GraphicsPipeline.RasterizerDesc.DepthBiasClamp = 0.0f;
        ShadowPSOCI.GraphicsPipeline.RasterizerDesc.SlopeScaledDepthBias = 2.0f;

        m_pDevice->CreateGraphicsPipelineState(ShadowPSOCI, &m_pCubeShadowPSO);
        if (m_pCubeShadowPSO)
        {
            m_pCubeShadowPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);
            m_pCubeShadowPSO->CreateShaderResourceBinding(&m_CubeShadowSRB, true);
        }
    }

    // Fallback: also bind constants through the SRB in case the PSO variables are mutable and static binding did not take effect.
        if (m_CubeSRB && m_VSConstants)
    {
        // Vertex stage
        IShaderResourceVariable* pVar = m_CubeSRB->GetVariableByName(SHADER_TYPE_VERTEX, "Constants");
        if (!pVar)
            pVar = m_CubeSRB->GetVariableByName(SHADER_TYPE_VERTEX, "VSConstants");
        if (pVar)
        {
            pVar->Set(m_VSConstants);
            std::cout << "[C6GE] Bound VS variable in SRB for Constants" << std::endl;
        }
        else
        {
            std::cout << "[C6GE] VS variable for Constants not found in SRB" << std::endl;
        }

        // Geometry stage
        if (m_pDevice->GetDeviceInfo().Features.GeometryShaders)
        {
            IShaderResourceVariable* pVarG = m_CubeSRB->GetVariableByName(SHADER_TYPE_GEOMETRY, "Constants");
            if (!pVarG)
                pVarG = m_CubeSRB->GetVariableByName(SHADER_TYPE_GEOMETRY, "GSConstants");
            if (pVarG)
            {
                pVarG->Set(m_VSConstants);
                std::cout << "[C6GE] Bound GS variable in SRB for Constants" << std::endl;
            }
            else
            {
                std::cout << "[C6GE] GS variable for Constants not found in SRB" << std::endl;
            }
        }

        // Pixel stage
        IShaderResourceVariable* pVarP = m_CubeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "Constants");
        if (!pVarP)
            pVarP = m_CubeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "PSConstants");
        if (pVarP)
        {
            pVarP->Set(m_VSConstants);
            std::cout << "[C6GE] Bound PS variable in SRB for Constants" << std::endl;
        }
        else
        {
            std::cout << "[C6GE] PS variable for Constants not found in SRB" << std::endl;
        }
    }
    else if (!m_VSConstants)
    {
        std::cerr << "[C6GE] Warning: Cannot bind constants - uniform buffer is null." << std::endl;
    }
    else
    {
        std::cerr << "[C6GE] Warning: Cube SRB is null; cannot bind constants via SRB fallback." << std::endl;
    }
    }

    void C6GERender::CreateFramebuffer()
    {

        // Create render target texture
        TextureDesc RTTexDesc;
        RTTexDesc.Name = "Framebuffer render target";
        RTTexDesc.Type = RESOURCE_DIM_TEX_2D;
        RTTexDesc.Width = std::max(1u, m_FramebufferWidth);
        RTTexDesc.Height = std::max(1u, m_FramebufferHeight);
        RTTexDesc.Format = m_pSwapChain->GetDesc().ColorBufferFormat; // Match swap chain format
        RTTexDesc.Usage = USAGE_DEFAULT;
        RTTexDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
        RTTexDesc.MipLevels = 1;
        RTTexDesc.SampleCount = 1;
        RTTexDesc.ArraySize = 1;

        m_pDevice->CreateTexture(RTTexDesc, nullptr, &m_pFramebufferTexture);
        if (m_pFramebufferTexture)
        {
            const auto &desc = m_pFramebufferTexture->GetDesc();
        }
        else
        {
        }

        // Create render target view
        TextureViewDesc RTVDesc;
        RTVDesc.ViewType = TEXTURE_VIEW_RENDER_TARGET;
        RTVDesc.Format = RTTexDesc.Format;
        if (m_pFramebufferTexture)
        {
            m_pFramebufferTexture->CreateView(RTVDesc, &m_pFramebufferRTV);
        }
        else
        {
            m_pFramebufferRTV.Release();
        }

        // Create shader resource view
        TextureViewDesc SRVDesc;
        SRVDesc.ViewType = TEXTURE_VIEW_SHADER_RESOURCE;
        SRVDesc.Format = RTTexDesc.Format;
        m_pFramebufferTexture->CreateView(SRVDesc, &m_pFramebufferSRV);
    // Default viewport SRV shows the raw framebuffer until post/TAA updates it
    m_pViewportDisplaySRV = m_pFramebufferSRV;

        // Create depth buffer texture
        TextureDesc DepthTexDesc;
        DepthTexDesc.Name = "Framebuffer depth buffer";
        DepthTexDesc.Type = RESOURCE_DIM_TEX_2D;
        DepthTexDesc.Width = std::max(1u, m_FramebufferWidth);
        DepthTexDesc.Height = std::max(1u, m_FramebufferHeight);
        DepthTexDesc.Format = m_pSwapChain->GetDesc().DepthBufferFormat; // Match swap chain depth format
        DepthTexDesc.Usage = USAGE_DEFAULT;
        DepthTexDesc.BindFlags = BIND_DEPTH_STENCIL;
        DepthTexDesc.MipLevels = 1;
        DepthTexDesc.SampleCount = 1;
        DepthTexDesc.ArraySize = 1;

        m_pDevice->CreateTexture(DepthTexDesc, nullptr, &m_pFramebufferDepth);
        if (m_pFramebufferDepth)
        {
            const auto &desc = m_pFramebufferDepth->GetDesc();
        }
        else
        {
        }

        // Create depth-stencil view
        TextureViewDesc DSVDesc;
        DSVDesc.ViewType = TEXTURE_VIEW_DEPTH_STENCIL;
        DSVDesc.Format = DepthTexDesc.Format;
        if (m_pFramebufferDepth)
        {
            m_pFramebufferDepth->CreateView(DSVDesc, &m_pFramebufferDSV);
        }
        else
        {
            m_pFramebufferDSV.Release();
        }
    }

    void C6GERender::CreatePlanePSO()
    {
        GraphicsPipelineStateCreateInfo PSOCreateInfo;

        PSOCreateInfo.PSODesc.Name = "Plane PSO";
        PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

        PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
        PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
        PSOCreateInfo.GraphicsPipeline.DSVFormat = m_pSwapChain->GetDesc().DepthBufferFormat;
        PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
        PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;

        ShaderCreateInfo ShaderCI;
        ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        ShaderCI.Desc.UseCombinedTextureSamplers = true;
        ShaderCI.CompileFlags = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

        ShaderMacro Macros[] = { {"CONVERT_PS_OUTPUT_TO_GAMMA", m_ConvertPSOutputToGamma ? "1" : "0"} };
    ShaderCI.Macros = {Macros, sizeof(Macros)/sizeof(Macros[0])};

        RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
        m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

        // Create plane vertex shader
        RefCntAutoPtr<IShader> pPlaneVS;
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "Plane VS";
    ShaderCI.FilePath = "assets/plane.vsh";
        m_pDevice->CreateShader(ShaderCI, &pPlaneVS);

        // Create plane pixel shader
        RefCntAutoPtr<IShader> pPlanePS;
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "Plane PS";
    ShaderCI.FilePath = "assets/plane.psh";
        m_pDevice->CreateShader(ShaderCI, &pPlanePS);

    PSOCreateInfo.pVS = pPlaneVS;
        PSOCreateInfo.pPS = pPlanePS;

        PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

        ShaderResourceVariableDesc Vars[] =
        {
            {SHADER_TYPE_PIXEL, "g_ShadowMap", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
        };
        PSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars;
    PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = sizeof(Vars)/sizeof(Vars[0]);

        SamplerDesc ComparisonSampler;
        ComparisonSampler.ComparisonFunc = COMPARISON_FUNC_LESS;
        ComparisonSampler.MinFilter = FILTER_TYPE_COMPARISON_LINEAR;
        ComparisonSampler.MagFilter = FILTER_TYPE_COMPARISON_LINEAR;
        ComparisonSampler.MipFilter = FILTER_TYPE_COMPARISON_LINEAR;

        ImmutableSamplerDesc ImtblSamplers[] =
        {
            {SHADER_TYPE_PIXEL, "g_ShadowMap", ComparisonSampler}
        };

        if (m_pDevice->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_GLES)
        {
            PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers = ImtblSamplers;
            PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = sizeof(ImtblSamplers)/sizeof(ImtblSamplers[0]);
        }

    // Match pipeline sample count with offscreen attachments
    PSOCreateInfo.GraphicsPipeline.SmplDesc.Count = m_SampleCount;
    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPlanePSO);

        if (!m_pPlanePSO)
        {
            std::cerr << "[C6GE] Error: Failed to create Plane PSO." << std::endl;
        }
        else
        {
            // Bind static constants if present
            auto* pVar = m_pPlanePSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants");
            if (pVar)
                pVar->Set(m_VSConstants);

            // Create a shader resource binding right away so the PSO always has
            // an SRB available even before the shadow map SRV is created.
            m_PlaneSRB.Release();
            m_pPlanePSO->CreateShaderResourceBinding(&m_PlaneSRB, true);
            if (m_PlaneSRB)
            {
                // If shadow SRV already exists, bind it; otherwise it will be
                // bound later in CreateShadowMap().
                if (m_ShadowMapSRV)
                {
                    auto* var = m_PlaneSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_ShadowMap");
                    if (var)
                        var->Set(m_ShadowMapSRV);
                }
                std::cout << "[C6GE] Created fallback Plane SRB at PSO creation time." << std::endl;
            }
            else
            {
                std::cerr << "[C6GE] Warning: Failed to create fallback Plane SRB at PSO creation time." << std::endl;
            }
        }

        // Also create a no-shadow variant that uses a simpler pixel shader
        // (plane_no_shadow.psh) and does not expose g_ShadowMap variable.
        // Reuse same VS and most of the PSO settings.
        GraphicsPipelineStateCreateInfo NoShadowPSOCI = PSOCreateInfo;
        RefCntAutoPtr<IShader> pNoShadowPS;
        ShaderCreateInfo NoShadowCI = ShaderCI;
        NoShadowCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        NoShadowCI.EntryPoint = "main";
        NoShadowCI.Desc.Name = "Plane NoShadow PS";
        NoShadowCI.FilePath = "assets/plane_no_shadow.psh";
        m_pDevice->CreateShader(NoShadowCI, &pNoShadowPS);
    NoShadowPSOCI.pVS = pPlaneVS;
    NoShadowPSOCI.pPS = pNoShadowPS;
    NoShadowPSOCI.PSODesc.Name = "Plane NoShadow PSO";
        NoShadowPSOCI.GraphicsPipeline.SmplDesc.Count = m_SampleCount;
        // No shadow variable exposure in this PSO
        NoShadowPSOCI.PSODesc.ResourceLayout.NumVariables = 0;
        NoShadowPSOCI.PSODesc.ResourceLayout.Variables = nullptr;
        if (m_pDevice->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_GLES)
        {
            NoShadowPSOCI.PSODesc.ResourceLayout.NumImmutableSamplers = 0;
            NoShadowPSOCI.PSODesc.ResourceLayout.ImmutableSamplers = nullptr;
        }
        m_pDevice->CreateGraphicsPipelineState(NoShadowPSOCI, &m_pPlaneNoShadowPSO);
        if (!m_pPlaneNoShadowPSO)
        {
            std::cerr << "[C6GE] Error: Failed to create Plane No-Shadow PSO." << std::endl;
        }
        else
        {
            auto* pVarNS = m_pPlaneNoShadowPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants");
            if (pVarNS)
                pVarNS->Set(m_VSConstants);

            // Create an SRB for the no-shadow PSO as well to satisfy backends
            // that require an SRB to be bound for any pipeline.
            m_PlaneNoShadowSRB.Release();
            m_pPlaneNoShadowPSO->CreateShaderResourceBinding(&m_PlaneNoShadowSRB, true);
            if (m_PlaneNoShadowSRB)
            {
                std::cout << "[C6GE] Created Plane No-Shadow SRB at PSO creation time." << std::endl;
            }
            else
            {
                std::cerr << "[C6GE] Warning: Failed to create Plane No-Shadow SRB at PSO creation time." << std::endl;
            }
        }
    }

    void C6GERender::CreateGLTFShadowPSO()
    {
        if (m_pGLTFShadowPSO)
            return;

        GraphicsPipelineStateCreateInfo PSOCI;
        PSOCI.PSODesc.Name = "GLTF Shadow PSO";
        PSOCI.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    PSOCI.GraphicsPipeline.NumRenderTargets = 0;
    PSOCI.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_UNKNOWN;
    PSOCI.GraphicsPipeline.DSVFormat = m_ShadowMapFormat;
    PSOCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    // Disable culling in shadow pass to avoid missing geometry due to winding flips
    PSOCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
    PSOCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;

        ShaderCreateInfo CI;
        CI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        CI.Desc.UseCombinedTextureSamplers = true;
        CI.CompileFlags = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

        RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
        m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
        CI.pShaderSourceStreamFactory = pShaderSourceFactory;

        RefCntAutoPtr<IShader> pVS;
        CI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        CI.EntryPoint = "main";
        CI.Desc.Name = "GLTF Shadow VS";
    CI.FilePath = "assets/gltf_shadow.vsh";
        m_pDevice->CreateShader(CI, &pVS);

        PSOCI.pVS = pVS;
        PSOCI.pPS = nullptr; // depth-only

        // Position-only input, buffer slot 0 (matches GLTF default attributes)
        LayoutElement LayoutElems[] = {
            LayoutElement{0, 0, 3, VT_FLOAT32, False}
        };
        PSOCI.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
        PSOCI.GraphicsPipeline.InputLayout.NumElements = _countof(LayoutElems);

        PSOCI.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

        m_pDevice->CreateGraphicsPipelineState(PSOCI, &m_pGLTFShadowPSO);
        if (m_pGLTFShadowPSO)
        {
            // Try to bind the common constants buffer
            if (auto* var = m_pGLTFShadowPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants"))
                var->Set(m_VSConstants);
            m_pGLTFShadowPSO->CreateShaderResourceBinding(&m_GLTFShadowSRB, true);
        }
    }

    void C6GERender::CreateGLTFShadowSkinnedPSO()
    {
        if (m_pGLTFShadowSkinnedPSO)
            return;

        GraphicsPipelineStateCreateInfo PSOCI;
        PSOCI.PSODesc.Name = "GLTF Shadow Skinned PSO";
        PSOCI.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
        PSOCI.GraphicsPipeline.NumRenderTargets = 0;
        PSOCI.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_UNKNOWN;
        PSOCI.GraphicsPipeline.DSVFormat = m_ShadowMapFormat;
        PSOCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        PSOCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
        PSOCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;

        ShaderCreateInfo CI;
        CI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        CI.Desc.UseCombinedTextureSamplers = true;
        CI.CompileFlags = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

        RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
        m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
        CI.pShaderSourceStreamFactory = pShaderSourceFactory;

        RefCntAutoPtr<IShader> pVS;
        CI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        CI.EntryPoint = "main";
        CI.Desc.Name = "GLTF Shadow Skinned VS";
    CI.FilePath = "assets/gltf_shadow_skinned.vsh";
        m_pDevice->CreateShader(CI, &pVS);

        PSOCI.pVS = pVS;
        PSOCI.pPS = nullptr; // depth-only

        // Position + Joints/Weights (DefaultVertexAttributes place JOINTS/WEIGHTS in buffer slot 1)
        LayoutElement LayoutElems[] = {
            LayoutElement{0, 0, 3, VT_FLOAT32, False}, // POSITION -> ATTRIB0 from VB0
            LayoutElement{4, 1, 4, VT_FLOAT32, False}, // JOINTS_0 -> ATTRIB4 from VB1
            LayoutElement{5, 1, 4, VT_FLOAT32, False}  // WEIGHTS_0 -> ATTRIB5 from VB1
        };
        PSOCI.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
        PSOCI.GraphicsPipeline.InputLayout.NumElements    = _countof(LayoutElems);

        // Expose a mutable constant buffer for skin matrices and bind VS constants as static
        PSOCI.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
        ShaderResourceVariableDesc Vars[] = {
            {SHADER_TYPE_VERTEX, "ShadowSkin", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
        };
        PSOCI.PSODesc.ResourceLayout.Variables    = Vars;
        PSOCI.PSODesc.ResourceLayout.NumVariables = _countof(Vars);

        m_pDevice->CreateGraphicsPipelineState(PSOCI, &m_pGLTFShadowSkinnedPSO);
        if (m_pGLTFShadowSkinnedPSO)
        {
            if (auto* var = m_pGLTFShadowSkinnedPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants"))
                var->Set(m_VSConstants);
            m_pGLTFShadowSkinnedPSO->CreateShaderResourceBinding(&m_GLTFShadowSkinnedSRB, true);

            // Create skin joint matrices constant buffer matching the HLSL struct
            struct ShadowSkinCB
            {
                float4x4 JointMatrices[256];
                int      JointCount;
                float3   _pad0;
            };
            CreateUniformBuffer(m_pDevice, sizeof(ShadowSkinCB), "GLTF Shadow Skin CB", &m_GLTFShadowSkinCB);
            if (m_GLTFShadowSkinnedSRB && m_GLTFShadowSkinCB)
            {
                if (auto* v = m_GLTFShadowSkinnedSRB->GetVariableByName(SHADER_TYPE_VERTEX, "ShadowSkin"))
                    v->Set(m_GLTFShadowSkinCB);
            }
        }
    }

    // Shadow-map visualization has been removed. Visualization PSO creation was here previously.

    void C6GERender::CreateShadowMap()
    {
        TextureDesc SMDesc;
        SMDesc.Name = "Shadow map";
        SMDesc.Type = RESOURCE_DIM_TEX_2D;
        SMDesc.Width = m_ShadowMapSize;
        SMDesc.Height = m_ShadowMapSize;
        SMDesc.Format = m_ShadowMapFormat;
        SMDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_DEPTH_STENCIL;
        m_ShadowMapTex.Release();
        m_pDevice->CreateTexture(SMDesc, nullptr, &m_ShadowMapTex);
        if (m_ShadowMapTex)
        {
            m_ShadowMapSRV = m_ShadowMapTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
            m_ShadowMapDSV = m_ShadowMapTex->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
        }

        // Create SRBs that use shadow map as mutable variable
        m_PlaneSRB.Release();
        if (m_pPlanePSO)
            m_pPlanePSO->CreateShaderResourceBinding(&m_PlaneSRB, true);
        if (m_PlaneSRB)
            m_PlaneSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_ShadowMap")->Set(m_ShadowMapSRV);

        // Visualization SRB removed; only bind shadow map to plane SRB
    }

    void C6GERender::CreateMainRenderPass()
    {
        // Disable render passes by default for now - they need more careful integration
        // TODO: Properly handle resource state transitions and ensure compatibility
        m_UseRenderPasses = false;
        
        // Enable render passes for all backends (when enabled via UI)
        // Render passes provide benefits on all modern APIs:
        // - Vulkan/Metal: Native support, optimal performance
        // - D3D12: Optimizes resource barriers and transitions  
        // - D3D11/OpenGL: Abstraction layer, minimal overhead

        // Create a render pass with MSAA support
        // Attachment 0 - Color buffer (MSAA if enabled, or regular)
        // Attachment 1 - Depth buffer (MSAA if enabled, or regular)
        constexpr Uint32 NumAttachments = 2;

        RenderPassAttachmentDesc Attachments[NumAttachments];
        
        // Color attachment
        Attachments[0].Format       = m_pSwapChain->GetDesc().ColorBufferFormat;
        Attachments[0].SampleCount  = m_SampleCount;
        Attachments[0].InitialState = RESOURCE_STATE_RENDER_TARGET;
        Attachments[0].FinalState   = RESOURCE_STATE_RENDER_TARGET;
        Attachments[0].LoadOp       = ATTACHMENT_LOAD_OP_CLEAR;
        Attachments[0].StoreOp      = ATTACHMENT_STORE_OP_STORE;

        // Depth attachment
        Attachments[1].Format       = m_pSwapChain->GetDesc().DepthBufferFormat;
        Attachments[1].SampleCount  = m_SampleCount;
        Attachments[1].InitialState = RESOURCE_STATE_DEPTH_WRITE;
        Attachments[1].FinalState   = RESOURCE_STATE_DEPTH_WRITE;
        Attachments[1].LoadOp       = ATTACHMENT_LOAD_OP_CLEAR;
        Attachments[1].StoreOp      = ATTACHMENT_STORE_OP_DISCARD; // Don't need depth after rendering

        // Single subpass for now - render everything in one pass
        SubpassDesc Subpasses[1];

        AttachmentReference RTAttachmentRef = {0, RESOURCE_STATE_RENDER_TARGET};
        AttachmentReference DepthAttachmentRef = {1, RESOURCE_STATE_DEPTH_WRITE};

        Subpasses[0].RenderTargetAttachmentCount = 1;
        Subpasses[0].pRenderTargetAttachments    = &RTAttachmentRef;
        Subpasses[0].pDepthStencilAttachment     = &DepthAttachmentRef;

        RenderPassDesc RPDesc;
        RPDesc.Name            = "Main render pass";
        RPDesc.AttachmentCount = NumAttachments;
        RPDesc.pAttachments    = Attachments;
        RPDesc.SubpassCount    = 1;
        RPDesc.pSubpasses      = Subpasses;

        m_pDevice->CreateRenderPass(RPDesc, &m_pMainRenderPass);
        
        if (m_pMainRenderPass)
        {
            std::cout << "[C6GE] Created main render pass successfully" << std::endl;
        }
        else
        {
            std::cerr << "[C6GE] Failed to create main render pass" << std::endl;
            m_UseRenderPasses = false;
        }
    }

    RefCntAutoPtr<IFramebuffer> C6GERender::CreateMainFramebuffer()
    {
        if (!m_pMainRenderPass)
        {
            RefCntAutoPtr<IFramebuffer> pNull;
            return pNull;
        }

        ITextureView* pAttachments[2];
        
        if (m_SampleCount > 1 && m_pMSColorRTV && m_pMSDepthDSV)
        {
            pAttachments[0] = m_pMSColorRTV;
            pAttachments[1] = m_pMSDepthDSV;
        }
        else
        {
            pAttachments[0] = m_pFramebufferRTV;
            pAttachments[1] = m_pFramebufferDSV;
        }

        FramebufferDesc FBDesc;
        FBDesc.Name            = "Main framebuffer";
        FBDesc.pRenderPass     = m_pMainRenderPass;
        FBDesc.AttachmentCount = 2;
        FBDesc.ppAttachments   = pAttachments;

        RefCntAutoPtr<IFramebuffer> pFramebuffer;
        m_pDevice->CreateFramebuffer(FBDesc, &pFramebuffer);
        
        return pFramebuffer;
    }

    IFramebuffer* C6GERender::GetCurrentMainFramebuffer()
    {
        if (!m_UseRenderPasses || !m_pMainRenderPass)
            return nullptr;

        // Use framebuffer RTV as key (it's always the same for our off-screen buffer)
        ITextureView* pKey = m_pFramebufferRTV;

        auto fb_it = m_FramebufferCache.find(pKey);
        if (fb_it != m_FramebufferCache.end())
        {
            return fb_it->second;
        }
        else
        {
            auto pFramebuffer = CreateMainFramebuffer();
            if (pFramebuffer)
            {
                auto it = m_FramebufferCache.emplace(pKey, pFramebuffer);
                return it.first->second;
            }
            return nullptr;
        }
    }

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
            struct Constants
            {
                float4x4 WorldViewProj;
                float4x4 NormalTranform;
                float4   LightDirection;
            };
            MapHelper<Constants> CBConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
            CBConstants->WorldViewProj = (m_CubeWorldMatrix * CameraViewProj);
            // Compute normal transform as inverse-transpose of the world matrix
            // without translation (tutorial uses RemoveTranslation + Inverse + Transpose).
            float4x4 NormalMatrix = m_CubeWorldMatrix.RemoveTranslation().Inverse().Transpose();
            CBConstants->NormalTranform = NormalMatrix;
            CBConstants->LightDirection = m_LightDirection;
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
        struct Constants
        {
            float4x4 WorldViewProj;
            float4x4 NormalTranform;
            float4   LightDirection;
        };
        {
            MapHelper<Constants> CBConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
            CBConstants->WorldViewProj = (World * CameraViewProj);
            float4x4 NormalMatrix      = World.RemoveTranslation().Inverse().Transpose();
            CBConstants->NormalTranform = NormalMatrix;
            CBConstants->LightDirection = m_LightDirection;
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

    void C6GERender::ResizeFramebuffer(Uint32 Width, Uint32 Height)
    {

        // After CreateFramebuffer() succeeds
if (m_pFramebufferSRV)
{
    m_ViewportTextureID = reinterpret_cast<ImTextureID>(m_pFramebufferSRV.RawPtr());
}
else
{
    m_ViewportTextureID = 0;
}

        if (Width == m_FramebufferWidth && Height == m_FramebufferHeight)
            return; // No need to resize

        m_FramebufferWidth = Width;
        m_FramebufferHeight = Height;

        // Release old resources
        m_pFramebufferTexture.Release();
        m_pFramebufferRTV.Release();
        m_pFramebufferSRV.Release();
        m_pFramebufferDepth.Release();
        m_pFramebufferDSV.Release();
        
        // Release MSAA resources
        m_pMSColorRTV.Release();
        m_pMSDepthDSV.Release();
        
        // Clear framebuffer cache when resizing
        m_FramebufferCache.clear();

        // Recreate framebuffer with new size
        CreateFramebuffer();
        CreateMSAARenderTarget();

        // Recreate post-processing targets
        DestroyPostFXTargets();
        CreatePostFXTargets();
    }

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

        // 1) Render shadow map
        if (RenderShadows && m_ShadowMapDSV)
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
                RenderPlane(RESOURCE_STATE_TRANSITION_MODE_VERIFY);

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

                RenderCube(m_CameraViewProjMatrix, false, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                {
                    auto Saved = m_CubeWorldMatrix;
                    m_CubeWorldMatrix = m_SecondCubeWorldMatrix;
                    RenderCube(m_CameraViewProjMatrix, false, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                    m_CubeWorldMatrix = Saved;
                }
                RenderPlane(RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

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
            RenderPlane(RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

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

    // --- Ray Tracing (Hybrid) stubs ----------------------------------------------------------
    void C6GERender::InitializeRayTracing()
    {
        if (!m_RayTracingSupported)
        {
            std::cout << "[C6GE] InitializeRayTracing skipped: device does not support ray tracing." << std::endl;
            return;
        }
        if (m_RayTracingInitialized)
            return;

        // Placeholder: set up screen-sized output target first.
        if (m_FramebufferWidth > 0 && m_FramebufferHeight > 0)
        {
            CreateRayTracingOutputTexture(m_FramebufferWidth, m_FramebufferHeight);
        }
        else
        {
            std::cout << "[C6GE] InitializeRayTracing: framebuffer size is zero; output texture will be created on first resize." << std::endl;
        }

    // Create BLAS/TLAS and debug + shadow PSOs
    CreateRayTracingAS();
    CreateRTDebugPSOs();

        // Create RT constants buffer
        if (!m_RTConstants)
        {
            CreateUniformBuffer(m_pDevice, sizeof(RTConstantsCPU), "RT constants CB", &m_RTConstants);
        }

        // Future: set up BLAS/TLAS, RT PSO, and SBT following Tutorial22_HybridRendering.

    m_RayTracingInitialized = true;
    std::cout << "[C6GE] InitializeRayTracing: ready." << std::endl;
    }

    void C6GERender::DestroyRayTracing()
    {
        if (!m_RayTracingInitialized)
            return;

        // Release RT output and any future RT resources when implemented.
        m_pRTOutputTex.Release();
        m_pRTOutputUAV.Release();
        m_pRTOutputSRV.Release();

        // Release RT PSOs, SRBs, and constants to avoid overwrite asserts on re-init
    m_RTCompositeSRB.Release();
    m_pRTCompositePSO.Release();
    m_RTAddCompositeSRB.Release();
    m_pRTAddCompositePSO.Release();
        m_RTShadowSRB.Release();
        m_pRTShadowPSO.Release();
        m_RTConstants.Release();

        DestroyRayTracingAS();

        m_RayTracingInitialized = false;
        std::cout << "[C6GE] DestroyRayTracing: resources released (stub)." << std::endl;
    }

    void C6GERender::RenderRayTracingPath()
    {
        const Uint32 w = std::max<Uint32>(1, m_FramebufferWidth);
        const Uint32 h = std::max<Uint32>(1, m_FramebufferHeight);

        // Shadow ray query path
    if (m_pRTShadowPSO && m_RTShadowSRB && m_RTConstants && m_pRTOutputUAV && m_pTLAS)
        {
            // Update constants
            RTConstantsCPU c{};
            c.InvViewProj = m_CameraViewProjMatrix.Inverse();
            c.ViewSize_Plane = float4{static_cast<float>(w), static_cast<float>(h), -2.0f, 5.0f};
            c.LightDir_Shadow = float4{m_LightDirection.x, m_LightDirection.y, m_LightDirection.z, 0.6f};
            // Soft shadow params: angular radius (radians), sample count (from UI)
            float AngularRadius = m_SoftShadowsEnabled ? m_SoftShadowAngularRad : 0.0f;
            float SampleCount   = m_SoftShadowsEnabled ? static_cast<float>(std::max(1, m_SoftShadowSamples)) : 1.0f;
            c.ShadowSoftParams = float4{AngularRadius, SampleCount, 0.0f, 0.0f};
            {
                MapHelper<RTConstantsCPU> M(m_pImmediateContext, m_RTConstants, MAP_WRITE, MAP_FLAG_DISCARD);
                *M = c;
            }

            // Bind UAV and TLAS
            if (auto* uav = m_RTShadowSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_RTOutputUAV"))
                uav->Set(m_pRTOutputUAV, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
            if (auto* tlas = m_RTShadowSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_TLAS"))
                tlas->Set(m_pTLAS, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
            if (auto* cb = m_RTShadowSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "RTConstants"))
                cb->Set(m_RTConstants, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);

            m_pImmediateContext->SetPipelineState(m_pRTShadowPSO);
            m_pImmediateContext->CommitShaderResources(m_RTShadowSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            const Uint32 gx = (w + 7) / 8;
            const Uint32 gy = (h + 7) / 8;
            DispatchComputeAttribs DispatchAttrs{gx, gy, 1};
            m_pImmediateContext->DispatchCompute(DispatchAttrs);
        }
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
            // Provide at least one directional light so models aren't black without IBL.
            // Light data immediately follows PBRFrameAttribs in the buffer.
            {
                HLSL::PBRLightAttribs* Lights = reinterpret_cast<HLSL::PBRLightAttribs*>(Frame + 1);
                float3 Dir = normalize(m_LightDirection);
                GLTF::Light DefaultDirLight;
                DefaultDirLight.Type      = GLTF::Light::TYPE::DIRECTIONAL;
                DefaultDirLight.Color     = float3{1, 1, 1};
                DefaultDirLight.Intensity = 1.0f;
                GLTF_PBR_Renderer::WritePBRLightShaderAttribs({&DefaultDirLight, nullptr, &Dir, /*DistanceScale*/ 1.0f}, Lights);
                RendererParams.LightCount = 1;
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
            struct ShadowVSConstants
            {
                float4x4 g_WorldViewProj;
                float4x4 g_NormalTranform; // unused in shader; keep layout compatible if needed
                float4   g_LightDirection; // unused
            };
            {
                MapHelper<ShadowVSConstants> C(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
                C->g_WorldViewProj = WVP;
                C->g_NormalTranform = float4x4::Identity();
                C->g_LightDirection = m_LightDirection;
            }

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

    void C6GERender::CreateRayTracingOutputTexture(Uint32 Width, Uint32 Height)
    {
        // Create a UAV-capable texture for ray tracing output (RGBA16F)
        TextureDesc Desc;
        Desc.Name      = "RT Output";
        Desc.Type      = RESOURCE_DIM_TEX_2D;
        Desc.Width     = std::max<Uint32>(1, Width);
        Desc.Height    = std::max<Uint32>(1, Height);
        Desc.MipLevels = 1;
        Desc.Format    = TEX_FORMAT_RGBA16_FLOAT;
        Desc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;

        RefCntAutoPtr<ITexture> pTex;
        m_pDevice->CreateTexture(Desc, nullptr, &pTex);

        m_pRTOutputTex = pTex;
        m_pRTOutputUAV.Release();
        m_pRTOutputSRV.Release();
        if (m_pRTOutputTex)
        {
            // Default UAV/SRV are enough; customize if needed later
            m_pRTOutputUAV = m_pRTOutputTex->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS);
            m_pRTOutputSRV = m_pRTOutputTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        }
    }

    void C6GERender::CreateRTDebugPSOs()
    {

        // Graphics PSO to composite SRV multiplicatively (modulate by factor)
        {
            ShaderCreateInfo CI;
            CI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
            CI.Desc.UseCombinedTextureSamplers = true;
            CI.CompileFlags = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

            RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
            m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
            CI.pShaderSourceStreamFactory = pShaderSourceFactory;

            RefCntAutoPtr<IShader> pVS, pPS;
            {
                CI.Desc.ShaderType = SHADER_TYPE_VERTEX;
                CI.EntryPoint = "main";
                CI.Desc.Name = "RT Composite VS";
                CI.FilePath = "assets/rt_composite.vsh";
                m_pDevice->CreateShader(CI, &pVS);
            }
            {
                CI.Desc.ShaderType = SHADER_TYPE_PIXEL;
                CI.EntryPoint = "main";
                CI.Desc.Name = "RT Composite PS";
                CI.FilePath = "assets/rt_composite.psh";
                m_pDevice->CreateShader(CI, &pPS);
            }

            GraphicsPipelineStateCreateInfo PSOCI;
            PSOCI.PSODesc.Name = "RT Composite PSO";
            PSOCI.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
            PSOCI.pVS = pVS;
            PSOCI.pPS = pPS;
            PSOCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            PSOCI.GraphicsPipeline.NumRenderTargets = 1;
            PSOCI.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
            PSOCI.GraphicsPipeline.DSVFormat = m_pSwapChain->GetDesc().DepthBufferFormat;
            PSOCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;
            PSOCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = False;
            PSOCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
            // Match MSAA sample count with current offscreen render target
            PSOCI.GraphicsPipeline.SmplDesc.Count = m_SampleCount;

            auto& RT0 = PSOCI.GraphicsPipeline.BlendDesc.RenderTargets[0];
            RT0.BlendEnable = True;
            // Out = Src * Dest (modulation)
            RT0.SrcBlend = BLEND_FACTOR_DEST_COLOR;
            RT0.DestBlend = BLEND_FACTOR_ZERO;
            RT0.BlendOp = BLEND_OPERATION_ADD;
            RT0.SrcBlendAlpha = BLEND_FACTOR_ONE;
            RT0.DestBlendAlpha = BLEND_FACTOR_ZERO;
            RT0.BlendOpAlpha = BLEND_OPERATION_ADD;
            RT0.RenderTargetWriteMask = COLOR_MASK_ALL;

            PSOCI.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
            ShaderResourceVariableDesc Vars[] = {
                {SHADER_TYPE_PIXEL, "g_RTOutput", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
            };
            PSOCI.PSODesc.ResourceLayout.Variables = Vars;
            PSOCI.PSODesc.ResourceLayout.NumVariables = _countof(Vars);

            SamplerDesc Smpl;
            Smpl.MinFilter = FILTER_TYPE_LINEAR;
            Smpl.MagFilter = FILTER_TYPE_LINEAR;
            Smpl.MipFilter = FILTER_TYPE_LINEAR;
            Smpl.AddressU = TEXTURE_ADDRESS_CLAMP;
            Smpl.AddressV = TEXTURE_ADDRESS_CLAMP;
            Smpl.AddressW = TEXTURE_ADDRESS_CLAMP;
            ImmutableSamplerDesc Imtbl[] = {
                // With combined texture samplers enabled, immutable sampler should be bound to the texture name
                // and will be used as textureName_sampler in shaders
                {SHADER_TYPE_PIXEL, "g_RTOutput", Smpl}
            };
            PSOCI.PSODesc.ResourceLayout.ImmutableSamplers = Imtbl;
            PSOCI.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(Imtbl);

            m_pDevice->CreateGraphicsPipelineState(PSOCI, &m_pRTCompositePSO);
            if (m_pRTCompositePSO)
            {
                m_pRTCompositePSO->CreateShaderResourceBinding(&m_RTCompositeSRB, true);
                if (m_RTCompositeSRB && m_pRTOutputSRV)
                {
                    if (auto* var = m_RTCompositeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_RTOutput"))
                        var->Set(m_pRTOutputSRV);
                }
            }
        }

    // Compute PSO that uses ray queries to produce a shadow mask into UAV (and reflections in RGB)
        {
            ShaderCreateInfo CI;
            CI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
            CI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
            CI.EntryPoint = "main";
            CI.Desc.Name = "RT Shadow CS";
            CI.FilePath = "assets/rt_shadow.csh";
            // Ray queries require DXC and HLSL SM 6.5+
            CI.ShaderCompiler = SHADER_COMPILER_DXC;
            CI.HLSLVersion = {6, 5};
            CI.CompileFlags = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

            RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
            m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
            CI.pShaderSourceStreamFactory = pShaderSourceFactory;

            RefCntAutoPtr<IShader> pCS;
            m_pDevice->CreateShader(CI, &pCS);

            ComputePipelineStateCreateInfo PSOCI;
            PSOCI.PSODesc.Name = "RT Shadow PSO";
            PSOCI.pCS = pCS;
            ShaderResourceVariableDesc Vars[] = {
                {SHADER_TYPE_COMPUTE, "g_RTOutputUAV", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
                {SHADER_TYPE_COMPUTE, "g_TLAS",        SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
                {SHADER_TYPE_COMPUTE, "RTConstants",    SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
            };
            PSOCI.PSODesc.ResourceLayout.Variables = Vars;
            PSOCI.PSODesc.ResourceLayout.NumVariables = _countof(Vars);

            m_pDevice->CreateComputePipelineState(PSOCI, &m_pRTShadowPSO);
            if (m_pRTShadowPSO)
            {
                m_pRTShadowPSO->CreateShaderResourceBinding(&m_RTShadowSRB, true);
                if (m_RTShadowSRB)
                {
                    if (m_pRTOutputUAV)
                        if (auto* var = m_RTShadowSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_RTOutputUAV")) var->Set(m_pRTOutputUAV);
                    if (m_pTLAS)
                        if (auto* var = m_RTShadowSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_TLAS")) var->Set(m_pTLAS);
                    if (m_RTConstants)
                        if (auto* var = m_RTShadowSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "RTConstants")) var->Set(m_RTConstants);
                }
            }
        }

        // Graphics PSO to additively composite reflection color (RGB) over the scene
        {
            ShaderCreateInfo CI;
            CI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
            CI.Desc.UseCombinedTextureSamplers = true;
            CI.CompileFlags = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

            RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
            m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
            CI.pShaderSourceStreamFactory = pShaderSourceFactory;

            RefCntAutoPtr<IShader> pVS, pPS;
            {
                CI.Desc.ShaderType = SHADER_TYPE_VERTEX;
                CI.EntryPoint = "main";
                CI.Desc.Name = "RT Add Composite VS";
                CI.FilePath = "assets/rt_composite.vsh";
                m_pDevice->CreateShader(CI, &pVS);
            }
            {
                CI.Desc.ShaderType = SHADER_TYPE_PIXEL;
                CI.EntryPoint = "main";
                CI.Desc.Name = "RT Add Composite PS";
                CI.FilePath = "assets/rt_reflect_composite.psh";
                m_pDevice->CreateShader(CI, &pPS);
            }

            GraphicsPipelineStateCreateInfo PSOCI;
            PSOCI.PSODesc.Name = "RT Add Composite PSO";
            PSOCI.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
            PSOCI.pVS = pVS;
            PSOCI.pPS = pPS;
            PSOCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            PSOCI.GraphicsPipeline.NumRenderTargets = 1;
            PSOCI.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
            PSOCI.GraphicsPipeline.DSVFormat = m_pSwapChain->GetDesc().DepthBufferFormat;
            PSOCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;
            PSOCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = False;
            PSOCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
            PSOCI.GraphicsPipeline.SmplDesc.Count = m_SampleCount;

            auto& RT0 = PSOCI.GraphicsPipeline.BlendDesc.RenderTargets[0];
            RT0.BlendEnable = True;
            // Additive: Out = Src + Dest
            RT0.SrcBlend = BLEND_FACTOR_ONE;
            RT0.DestBlend = BLEND_FACTOR_ONE;
            RT0.BlendOp = BLEND_OPERATION_ADD;
            RT0.SrcBlendAlpha = BLEND_FACTOR_ONE;
            RT0.DestBlendAlpha = BLEND_FACTOR_ONE;
            RT0.BlendOpAlpha = BLEND_OPERATION_ADD;
            RT0.RenderTargetWriteMask = COLOR_MASK_ALL;

            PSOCI.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
            ShaderResourceVariableDesc Vars[] = {
                {SHADER_TYPE_PIXEL, "g_RTOutput", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
            };
            PSOCI.PSODesc.ResourceLayout.Variables = Vars;
            PSOCI.PSODesc.ResourceLayout.NumVariables = _countof(Vars);

            SamplerDesc Smpl;
            Smpl.MinFilter = FILTER_TYPE_LINEAR;
            Smpl.MagFilter = FILTER_TYPE_LINEAR;
            Smpl.MipFilter = FILTER_TYPE_LINEAR;
            Smpl.AddressU = TEXTURE_ADDRESS_CLAMP;
            Smpl.AddressV = TEXTURE_ADDRESS_CLAMP;
            Smpl.AddressW = TEXTURE_ADDRESS_CLAMP;
            ImmutableSamplerDesc Imtbl[] = {
                {SHADER_TYPE_PIXEL, "g_RTOutput", Smpl}
            };
            PSOCI.PSODesc.ResourceLayout.ImmutableSamplers = Imtbl;
            PSOCI.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(Imtbl);

            m_pDevice->CreateGraphicsPipelineState(PSOCI, &m_pRTAddCompositePSO);
            if (m_pRTAddCompositePSO)
            {
                m_pRTAddCompositePSO->CreateShaderResourceBinding(&m_RTAddCompositeSRB, true);
                if (m_RTAddCompositeSRB && m_pRTOutputSRV)
                {
                    if (auto* var = m_RTAddCompositeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_RTOutput"))
                        var->Set(m_pRTOutputSRV);
                }
            }
        }
    }

    void C6GERender::CreateRayTracingAS()
    {
        // Release any existing
        m_pBLAS_Cube.Release();
        m_pBLAS_Plane.Release();
        m_pTLAS.Release();
        m_pTLASInstances.Release();
        m_pTLASScratch.Release();
        m_CubeRTVertexBuffer.Release();
        m_CubeRTIndexBuffer.Release();
        m_PlaneRTVertexBuffer.Release();
        m_PlaneRTIndexBuffer.Release();

        if (!m_CubeVertexBuffer || !m_CubeIndexBuffer)
        {
            std::cerr << "[C6GE] CreateRayTracingAS: cube buffers are missing; skipping AS creation." << std::endl;
            return;
        }

        // --- Prepare RT-capable copies of cube VB/IB (required for BLAS input) ---
        {
            // Create RT vertex buffer copy
            const auto& SrcVBDesc = m_CubeVertexBuffer->GetDesc();
            BufferDesc   RTVBDesc;
            RTVBDesc.Name            = "Cube RT Vertex Buffer";
            RTVBDesc.Usage           = USAGE_DEFAULT;
            RTVBDesc.BindFlags       = BIND_VERTEX_BUFFER | BIND_RAY_TRACING;
            RTVBDesc.Size            = SrcVBDesc.Size;
            RTVBDesc.ElementByteStride = SrcVBDesc.ElementByteStride != 0 ? SrcVBDesc.ElementByteStride : 32u;
            m_pDevice->CreateBuffer(RTVBDesc, nullptr, &m_CubeRTVertexBuffer);

            // Create RT index buffer copy
            const auto& SrcIBDesc = m_CubeIndexBuffer->GetDesc();
            BufferDesc   RTIBDesc;
            RTIBDesc.Name            = "Cube RT Index Buffer";
            RTIBDesc.Usage           = USAGE_DEFAULT;
            RTIBDesc.BindFlags       = BIND_INDEX_BUFFER | BIND_RAY_TRACING;
            RTIBDesc.Size            = SrcIBDesc.Size;
            RTIBDesc.ElementByteStride = SrcIBDesc.ElementByteStride != 0 ? SrcIBDesc.ElementByteStride : 4u;
            m_pDevice->CreateBuffer(RTIBDesc, nullptr, &m_CubeRTIndexBuffer);

            // Copy contents from raster VB/IB into RT copies
            if (m_CubeRTVertexBuffer)
            {
                m_pImmediateContext->CopyBuffer(m_CubeVertexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                                m_CubeRTVertexBuffer, 0, RTVBDesc.Size,
                                                RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            }
            if (m_CubeRTIndexBuffer)
            {
                m_pImmediateContext->CopyBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                                m_CubeRTIndexBuffer, 0, RTIBDesc.Size,
                                                RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            }
        }

        // --- Create BLAS for the cube using RT-capable buffers ---
        Uint32       VertexStride = m_CubeRTVertexBuffer && m_CubeRTVertexBuffer->GetDesc().ElementByteStride != 0 ? m_CubeRTVertexBuffer->GetDesc().ElementByteStride : 32u; // pos(3f)+normal(3f)+uv(2f)
        const Uint64 VBSize       = m_CubeRTVertexBuffer ? m_CubeRTVertexBuffer->GetDesc().Size : 0ull;
        Uint32       NumVertices  = VertexStride != 0 ? static_cast<Uint32>(VBSize / VertexStride) : 0u;

        Uint32       IndexStride  = m_CubeRTIndexBuffer && m_CubeRTIndexBuffer->GetDesc().ElementByteStride != 0 ? m_CubeRTIndexBuffer->GetDesc().ElementByteStride : 4u;
        const Uint64 IBSize       = m_CubeRTIndexBuffer ? m_CubeRTIndexBuffer->GetDesc().Size : 0ull;
        Uint32       NumIndices   = IndexStride != 0 ? static_cast<Uint32>(IBSize / IndexStride) : 0u;
        if (NumIndices == 0 || NumVertices == 0)
        {
            std::cerr << "[C6GE] CreateRayTracingAS: invalid cube counts (V=" << NumVertices << ", I=" << NumIndices << ")." << std::endl;
            return;
        }

        BLASTriangleDesc Triangles{};
        Triangles.GeometryName         = "Cube";
        Triangles.MaxVertexCount       = NumVertices;
        Triangles.VertexValueType      = VT_FLOAT32;
        Triangles.VertexComponentCount = 3; // position only
        Triangles.MaxPrimitiveCount    = NumIndices / 3;
        Triangles.IndexType            = VT_UINT32;

        {
            BottomLevelASDesc ASDesc;
            ASDesc.Name          = "Cube BLAS";
            ASDesc.Flags         = RAYTRACING_BUILD_AS_PREFER_FAST_TRACE;
            ASDesc.pTriangles    = &Triangles;
            ASDesc.TriangleCount = 1;
            m_pDevice->CreateBLAS(ASDesc, &m_pBLAS_Cube);
        }

        if (!m_pBLAS_Cube)
        {
            std::cerr << "[C6GE] CreateRayTracingAS: failed to create cube BLAS." << std::endl;
            return;
        }

        // Scratch buffer for BLAS build
        {
            BufferDesc BuffDesc;
            BuffDesc.Name      = "BLAS Scratch Buffer";
            BuffDesc.Usage     = USAGE_DEFAULT;
            BuffDesc.BindFlags = BIND_RAY_TRACING;
            BuffDesc.Size      = m_pBLAS_Cube->GetScratchBufferSizes().Build;
            m_pTLASScratch = nullptr; // reuse field for temporary scratch if needed later
            RefCntAutoPtr<IBuffer> pScratch;
            m_pDevice->CreateBuffer(BuffDesc, nullptr, &pScratch);

            BLASBuildTriangleData TriangleData{};
            TriangleData.GeometryName         = Triangles.GeometryName;
            TriangleData.pVertexBuffer        = m_CubeRTVertexBuffer;
            TriangleData.VertexStride         = VertexStride;
            TriangleData.VertexOffset         = 0;
            TriangleData.VertexCount          = NumVertices;
            TriangleData.VertexValueType      = Triangles.VertexValueType;
            TriangleData.VertexComponentCount = Triangles.VertexComponentCount;
            TriangleData.pIndexBuffer         = m_CubeRTIndexBuffer;
            TriangleData.IndexOffset          = 0;
            TriangleData.PrimitiveCount       = Triangles.MaxPrimitiveCount;
            TriangleData.IndexType            = Triangles.IndexType;
            TriangleData.Flags                = RAYTRACING_GEOMETRY_FLAG_OPAQUE;

            BuildBLASAttribs Attribs{};
            Attribs.pBLAS                         = m_pBLAS_Cube;
            Attribs.pTriangleData                 = &TriangleData;
            Attribs.TriangleDataCount             = 1;
            Attribs.pScratchBuffer                = pScratch;
            Attribs.BLASTransitionMode            = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
            Attribs.GeometryTransitionMode        = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
            Attribs.ScratchBufferTransitionMode   = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

            m_pImmediateContext->BuildBLAS(Attribs);
        }

        // --- Create BLAS for the procedural plane (quad) ---
        {
            // Create a tiny static plane mesh: y = -2, extent = 5
            struct Pos3 { float x, y, z; };
            const float PlaneY = -2.0f;
            const float Ext    =  5.0f;
            const Pos3 PlaneVerts[4] = {
                {-Ext, PlaneY, -Ext},
                {-Ext, PlaneY,  Ext},
                { Ext, PlaneY, -Ext},
                { Ext, PlaneY,  Ext}
            };
            const Uint32 PlaneIndices[6] = { 0,1,2, 2,1,3 };

            // Create RT-capable VB/IB with initial data
            {
                BufferDesc VB{};
                VB.Name = "Plane RT Vertex Buffer";
                VB.Usage = USAGE_IMMUTABLE;
                VB.BindFlags = BIND_VERTEX_BUFFER | BIND_RAY_TRACING;
                VB.Size = sizeof(PlaneVerts);
                BufferData VBData{PlaneVerts, VB.Size};
                m_pDevice->CreateBuffer(VB, &VBData, &m_PlaneRTVertexBuffer);

                BufferDesc IB{};
                IB.Name = "Plane RT Index Buffer";
                IB.Usage = USAGE_IMMUTABLE;
                IB.BindFlags = BIND_INDEX_BUFFER | BIND_RAY_TRACING;
                IB.Size = sizeof(PlaneIndices);
                BufferData IBData{PlaneIndices, IB.Size};
                m_pDevice->CreateBuffer(IB, &IBData, &m_PlaneRTIndexBuffer);
            }

            BLASTriangleDesc Triangles{};
            Triangles.GeometryName         = "Plane";
            Triangles.MaxVertexCount       = 4;
            Triangles.VertexValueType      = VT_FLOAT32;
            Triangles.VertexComponentCount = 3; // position only
            Triangles.MaxPrimitiveCount    = 2;
            Triangles.IndexType            = VT_UINT32;

            BottomLevelASDesc ASDesc;
            ASDesc.Name          = "Plane BLAS";
            ASDesc.Flags         = RAYTRACING_BUILD_AS_PREFER_FAST_TRACE;
            ASDesc.pTriangles    = &Triangles;
            ASDesc.TriangleCount = 1;
            m_pDevice->CreateBLAS(ASDesc, &m_pBLAS_Plane);

            if (m_pBLAS_Plane)
            {
                // Build plane BLAS
                BufferDesc BuffDesc;
                BuffDesc.Name      = "Plane BLAS Scratch";
                BuffDesc.Usage     = USAGE_DEFAULT;
                BuffDesc.BindFlags = BIND_RAY_TRACING;
                BuffDesc.Size      = m_pBLAS_Plane->GetScratchBufferSizes().Build;
                RefCntAutoPtr<IBuffer> pScratch;
                m_pDevice->CreateBuffer(BuffDesc, nullptr, &pScratch);

                const Uint32 VertexStride = sizeof(Pos3);
                BLASBuildTriangleData TriangleData{};
                TriangleData.GeometryName         = Triangles.GeometryName;
                TriangleData.pVertexBuffer        = m_PlaneRTVertexBuffer;
                TriangleData.VertexStride         = VertexStride;
                TriangleData.VertexOffset         = 0;
                TriangleData.VertexCount          = 4;
                TriangleData.VertexValueType      = Triangles.VertexValueType;
                TriangleData.VertexComponentCount = Triangles.VertexComponentCount;
                TriangleData.pIndexBuffer         = m_PlaneRTIndexBuffer;
                TriangleData.IndexOffset          = 0;
                TriangleData.PrimitiveCount       = 2;
                TriangleData.IndexType            = Triangles.IndexType;
                TriangleData.Flags                = RAYTRACING_GEOMETRY_FLAG_OPAQUE;

                BuildBLASAttribs Attribs{};
                Attribs.pBLAS                       = m_pBLAS_Plane;
                Attribs.pTriangleData               = &TriangleData;
                Attribs.TriangleDataCount           = 1;
                Attribs.pScratchBuffer              = pScratch;
                Attribs.BLASTransitionMode          = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
                Attribs.GeometryTransitionMode      = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
                Attribs.ScratchBufferTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

                m_pImmediateContext->BuildBLAS(Attribs);
            }
            else
            {
                std::cerr << "[C6GE] CreateRayTracingAS: failed to create plane BLAS." << std::endl;
            }
        }

        // --- Create TLAS with capacity for many instances (plane + ECS meshes) ---
        {
            TopLevelASDesc TLASDesc;
            TLASDesc.Name             = "Scene TLAS";
            TLASDesc.MaxInstanceCount = std::max<Uint32>(m_MaxTLASInstances, 1u); // at least 1 (plane)
            TLASDesc.Flags            = RAYTRACING_BUILD_AS_ALLOW_UPDATE | RAYTRACING_BUILD_AS_PREFER_FAST_TRACE;
            m_pDevice->CreateTLAS(TLASDesc, &m_pTLAS);
        }

        if (!m_pTLAS)
        {
            std::cerr << "[C6GE] CreateRayTracingAS: failed to create TLAS." << std::endl;
            return;
        }

        // Create scratch and instance buffers for TLAS build
        if (!m_pTLASScratch)
        {
            BufferDesc BuffDesc;
            BuffDesc.Name      = "TLAS Scratch Buffer";
            BuffDesc.Usage     = USAGE_DEFAULT;
            BuffDesc.BindFlags = BIND_RAY_TRACING;
            BuffDesc.Size      = std::max(m_pTLAS->GetScratchBufferSizes().Build, m_pTLAS->GetScratchBufferSizes().Update);
            m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_pTLASScratch);
        }

        if (!m_pTLASInstances)
        {
            BufferDesc BuffDesc;
            BuffDesc.Name      = "TLAS Instance Buffer";
            BuffDesc.Usage     = USAGE_DEFAULT;
            BuffDesc.BindFlags = BIND_RAY_TRACING;
            BuffDesc.Size      = Uint64{TLAS_INSTANCE_DATA_SIZE} * Uint64{std::max<Uint32>(m_MaxTLASInstances, 1u)};
            m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_pTLASInstances);
        }

    // Build initial instances from ECS: plane + all cube meshes + glTF meshes
        std::vector<TLASBuildInstanceData> Instances;
        std::vector<std::string>           InstanceNames; // keep storage for names while building
        Instances.reserve(16);
        InstanceNames.reserve(16);
        // Plane (static)
        {
            TLASBuildInstanceData inst{};
            InstanceNames.emplace_back("Plane");
            inst.InstanceName = InstanceNames.back().c_str();
            inst.pBLAS        = m_pBLAS_Plane;
            inst.Mask         = 0xFF;
            inst.CustomId     = 0;
            float4x4 I = float4x4::Identity();
            inst.Transform.SetRotation(I.Data(), 4);
            inst.Transform.SetTranslation(0.f, 0.f, 0.f);
            Instances.push_back(inst);
        }
        // ECS cubes and glTF
        if (m_World)
        {
            auto& reg = m_World->Registry();
            auto view = reg.view<ECS::Transform, ECS::StaticMesh>();
            Uint32 cid = 1;
            for (auto e : view)
            {
                const auto& sm = view.get<ECS::StaticMesh>(e);
                if (sm.type != ECS::StaticMesh::MeshType::Cube)
                    continue;
                const auto& tr = view.get<ECS::Transform>(e);
                const float4x4 W = tr.WorldMatrix();
                TLASBuildInstanceData inst{};
                InstanceNames.emplace_back(std::string("Cube_") + std::to_string(static_cast<Uint32>(e)));
                inst.InstanceName = InstanceNames.back().c_str();
                inst.pBLAS        = m_pBLAS_Cube;
                inst.Mask         = 0xFF;
                inst.CustomId     = cid;
                inst.Transform.SetRotation(W.Data(), 4);
                inst.Transform.SetTranslation(W.m30, W.m31, W.m32);
                Instances.push_back(inst);
                ++cid;
                if (cid >= m_MaxTLASInstances)
                    break; // respect capacity hint
            }

            // glTF dynamic meshes
            auto viewGltf = reg.view<ECS::Transform, ECS::Mesh>();
            for (auto e : viewGltf)
            {
                const auto& m = viewGltf.get<ECS::Mesh>(e);
                if (m.kind != ECS::Mesh::Kind::Dynamic || m.assetId.empty())
                    continue;
                EnsureGLTFBLAS(m.assetId);
                auto itAsset = m_GltfAssets.find(m.assetId);
                if (itAsset == m_GltfAssets.end())
                    continue;
                if (!itAsset->second.BLAS)
                    continue;
                const auto& tr = viewGltf.get<ECS::Transform>(e);
                const float4x4 W = tr.WorldMatrix();
                TLASBuildInstanceData inst{};
                InstanceNames.emplace_back(std::string("GLTF_") + std::to_string(static_cast<Uint32>(e)));
                inst.InstanceName = InstanceNames.back().c_str();
                inst.pBLAS        = itAsset->second.BLAS;
                inst.Mask         = 0xFF;
                inst.CustomId     = cid;
                inst.Transform.SetRotation(W.Data(), 4);
                inst.Transform.SetTranslation(W.m30, W.m31, W.m32);
                Instances.push_back(inst);
                ++cid;
                if (cid >= m_MaxTLASInstances)
                    break;
            }
        }

        BuildTLASAttribs TLASAttribs{};
        TLASAttribs.pTLAS                         = m_pTLAS;
        TLASAttribs.Update                        = false; // first build
        TLASAttribs.pScratchBuffer                = m_pTLASScratch;
        TLASAttribs.pInstanceBuffer               = m_pTLASInstances;
        TLASAttribs.pInstances                    = Instances.empty() ? nullptr : Instances.data();
        TLASAttribs.InstanceCount                 = static_cast<Uint32>(Instances.size());
        TLASAttribs.TLASTransitionMode            = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        TLASAttribs.BLASTransitionMode            = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        TLASAttribs.InstanceBufferTransitionMode  = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        TLASAttribs.ScratchBufferTransitionMode   = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

        m_pImmediateContext->BuildTLAS(TLASAttribs);

        m_LastTLASInstanceCount = TLASAttribs.InstanceCount;
        std::cout << "[C6GE] CreateRayTracingAS: built BLAS and TLAS with " << TLASAttribs.InstanceCount << " instance(s) from ECS." << std::endl;
    }

    void C6GERender::DestroyRayTracingAS()
    {
        m_pBLAS_Cube.Release();
        m_pBLAS_Plane.Release();
        m_pTLAS.Release();
        m_pTLASInstances.Release();
        m_pTLASScratch.Release();
        m_CubeRTVertexBuffer.Release();
        m_CubeRTIndexBuffer.Release();
        m_PlaneRTVertexBuffer.Release();
        m_PlaneRTIndexBuffer.Release();
        std::cout << "[C6GE] DestroyRayTracingAS: released (no-op)." << std::endl;
    }

    void C6GERender::UpdateTLAS()
    {
        if (!m_pTLAS || !m_pBLAS_Cube)
            return;

    // Build instance list from ECS: plane + cube meshes + glTF meshes
        std::vector<TLASBuildInstanceData> Instances;
        std::vector<std::string>           InstanceNames; // keep storage for names while building
        Instances.reserve(16);
        InstanceNames.reserve(16);
        // Plane instance (static)
        if (m_pBLAS_Plane)
        {
            TLASBuildInstanceData inst{};
            InstanceNames.emplace_back("Plane");
            inst.InstanceName = InstanceNames.back().c_str();
            inst.pBLAS        = m_pBLAS_Plane;
            inst.Mask         = 0xFF;
            inst.CustomId     = 0;
            float4x4 I = float4x4::Identity();
            inst.Transform.SetRotation(I.Data(), 4);
            inst.Transform.SetTranslation(0.f, 0.f, 0.f);
            Instances.push_back(inst);
        }
        // ECS cubes and glTF
        if (m_World)
        {
            auto& reg = m_World->Registry();
            auto view = reg.view<ECS::Transform, ECS::StaticMesh>();
            Uint32 cid = 1;
            for (auto e : view)
            {
                const auto& sm = view.get<ECS::StaticMesh>(e);
                if (sm.type != ECS::StaticMesh::MeshType::Cube)
                    continue;
                const auto& tr = view.get<ECS::Transform>(e);
                const float4x4 W = tr.WorldMatrix();
                TLASBuildInstanceData inst{};
                InstanceNames.emplace_back(std::string("Cube_") + std::to_string(static_cast<Uint32>(e)));
                inst.InstanceName = InstanceNames.back().c_str();
                inst.pBLAS        = m_pBLAS_Cube;
                inst.Mask         = 0xFF;
                inst.CustomId     = cid;
                inst.Transform.SetRotation(W.Data(), 4);
                inst.Transform.SetTranslation(W.m30, W.m31, W.m32);
                Instances.push_back(inst);
                ++cid;
                if (cid >= m_MaxTLASInstances)
                    break;
            }

            // glTF dynamic meshes
            auto viewGltf = reg.view<ECS::Transform, ECS::Mesh>();
            for (auto e : viewGltf)
            {
                const auto& m = viewGltf.get<ECS::Mesh>(e);
                if (m.kind != ECS::Mesh::Kind::Dynamic || m.assetId.empty())
                    continue;
                EnsureGLTFBLAS(m.assetId);
                auto itAsset = m_GltfAssets.find(m.assetId);
                if (itAsset == m_GltfAssets.end())
                    continue;
                if (!itAsset->second.BLAS)
                    continue;
                const auto& tr = viewGltf.get<ECS::Transform>(e);
                const float4x4 W = tr.WorldMatrix();
                TLASBuildInstanceData inst{};
                InstanceNames.emplace_back(std::string("GLTF_") + std::to_string(static_cast<Uint32>(e)));
                inst.InstanceName = InstanceNames.back().c_str();
                inst.pBLAS        = itAsset->second.BLAS;
                inst.Mask         = 0xFF;
                inst.CustomId     = cid;
                inst.Transform.SetRotation(W.Data(), 4);
                inst.Transform.SetTranslation(W.m30, W.m31, W.m32);
                Instances.push_back(inst);
                ++cid;
                if (cid >= m_MaxTLASInstances)
                    break;
            }
        }

        const Uint32 InstanceCount = static_cast<Uint32>(Instances.size());

        // Ensure scratch and instance buffers exist and are large enough
        if (!m_pTLASScratch)
        {
            BufferDesc BuffDesc;
            BuffDesc.Name      = "TLAS Scratch Buffer";
            BuffDesc.Usage     = USAGE_DEFAULT;
            BuffDesc.BindFlags = BIND_RAY_TRACING;
            BuffDesc.Size      = std::max(m_pTLAS->GetScratchBufferSizes().Build, m_pTLAS->GetScratchBufferSizes().Update);
            m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_pTLASScratch);
        }
        if (!m_pTLASInstances || m_pTLASInstances->GetDesc().Size < Uint64{TLAS_INSTANCE_DATA_SIZE} * Uint64{std::max(InstanceCount, 1u)})
        {
            BufferDesc BuffDesc;
            BuffDesc.Name      = "TLAS Instance Buffer";
            BuffDesc.Usage     = USAGE_DEFAULT;
            BuffDesc.BindFlags = BIND_RAY_TRACING;
            BuffDesc.Size      = Uint64{TLAS_INSTANCE_DATA_SIZE} * Uint64{std::max(InstanceCount, 1u)};
            m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_pTLASInstances);
        }

        BuildTLASAttribs TLASAttribs{};
        TLASAttribs.pTLAS                        = m_pTLAS;
        // Rebuild if instance count changed; otherwise do a fast update
        TLASAttribs.Update                       = (InstanceCount == m_LastTLASInstanceCount);
        TLASAttribs.pScratchBuffer               = m_pTLASScratch;
        TLASAttribs.pInstanceBuffer              = m_pTLASInstances;
        TLASAttribs.pInstances                   = Instances.empty() ? nullptr : Instances.data();
        TLASAttribs.InstanceCount                = InstanceCount;
        TLASAttribs.TLASTransitionMode           = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        TLASAttribs.BLASTransitionMode           = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        TLASAttribs.InstanceBufferTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        TLASAttribs.ScratchBufferTransitionMode  = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

        m_pImmediateContext->BuildTLAS(TLASAttribs);
        m_LastTLASInstanceCount = InstanceCount;
    }

    void C6GERender::CreateMSAARenderTarget()
    {
        if (m_SampleCount == 1)
            return;

        const SwapChainDesc& SCDesc = m_pSwapChain->GetDesc();
        // Create window-size multi-sampled offscreen render target
        TextureDesc ColorDesc;
        ColorDesc.Name           = "Multisampled render target";
        ColorDesc.Type           = RESOURCE_DIM_TEX_2D;
        ColorDesc.BindFlags      = BIND_RENDER_TARGET;
        ColorDesc.Width          = m_FramebufferWidth;
        ColorDesc.Height         = m_FramebufferHeight;
        ColorDesc.MipLevels      = 1;
        ColorDesc.Format         = SCDesc.ColorBufferFormat;
        bool NeedsSRGBConversion = m_pDevice->GetDeviceInfo().IsD3DDevice() && (ColorDesc.Format == TEX_FORMAT_RGBA8_UNORM_SRGB || ColorDesc.Format == TEX_FORMAT_BGRA8_UNORM_SRGB);
        if (NeedsSRGBConversion)
        {
            // Internally Direct3D swap chain images are not SRGB, and ResolveSubresource
            // requires source and destination formats to match exactly or be typeless.
            // So we will have to create a typeless texture and use SRGB render target view with it.
            ColorDesc.Format = ColorDesc.Format == TEX_FORMAT_RGBA8_UNORM_SRGB ? TEX_FORMAT_RGBA8_TYPELESS : TEX_FORMAT_BGRA8_TYPELESS;
        }

        // Set the desired number of samples
        ColorDesc.SampleCount = m_SampleCount;
        // Define optimal clear value
        ColorDesc.ClearValue.Format   = SCDesc.ColorBufferFormat;
        ColorDesc.ClearValue.Color[0] = 0.125f;
        ColorDesc.ClearValue.Color[1] = 0.125f;
        ColorDesc.ClearValue.Color[2] = 0.125f;
        ColorDesc.ClearValue.Color[3] = 1.f;
    RefCntAutoPtr<ITexture> pColor;
    m_pDevice->CreateTexture(ColorDesc, nullptr, &pColor);
    m_pMSColorTex = pColor; // keep parent alive for default RTV

        // Store the render target view
        m_pMSColorRTV.Release();
        if (NeedsSRGBConversion)
        {
            TextureViewDesc RTVDesc;
            RTVDesc.ViewType = TEXTURE_VIEW_RENDER_TARGET;
            RTVDesc.Format   = SCDesc.ColorBufferFormat;
            if (m_pMSColorTex)
                m_pMSColorTex->CreateView(RTVDesc, &m_pMSColorRTV);
        }
        else
        {
            if (m_pMSColorTex)
                m_pMSColorRTV = m_pMSColorTex->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
        }

        // Create window-size multi-sampled depth buffer
        TextureDesc DepthDesc = ColorDesc;
        DepthDesc.Name        = "Multisampled depth buffer";
        DepthDesc.Format      = SCDesc.DepthBufferFormat;
        DepthDesc.BindFlags   = BIND_DEPTH_STENCIL;
        // Define optimal clear value
        DepthDesc.ClearValue.Format               = DepthDesc.Format;
        DepthDesc.ClearValue.DepthStencil.Depth   = 1;
        DepthDesc.ClearValue.DepthStencil.Stencil = 0;

        RefCntAutoPtr<ITexture> pDepth;
        m_pDevice->CreateTexture(DepthDesc, nullptr, &pDepth);
        m_pMSDepthTex = pDepth; // keep parent alive for default DSV
        // Store the depth-stencil view
        if (m_pMSDepthTex)
            m_pMSDepthDSV = m_pMSDepthTex->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
    }

    void C6GERender::CreatePostFXTargets()
    {
        // Create a single render target that matches the off-screen framebuffer
        DestroyPostFXTargets();

        if (m_FramebufferWidth == 0 || m_FramebufferHeight == 0)
            return;

        TextureDesc Desc;
        Desc.Name      = "PostFX Color";
        Desc.Type      = RESOURCE_DIM_TEX_2D;
        Desc.Width     = m_FramebufferWidth;
        Desc.Height    = m_FramebufferHeight;
        Desc.MipLevels = 1;
        // Use non-sRGB format to avoid double-encoding when we perform manual gamma in shader
        auto SCFmt = m_pSwapChain->GetDesc().ColorBufferFormat;
        switch (SCFmt)
        {
            case TEX_FORMAT_RGBA8_UNORM_SRGB: Desc.Format = TEX_FORMAT_RGBA8_UNORM; break;
            case TEX_FORMAT_BGRA8_UNORM_SRGB: Desc.Format = TEX_FORMAT_BGRA8_UNORM; break;
            default:                           Desc.Format = SCFmt;                break;
        }
        Desc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
        RefCntAutoPtr<ITexture> pTex;
        m_pDevice->CreateTexture(Desc, nullptr, &pTex);
        m_pPostTexture = pTex;
        if (m_pPostTexture)
        {
            m_pPostRTV = m_pPostTexture->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
            m_pPostSRV = m_pPostTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        }
    }

    void C6GERender::DestroyPostFXTargets()
    {
        m_pPostRTV.Release();
        m_pPostSRV.Release();
        m_pPostTexture.Release();
        // Invalidate any cached SRBs referencing old SRVs
        m_PostGammaSRBCache.clear();
    }

    void C6GERender::CreatePostFXPSOs()
    {
        // Fullscreen gamma-correction pass using DiligentFX GammaCorrection.fx equivalent
        // We'll use our local fullscreen VS and a simple pixel shader from assets/post_gamma.psh
        ShaderCreateInfo CI;
        CI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        CI.Desc.UseCombinedTextureSamplers = true;
        CI.CompileFlags = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

        RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
        m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
        CI.pShaderSourceStreamFactory = pShaderSourceFactory;

        RefCntAutoPtr<IShader> pVS, pPS;
        {
            CI.Desc.ShaderType = SHADER_TYPE_VERTEX;
            CI.EntryPoint      = "main";
            CI.Desc.Name       = "PostFX VS";
            CI.FilePath        = "assets/rt_composite.vsh"; // fullscreen triangle VS
            m_pDevice->CreateShader(CI, &pVS);
        }
        {
            CI.Desc.ShaderType = SHADER_TYPE_PIXEL;
            CI.EntryPoint      = "main";
            CI.Desc.Name       = "PostFX Gamma PS";
            CI.FilePath        = "assets/post_gamma.psh";
            // Decide if we apply manual gamma based on output RT format (we use non-sRGB -> manual gamma = 1)
            ShaderMacro Macros[] = {
                {"APPLY_MANUAL_GAMMA", "1"}
            };
            CI.Macros = {Macros, static_cast<Uint32>(_countof(Macros))};
            m_pDevice->CreateShader(CI, &pPS);
        }

        GraphicsPipelineStateCreateInfo PSOCI;
        PSOCI.PSODesc.Name                                = "PostFX Gamma PSO";
        PSOCI.PSODesc.PipelineType                        = PIPELINE_TYPE_GRAPHICS;
        PSOCI.pVS                                         = pVS;
        PSOCI.pPS                                         = pPS;
        PSOCI.GraphicsPipeline.PrimitiveTopology          = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        PSOCI.GraphicsPipeline.NumRenderTargets           = 1;
        // Match the post target's format
        PSOCI.GraphicsPipeline.RTVFormats[0]              = m_pPostTexture ? m_pPostTexture->GetDesc().Format : m_pSwapChain->GetDesc().ColorBufferFormat;
        PSOCI.GraphicsPipeline.DSVFormat                  = TEX_FORMAT_UNKNOWN;
        PSOCI.GraphicsPipeline.DepthStencilDesc.DepthEnable      = False;
        PSOCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = False;
        PSOCI.GraphicsPipeline.RasterizerDesc.CullMode    = CULL_MODE_NONE;

        // Resource layout: one dynamic SRV and immutable sampler
        ShaderResourceVariableDesc Vars[] = {
            {SHADER_TYPE_PIXEL, "g_TextureColor", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
        };
        PSOCI.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
        PSOCI.PSODesc.ResourceLayout.Variables           = Vars;
        PSOCI.PSODesc.ResourceLayout.NumVariables        = _countof(Vars);

        SamplerDesc Smpl;
        Smpl.MinFilter = FILTER_TYPE_LINEAR;
        Smpl.MagFilter = FILTER_TYPE_LINEAR;
        Smpl.MipFilter = FILTER_TYPE_LINEAR;
        Smpl.AddressU  = TEXTURE_ADDRESS_CLAMP;
        Smpl.AddressV  = TEXTURE_ADDRESS_CLAMP;
        Smpl.AddressW  = TEXTURE_ADDRESS_CLAMP;
        ImmutableSamplerDesc Imtbl[] = {
            {SHADER_TYPE_PIXEL, "g_TextureColor", Smpl}
        };
        PSOCI.PSODesc.ResourceLayout.ImmutableSamplers    = Imtbl;
        PSOCI.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(Imtbl);

        m_pDevice->CreateGraphicsPipelineState(PSOCI, &m_pPostGammaPSO);
        if (m_pPostGammaPSO)
        {
            // Clear SRB cache on PSO (re)creation
            m_PostGammaSRBCache.clear();
        }
    }

    void C6GERender::Update(double CurrTime, double ElapsedTime, bool DoUpdateUI)
    {
        SampleBase::Update(CurrTime, ElapsedTime, DoUpdateUI);

        // Only update scene if playing
        if (!IsPlaying())
            return;

    // Legacy matrices may still be used for RT; leave them identity unless user updates entities explicitly
    m_CubeWorldMatrix = float4x4::Identity();
    m_SecondCubeWorldMatrix = float4x4::Identity();

    // Camera is at (0, 0, -5) looking along the Z axis
    float4x4 View = float4x4::Translation(0.f, 0.0f, 5.0f);

        // Get pretransform matrix that rotates the scene according the surface orientation
        float4x4 SrfPreTransform = GetSurfacePretransformMatrix(float3{0, 0, 1});

        // Get projection matrix adjusted to the current screen orientation
    float4x4 Proj = GetAdjustedProjectionMatrix(PI_F / 4.0f, 0.1f, 100.f);

        // Compute camera view-projection matrix (world transforms applied per-object)
    // Cache matrices for gizmo math
    m_ViewMatrix           = View * SrfPreTransform; // include pretransform so it matches VP used for rendering
    m_ProjMatrix           = Proj;
    m_CameraViewProjMatrix = m_ViewMatrix * m_ProjMatrix;
    }

    void C6GERender::EnsureWorld()
    {
        if (!m_World)
            m_World = std::make_unique<ECS::World>();
        if (!m_RenderSystem)
            m_RenderSystem = std::make_unique<C6GE::Systems::RenderSystem>(this);
    }

    void C6GERender::WindowResize(Uint32 Width, Uint32 Height)
    {
        if (!m_pSwapChain || !m_pDevice)
            return;

        float NearPlane = 0.1f;
        float FarPlane = 250.f;
        float AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
        m_Camera.SetProjAttribs(NearPlane, FarPlane, AspectRatio, PI_F / 4.f,
                                m_pSwapChain->GetDesc().PreTransform, m_pDevice->GetDeviceInfo().NDC.MinZ == -1);

    // Always sync framebuffer with swap chain
    const auto& scDesc = m_pSwapChain->GetDesc();
    Uint32 targetW = scDesc.Width;
    Uint32 targetH = scDesc.Height;

    if (targetW != m_FramebufferWidth || targetH != m_FramebufferHeight)
    {
        ResizeFramebuffer(targetW, targetH);
        // Defer RT restart to next frame instead of doing it here (set flag below)
        if (m_RayTracingSupported && m_EnableRayTracing)
        {
            m_PendingRTRestart = true;
        }
        // Bump temporal index to reset jitter/feedback a bit after resize
        m_TemporalFrameIndex = 0;
    }
    }

} // namespace Diligent