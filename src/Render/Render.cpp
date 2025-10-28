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
        RefCntAutoPtr<ITexture> playTex, pauseTex;
        
        try {
            CreateTextureFromFile("Editor/PlayIcon.png", loadInfo, m_pDevice, &playTex);
            if (playTex)
            {
                auto srv = playTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
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
            CreateTextureFromFile("Editor/PauseIcon.png", loadInfo, m_pDevice, &pauseTex);
            if (pauseTex)
            {
                auto srv = pauseTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
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
            auto texture = TexturedCube::LoadTexture(m_pDevice, "../../src/Assets/C6GELogo.png");
            if (texture)
            {
                m_TextureSRV = texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
                if (!m_TextureSRV)
                {
                    std::cerr << "[C6GE] Failed to get SRV for C6GELogo.png" << std::endl;
                }
            }
            else
            {
                std::cerr << "[C6GE] Failed to load texture: ../../src/Assets/C6GELogo.png" << std::endl;
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
                            int index = sampleCountNames.size();
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

                // Ray Tracing (Hybrid Rendering) - off by default
                if (ImGui::CollapsingHeader("Ray Tracing (Hybrid)", ImGuiTreeNodeFlags_DefaultOpen))
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

                    ImGui::SameLine();
                    ImGui::TextDisabled("(?)");
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::BeginTooltip();
                        ImGui::Text("Hybrid rendering uses rasterization + ray tracing for reflections/shadows.");
                        ImGui::Text("This toggle is off by default and requires DXR (D3D12) or Vulkan RT.");
                        ImGui::EndTooltip();
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
            ShaderCI.FilePath = "../../assets/cube.vsh";
            m_pDevice->CreateShader(ShaderCI, &pVS);
        }

        // Create a geometry shader only if the device supports geometry shaders.
        RefCntAutoPtr<IShader> pGS;
        if (m_pDevice->GetDeviceInfo().Features.GeometryShaders)
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_GEOMETRY;
            ShaderCI.EntryPoint = "main";
            ShaderCI.Desc.Name = "Cube GS";
            ShaderCI.FilePath = "../../assets/cube.gsh";
            m_pDevice->CreateShader(ShaderCI, &pGS);
        }

        // Create a pixel shader
        RefCntAutoPtr<IShader> pPS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
            ShaderCI.EntryPoint = "main";
            ShaderCI.Desc.Name = "Cube PS";
            ShaderCI.FilePath = "../../assets/cube.psh";
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
        ShadowShaderCI.FilePath = "../../assets/cube_shadow.vsh";
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
        ShaderCI.FilePath = "../../assets/plane.vsh";
        m_pDevice->CreateShader(ShaderCI, &pPlaneVS);

        // Create plane pixel shader
        RefCntAutoPtr<IShader> pPlanePS;
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "Plane PS";
        ShaderCI.FilePath = "../../assets/plane.psh";
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
        NoShadowCI.FilePath = "../../assets/plane_no_shadow.psh";
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

        // Render cube to shadow map
        RenderCube(WorldToLightProjSpaceMatr, true);
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
        float4 ClearColor = {0.350f, 0.350f, 0.350f, 1.0f};
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

                // Render cube and plane (they update their own constant buffers)
                // Note: Inside render pass, MUST use RESOURCE_STATE_TRANSITION_MODE_VERIFY
                RenderCube(m_CameraViewProjMatrix, false, RESOURCE_STATE_TRANSITION_MODE_VERIFY);
                RenderPlane(RESOURCE_STATE_TRANSITION_MODE_VERIFY);

                // Composite RT output additively over the scene (if enabled)
                if (m_EnableRayTracing && m_RayTracingSupported && m_RayTracingInitialized && m_pRTCompositePSO && m_RTCompositeSRB && m_pRTOutputSRV)
                {
                    if (auto* var = m_RTCompositeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_RTOutput"))
                        var->Set(m_pRTOutputSRV, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
                    m_pImmediateContext->SetPipelineState(m_pRTCompositePSO);
                    m_pImmediateContext->CommitShaderResources(m_RTCompositeSRB, RESOURCE_STATE_TRANSITION_MODE_VERIFY);
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
            }
        }
        else
        {
            // Traditional rendering without render passes
            m_pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            RenderCube(m_CameraViewProjMatrix, false, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
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
        }

        // If using MSAA, resolve the multi-sampled render target to the framebuffer
        if (m_SampleCount > 1 && m_pMSColorRTV && m_pFramebufferTexture)
        {
            ResolveTextureSubresourceAttribs ResolveAttribs;
            ResolveAttribs.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
            ResolveAttribs.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
            m_pImmediateContext->ResolveTextureSubresource(m_pMSColorRTV->GetTexture(), m_pFramebufferTexture, ResolveAttribs);
        }

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
                CI.FilePath = "../../assets/rt_composite.vsh";
                m_pDevice->CreateShader(CI, &pVS);
            }
            {
                CI.Desc.ShaderType = SHADER_TYPE_PIXEL;
                CI.EntryPoint = "main";
                CI.Desc.Name = "RT Composite PS";
                CI.FilePath = "../../assets/rt_composite.psh";
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

        // Compute PSO that uses ray queries to produce a shadow mask into UAV
        {
            ShaderCreateInfo CI;
            CI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
            CI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
            CI.EntryPoint = "main";
            CI.Desc.Name = "RT Shadow CS";
            CI.FilePath = "../../assets/rt_shadow.csh";
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

        // --- Create TLAS with a single cube instance ---
        {
            TopLevelASDesc TLASDesc;
            TLASDesc.Name             = "Scene TLAS";
            TLASDesc.MaxInstanceCount = 1;
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
            BuffDesc.Size      = Uint64{TLAS_INSTANCE_DATA_SIZE} * Uint64{1};
            m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_pTLASInstances);
        }

        // Setup a single instance referencing the cube BLAS
        TLASBuildInstanceData Instance{};
        Instance.InstanceName = "Cube Instance";
        Instance.pBLAS        = m_pBLAS_Cube;
        Instance.Mask         = 0xFF;
        Instance.CustomId     = 0;
        // Identity transform: no rotation or translation for now
        {
            // Identity transform: rotation = identity, translation = (0,0,0)
            float4x4 I = float4x4::Identity();
            Instance.Transform.SetRotation(I.Data(), 4);
            Instance.Transform.SetTranslation(0.f, 0.f, 0.f);
        }

        BuildTLASAttribs TLASAttribs{};
        TLASAttribs.pTLAS                         = m_pTLAS;
        TLASAttribs.Update                        = false; // first build
        TLASAttribs.pScratchBuffer                = m_pTLASScratch;
        TLASAttribs.pInstanceBuffer               = m_pTLASInstances;
        TLASAttribs.pInstances                    = &Instance;
        TLASAttribs.InstanceCount                 = 1;
        TLASAttribs.TLASTransitionMode            = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        TLASAttribs.BLASTransitionMode            = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        TLASAttribs.InstanceBufferTransitionMode  = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        TLASAttribs.ScratchBufferTransitionMode   = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

        m_pImmediateContext->BuildTLAS(TLASAttribs);

        std::cout << "[C6GE] CreateRayTracingAS: built cube BLAS and single-instance TLAS." << std::endl;
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
        std::cout << "[C6GE] DestroyRayTracingAS: released (no-op)." << std::endl;
    }

    void C6GERender::UpdateTLAS()
    {
        if (!m_pTLAS || !m_pBLAS_Cube)
            return;

        // Ensure scratch and instance buffers exist
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
            BuffDesc.Size      = Uint64{TLAS_INSTANCE_DATA_SIZE} * Uint64{1};
            m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_pTLASInstances);
        }

        // Update the single instance transform using the current cube world matrix
        TLASBuildInstanceData Instance{};
        Instance.InstanceName = "Cube Instance";
        Instance.pBLAS        = m_pBLAS_Cube;
        Instance.Mask         = 0xFF;
        Instance.CustomId     = 0;
        {
            const float4x4& W = m_CubeWorldMatrix;
            Instance.Transform.SetRotation(W.Data(), 4);
            Instance.Transform.SetTranslation(W.m30, W.m31, W.m32);
        }

        BuildTLASAttribs TLASAttribs{};
        TLASAttribs.pTLAS                        = m_pTLAS;
        TLASAttribs.Update                       = true; // refit/update after initial build
        TLASAttribs.pScratchBuffer               = m_pTLASScratch;
        TLASAttribs.pInstanceBuffer              = m_pTLASInstances;
        TLASAttribs.pInstances                   = &Instance;
        TLASAttribs.InstanceCount                = 1;
        TLASAttribs.TLASTransitionMode           = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        TLASAttribs.BLASTransitionMode           = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        TLASAttribs.InstanceBufferTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        TLASAttribs.ScratchBufferTransitionMode  = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

        m_pImmediateContext->BuildTLAS(TLASAttribs);
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

        // Store the render target view
        m_pMSColorRTV.Release();
        if (NeedsSRGBConversion)
        {
            TextureViewDesc RTVDesc;
            RTVDesc.ViewType = TEXTURE_VIEW_RENDER_TARGET;
            RTVDesc.Format   = SCDesc.ColorBufferFormat;
            pColor->CreateView(RTVDesc, &m_pMSColorRTV);
        }
        else
        {
            m_pMSColorRTV = pColor->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
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
        // Store the depth-stencil view
        m_pMSDepthDSV = pDepth->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
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
        // Defer RT restart to next frame instead of doing it here (set flag below)
        if (m_RayTracingSupported && m_EnableRayTracing)
        {
            m_PendingRTRestart = true;
        }
    }
    }

} // namespace Diligent