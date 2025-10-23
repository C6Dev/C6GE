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
#include "DiligentTools/ThirdParty/imGuIZMO.quat/imGuIZMO.h"
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
#include "DiligentTools/AssetLoader/interface/GLTFLoader.hpp"
#include "DiligentFX/PBR/interface/GLTF_PBR_Renderer.hpp"

namespace Diligent
{
    // Host-side access to shader structs
    namespace HLSL
    {
#include "../../external/DiligentEngine/DiligentFX/Shaders/Common/public/BasicStructures.fxh"
#include "../../external/DiligentEngine/DiligentFX/Shaders/PBR/public/PBR_Structures.fxh"
#include "../../external/DiligentEngine/DiligentFX/Shaders/PBR/private/RenderPBR_Structures.fxh"
    }

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

    } // namespace

    C6GERender::~C6GERender()
    {
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

        // Load play/pause icons (after m_pDevice is valid)
        TextureLoadInfo loadInfo;
        loadInfo.IsSRGB = true;
        RefCntAutoPtr<ITexture> playTex, pauseTex;
        CreateTextureFromFile("Editor/PlayIcon.png", loadInfo, m_pDevice, &playTex);
        if (!playTex)
            std::cerr << "[C6GE] Failed to load Editor/PlayIcon.png" << std::endl;
        else if (auto srv = playTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE))
            m_PlayIconSRV = srv;
        else
            std::cerr << "[C6GE] Failed to get SRV for PlayIcon.png" << std::endl;

        CreateTextureFromFile("Editor/PauseIcon.png", loadInfo, m_pDevice, &pauseTex);
        if (!pauseTex)
            std::cerr << "[C6GE] Failed to load Editor/PauseIcon.png" << std::endl;
        else if (auto srv = pauseTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE))
            m_PauseIconSRV = srv;
        else
            std::cerr << "[C6GE] Failed to get SRV for PauseIcon.png" << std::endl;

        // Initialize camera position to look at the cube
        m_Camera.SetPos(float3(0.f, 0.f, 5.f));
        m_Camera.SetRotation(0.f, 0.f);

        // Create VS constant buffer used by plane/cube shaders (cbuffer "Constants")
        {
            BufferDesc CBDesc;
            CBDesc.Name = "VS Constants";
            CBDesc.Size = sizeof(Constants);
            CBDesc.Usage = USAGE_DYNAMIC;
            CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
            CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
            m_pDevice->CreateBuffer(CBDesc, nullptr, &m_VSConstants);
        }

        // Create plane and shadow-related PSOs
        CreatePlanePSO();


        // Load AnisotropyBarnLamp .bin model here
        try {
            std::string modelPath = "AnisotropyBarnLamp/glTF/AnisotropyBarnLamp.gltf";
            Diligent::GLTF::ModelCreateInfo modelCI;
            modelCI.FileName = modelPath.c_str();
            m_BarnLampModel = std::make_unique<GLTF::Model>(m_pDevice, m_pImmediateContext, modelCI);
            if (!m_BarnLampModel || m_BarnLampModel->Meshes.empty()) {
                std::cerr << "[C6GE] Failed to load AnisotropyBarnLamp model or no meshes present." << std::endl;
            } else {
                std::cout << "[C6GE] Loaded AnisotropyBarnLamp model successfully." << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "[C6GE] Exception loading AnisotropyBarnLamp model: " << e.what() << std::endl;
        }


        // Query actual swapchain and depth formats
        TEXTURE_FORMAT swapchainFormat = TEX_FORMAT_RGBA8_UNORM;
        TEXTURE_FORMAT depthFormat = TEX_FORMAT_D16_UNORM;
        if (m_pSwapChain)
        {
            const auto &scDesc = m_pSwapChain->GetDesc();
            swapchainFormat = scDesc.ColorBufferFormat;
            depthFormat = scDesc.DepthBufferFormat;
        }

        // Create frame attributes constant buffer (required by GLTF_PBR_Renderer)
        {
            BufferDesc CBDesc;
            CBDesc.Name = "PBR frame attribs buffer";
            Diligent::GLTF_PBR_Renderer::CreateInfo RendererCI;
            RendererCI.NumRenderTargets = 1;
            RendererCI.RTVFormats[0] = swapchainFormat;
            RendererCI.DSVFormat = depthFormat;
            auto TempRenderer = std::make_unique<Diligent::GLTF_PBR_Renderer>(m_pDevice, nullptr, m_pImmediateContext, RendererCI);
            CBDesc.Size = TempRenderer->GetPRBFrameAttribsSize();
            CBDesc.Usage = USAGE_DYNAMIC;
            CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
            CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
            m_pDevice->CreateBuffer(CBDesc, nullptr, &m_FrameAttribsCB);
        }

        // Create the GLTF PBR renderer
        {
            Diligent::GLTF_PBR_Renderer::CreateInfo RendererCI;
            RendererCI.NumRenderTargets = 1;
            RendererCI.RTVFormats[0] = swapchainFormat;
            RendererCI.DSVFormat = depthFormat;
            m_GLTFRenderer = std::make_unique<Diligent::GLTF_PBR_Renderer>(m_pDevice, nullptr, m_pImmediateContext, RendererCI);
        }

        // Transition IBL textures to SHADER_RESOURCE state immediately after renderer creation
        if (m_GLTFRenderer)
        {
            if (auto* pIrradianceCube = m_GLTFRenderer->GetIrradianceCubeSRV())
            {
                ITexture* pTex = pIrradianceCube->GetTexture();
                if (pTex)
                {
                    StateTransitionDesc barrier{pTex, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_SHADER_RESOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE};
                    barrier.FirstMipLevel = 0;
                    barrier.MipLevelsCount = REMAINING_MIP_LEVELS;
                    barrier.FirstArraySlice = 0;
                    barrier.ArraySliceCount = REMAINING_ARRAY_SLICES;
                    m_pImmediateContext->TransitionResourceStates(1, &barrier);
                }
            }
            if (auto* pPrefEnvMap = m_GLTFRenderer->GetPrefilteredEnvMapSRV())
            {
                ITexture* pTex = pPrefEnvMap->GetTexture();
                if (pTex)
                {
                    StateTransitionDesc barrier{pTex, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_SHADER_RESOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE};
                    barrier.FirstMipLevel = 0;
                    barrier.MipLevelsCount = REMAINING_MIP_LEVELS;
                    barrier.FirstArraySlice = 0;
                    barrier.ArraySliceCount = REMAINING_ARRAY_SLICES;
                    m_pImmediateContext->TransitionResourceStates(1, &barrier);
                }
            }
        }

        // Create resource bindings for the model
        if (m_BarnLampModel)
        {
            m_ModelResourceBindings = m_GLTFRenderer->CreateResourceBindings(*m_BarnLampModel, m_FrameAttribsCB);
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

        CreateFramebuffer();

        // Create shadow map and bind it to plane SRB / shadow-visualization SRB
        CreateShadowMap();
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
        // Ensure ImGui display size and framebuffer scale are set correctly (for HiDPI/Retina)
        ImGui::GetIO().DisplaySize = ImVec2((float)m_FramebufferWidth, (float)m_FramebufferHeight);
#ifdef __APPLE__
        // On macOS, framebuffer scale may be >1 for Retina
        ImGui::GetIO().DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
        if (m_pSwapChain)
        {
            const auto& scDesc = m_pSwapChain->GetDesc();
            if (scDesc.Width > 0 && scDesc.Height > 0)
            {
                ImGui::GetIO().DisplayFramebufferScale.x = (float)scDesc.Width / (float)m_FramebufferWidth;
                ImGui::GetIO().DisplayFramebufferScale.y = (float)scDesc.Height / (float)m_FramebufferHeight;
            }
        }
#endif
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

        if (RenderSettingsOpen)
        {
            if (ImGui::Begin("Render Settings", &RenderSettingsOpen, ImGuiWindowFlags_NoCollapse))
            {
                ImGui::SliderFloat("Line Width", &m_LineWidth, 1.f, 10.f);
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

            // Create a fullscreen dockspace
            auto *viewport = ImGui::GetMainViewport();
            // Offset dockspace below the main menu bar
            ImVec2 dockspacePos = viewport->Pos;
            float menuBarHeight = ImGui::GetFrameHeight();
            dockspacePos.y += menuBarHeight;
            ImVec2 dockspaceSize = viewport->Size;
            dockspaceSize.y -= menuBarHeight;
            ImGui::SetNextWindowPos(dockspacePos);
            ImGui::SetNextWindowSize(dockspaceSize);
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
                            ImGui::Text("Version: V0.1 Beta");
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

                    // Display the framebuffer texture at viewport size
                    ImGui::Image(
                        reinterpret_cast<void *>(m_pFramebufferSRV.RawPtr()),
                        contentSize);
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
                // Add scene hierarchy content here
            }
            ImGui::End();

            if (ImGui::Begin("Properties", nullptr, ImGuiWindowFlags_None))
            {
                ImGui::Text("Properties Panel");
                // Add properties content here
            }
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

    // Cube PSO fully removed
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
        ShaderCI.FilePath = "plane.vsh";
        m_pDevice->CreateShader(ShaderCI, &pPlaneVS);

        // Create plane pixel shader
        RefCntAutoPtr<IShader> pPlanePS;
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "Plane PS";
        ShaderCI.FilePath = "plane.psh";
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
        NoShadowCI.FilePath = "plane_no_shadow.psh";
        m_pDevice->CreateShader(NoShadowCI, &pNoShadowPS);
    NoShadowPSOCI.pVS = pPlaneVS;
    NoShadowPSOCI.pPS = pNoShadowPS;
    NoShadowPSOCI.PSODesc.Name = "Plane NoShadow PSO";
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
        RefCntAutoPtr<ITexture> ShadowMap;
        m_pDevice->CreateTexture(SMDesc, nullptr, &ShadowMap);
        m_ShadowMapSRV = ShadowMap->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        m_ShadowMapDSV = ShadowMap->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);

        // Create SRBs that use shadow map as mutable variable
        m_PlaneSRB.Release();
        if (m_pPlanePSO)
            m_pPlanePSO->CreateShaderResourceBinding(&m_PlaneSRB, true);
        if (m_PlaneSRB)
            m_PlaneSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_ShadowMap")->Set(m_ShadowMapSRV);

        // Visualization SRB removed; only bind shadow map to plane SRB
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

        // Render cube to shadow map
        RenderCube(WorldToLightProjSpaceMatr, true);
    }

    void C6GERender::RenderCube(const float4x4& CameraViewProj, bool IsShadowPass)
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
        m_pImmediateContext->SetVertexBuffers(0, 1, pBuffs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
        m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        if (IsShadowPass)
        {
            if (!m_pCubeShadowPSO)
            {
                std::cerr << "[C6GE] Error: Cube shadow PSO is null; skipping cube shadow draw." << std::endl;
                return;
            }
            m_pImmediateContext->SetPipelineState(m_pCubeShadowPSO);
            if (m_CubeShadowSRB)
                m_pImmediateContext->CommitShaderResources(m_CubeShadowSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
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
                m_pImmediateContext->CommitShaderResources(m_CubeSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }

        DrawIndexedAttribs DrawAttrs;
        DrawAttrs.IndexType = VT_UINT32;
        DrawAttrs.NumIndices = 36;
        DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
        m_pImmediateContext->DrawIndexed(DrawAttrs);
    }

    void C6GERender::RenderPlane()
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
            m_pImmediateContext->CommitShaderResources(m_PlaneSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }
        else
        {
            // No-shadow PSO: ensure its SRB (if any) is committed to satisfy backend validation
            if (m_PlaneNoShadowSRB)
            {
                m_pImmediateContext->CommitShaderResources(m_PlaneNoShadowSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
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

        // Recreate framebuffer with new size
        CreateFramebuffer();
    }

    // Render a frame
    void C6GERender::Render()
    {
        // Guard: Only render if framebuffer has valid size
        if (m_FramebufferWidth == 0 || m_FramebufferHeight == 0 || !m_pFramebufferRTV || !m_pFramebufferDSV)
        {
            return;
        }

        // 1) Render shadow map
        if (RenderShadows && m_ShadowMapDSV)
        {
            m_pImmediateContext->SetRenderTargets(0, nullptr, m_ShadowMapDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            m_pImmediateContext->ClearDepthStencil(m_ShadowMapDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            RenderShadowMap();
        }

        // 2) Render scene into off-screen framebuffer for ImGui viewport
        ITextureView *pRTV = m_pFramebufferRTV;
        ITextureView *pDSV = m_pFramebufferDSV;
        float4 ClearColor = {0.350f, 0.350f, 0.350f, 1.0f};
        if (m_ConvertPSOutputToGamma)
            ClearColor = LinearToSRGB(ClearColor);

        m_pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);



        // Render the loaded GLTF model instead of the cube
        if (m_BarnLampModel && m_GLTFRenderer && m_FrameAttribsCB)
        {
            // Build frame attributes (camera + renderer params + lights)
            MapHelper<HLSL::PBRFrameAttribs> FrameAttribs{m_pImmediateContext, m_FrameAttribsCB, MAP_WRITE, MAP_FLAG_DISCARD};

            // Camera setup
            HLSL::CameraAttribs& CamAttribs     = FrameAttribs->Camera;
            HLSL::CameraAttribs& PrevCamAttribs = FrameAttribs->PrevCamera;

            // Simple camera: position at (0,0,-5) looking towards +Z; apply surface pretransform
            const float4x4 View           = float4x4::Translation(0.f, 0.0f, 1.0f);
            const float4x4 SrfPreTransform = GetSurfacePretransformMatrix(float3{0, 0, 1});
            const float4x4 Proj            = GetAdjustedProjectionMatrix(PI_F / 4.0f, 0.1f, 100.f);
            const float4x4 ViewMatrix      = View;
            const float4x4 ProjMatrix      = Proj;
            const float4x4 ViewProj        = ViewMatrix * SrfPreTransform * ProjMatrix;
            const float4x4 WorldMatrix     = ViewMatrix.Inverse();

            CamAttribs.f4ViewportSize = float4{
                static_cast<float>(m_FramebufferWidth),
                static_cast<float>(m_FramebufferHeight),
                1.f / static_cast<float>(m_FramebufferWidth),
                1.f / static_cast<float>(m_FramebufferHeight)
            };

            CamAttribs.fHandness = 1.0f;

            WriteShaderMatrix(&CamAttribs.mView, ViewMatrix, !m_PackMatrixRowMajor);
            WriteShaderMatrix(&CamAttribs.mProj, ProjMatrix, !m_PackMatrixRowMajor);
            WriteShaderMatrix(&CamAttribs.mViewProj, ViewProj, !m_PackMatrixRowMajor);
            WriteShaderMatrix(&CamAttribs.mViewInv, WorldMatrix, !m_PackMatrixRowMajor);
            WriteShaderMatrix(&CamAttribs.mProjInv, ProjMatrix.Inverse(), !m_PackMatrixRowMajor);
            WriteShaderMatrix(&CamAttribs.mViewProjInv, ViewProj.Inverse(), !m_PackMatrixRowMajor);
            CamAttribs.f4Position = float4{float3::MakeVector(WorldMatrix[3]), 1};
            CamAttribs.f2Jitter   = float2{0, 0};

            // Clip planes
            float fNearPlaneZ = 0.1f;
            float fFarPlaneZ  = 100.0f;
            ProjMatrix.GetNearFarClipPlanes(fNearPlaneZ, fFarPlaneZ, m_pDevice->GetDeviceInfo().NDC.MinZ == -1);
            CamAttribs.SetClipPlanes(fNearPlaneZ, fFarPlaneZ);

            // For now, set PrevCamera = CurrCamera (no motion vectors)
            PrevCamAttribs = CamAttribs;

            // Default light (directional)
            HLSL::PBRLightAttribs* Lights = reinterpret_cast<HLSL::PBRLightAttribs*>(FrameAttribs + 1);
            GLTF::Light DefaultLight;
            DefaultLight.Type      = GLTF::Light::TYPE::DIRECTIONAL;
            DefaultLight.Color     = float3{1, 1, 1};
            DefaultLight.Intensity = 3.0f;
            const float3 LightDir  = m_LightDirection; // Already normalized
            GLTF_PBR_Renderer::WritePBRLightShaderAttribs({&DefaultLight, nullptr, &LightDir, 1.0f}, Lights);

            // Renderer parameters
            HLSL::PBRRendererShaderParameters& RendererParams = FrameAttribs->Renderer;
            m_GLTFRenderer->SetInternalShaderParameters(RendererParams);
            RendererParams.LightCount = 1;
            RendererParams.DebugView  = static_cast<int>(GLTF_PBR_Renderer::DebugViewType::None);
            RendererParams.MipBias    = 0;

            // Compute transforms (identity for now, or use m_CubeWorldMatrix for placement)
            GLTF::ModelTransforms transforms;
            m_BarnLampModel->ComputeTransforms(0, transforms, m_CubeWorldMatrix); // Use cube's transform for now

            // Ensure IBL textures are in correct state before rendering
            if (auto* pIrradianceCube = m_GLTFRenderer->GetIrradianceCubeSRV())
            {
                ITexture* pTex = pIrradianceCube->GetTexture();
                if (pTex)
                {
                    StateTransitionDesc barrier{pTex, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_SHADER_RESOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE};
                    barrier.FirstMipLevel  = 0;
                    barrier.MipLevelsCount = REMAINING_MIP_LEVELS;
                    barrier.FirstArraySlice = 0;
                    barrier.ArraySliceCount = REMAINING_ARRAY_SLICES;
                    m_pImmediateContext->TransitionResourceStates(1, &barrier);
                }
            }
            if (auto* pPrefEnvMap = m_GLTFRenderer->GetPrefilteredEnvMapSRV())
            {
                ITexture* pTex = pPrefEnvMap->GetTexture();
                if (pTex)
                {
                    StateTransitionDesc barrier{pTex, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_SHADER_RESOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE};
                    barrier.FirstMipLevel  = 0;
                    barrier.MipLevelsCount = REMAINING_MIP_LEVELS;
                    barrier.FirstArraySlice = 0;
                    barrier.ArraySliceCount = REMAINING_ARRAY_SLICES;
                    m_pImmediateContext->TransitionResourceStates(1, &barrier);
                }
            }

            // Bind per-frame data and prepare renderer
            m_GLTFRenderer->Begin(m_pImmediateContext);

            // Render the model with proper IBL texture states
            m_GLTFRenderer->Render(
                m_pImmediateContext,
                *m_BarnLampModel,
                transforms,
                nullptr, // No motion vectors
                Diligent::GLTF_PBR_Renderer::RenderInfo{},
                &m_ModelResourceBindings
            );
        }

        // Render the plane as before
        RenderPlane();

        // Shadow visualization removed. No additional overlays.

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

    void C6GERender::Update(double CurrTime, double ElapsedTime, bool DoUpdateUI)
    {
        SampleBase::Update(CurrTime, ElapsedTime, DoUpdateUI);

        // Only update scene if playing
        if (!IsPlaying())
            return;

    // Apply rotation
    m_CubeWorldMatrix = float4x4::RotationY(static_cast<float>(CurrTime) * 1.0f) * float4x4::RotationX(-PI_F * 0.1f);

        // Camera is at (0, 0, -5) looking along the Z axis
        float4x4 View = float4x4::Translation(0.f, 0.0f, 5.0f);

        // Get pretransform matrix that rotates the scene according the surface orientation
        float4x4 SrfPreTransform = GetSurfacePretransformMatrix(float3{0, 0, 1});

        // Get projection matrix adjusted to the current screen orientation
        float4x4 Proj = GetAdjustedProjectionMatrix(PI_F / 4.0f, 0.1f, 100.f);

        // Compute camera view-projection matrix (world transforms applied per-object)
    m_CameraViewProjMatrix = View * SrfPreTransform * Proj;
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
    }
    }

} // namespace Diligent