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
    bool RenderShadows = true;
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
        struct InstanceData
        {
            float4x4 Matrix;
            float TextureInd = 0;
        };
    } // namespace

    C6GERender::~C6GERender()
    {
    }

    void C6GERender::ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs &Attribs)
    {
        SampleBase::ModifyEngineInitInfo(Attribs);

        Attribs.EngineCI.Features.DepthClamp = DEVICE_FEATURE_STATE_OPTIONAL;

        // Request deferred contexts from the engine so worker threads can use them.
        // Reserve at least 2 deferred contexts or (hardware_concurrency - 1), whichever is larger.
        {
            Uint32 NumDeferred = std::max(std::thread::hardware_concurrency() > 1 ? std::thread::hardware_concurrency() - 1 : 1u, 2u);
            Attribs.EngineCI.NumDeferredContexts = NumDeferred;
        }

#if D3D12_SUPPORTED
        if (Attribs.DeviceType == RENDER_DEVICE_TYPE_D3D12)
        {
            EngineD3D12CreateInfo &D3D12CI = static_cast<EngineD3D12CreateInfo &>(Attribs.EngineCI);
            D3D12CI.GPUDescriptorHeapSize[1] = 1024; // Sampler descriptors
            D3D12CI.GPUDescriptorHeapDynamicSize[1] = 1024;
        }
#endif
    }

    void C6GERender::Multithreading()
    {
        // TODO: StopWorkerThreads();
    }

    void C6GERender::Initialize(const SampleInitInfo &InitInfo)
    {
        SampleBase::Initialize(InitInfo);

        // Limit maximum worker threads to the number of valid (non-null) deferred contexts provided by the engine.
        int NumValidDeferredCtx = 0;
        for (size_t i = 0; i < m_pDeferredContexts.size(); ++i)
            if (m_pDeferredContexts[i])
                ++NumValidDeferredCtx;
        m_MaxThreads = NumValidDeferredCtx;
        if (m_NumWorkerThreads > m_MaxThreads)
            m_NumWorkerThreads = m_MaxThreads;

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

        std::vector<StateTransitionDesc> Barriers;

        CreatePipelineState(Barriers);

        CreateInstanceBuffer();
        LoadTextures(Barriers);

        // Execute all barriers after LoadTextures so texture transitions are included in Barriers
        if (!Barriers.empty())
            m_pImmediateContext->TransitionResourceStates(static_cast<Uint32>(Barriers.size()), Barriers.data());

        // Ensure dynamic uniform buffers are allocated on Vulkan backend by mapping them once.
        // Memory for dynamic buffers is allocated when they are first mapped.
        if (m_InstanceConstants)
        {
            MapHelper<float4x4> InitInstCB(m_pImmediateContext, m_InstanceConstants, MAP_WRITE, MAP_FLAG_DISCARD);
            // Initialize with identity to allocate memory; actual data will be updated per-draw.
            *InitInstCB = float4x4::Identity();
        }

        // TODO: StartWorkerThreads(m_NumWorkerThreads);

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

        // Create cube vertex and index buffers used by DrawIndexed
        m_CubeVertexBuffer = TexturedCube::CreateVertexBuffer(m_pDevice, GEOMETRY_PRIMITIVE_VERTEX_FLAG_POS_TEX);
        m_CubeIndexBuffer = TexturedCube::CreateIndexBuffer(m_pDevice);
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
                y = vpPos.y + 44.0f;
            }
            ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
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
                if (ImGui::SliderInt("Grid Size", &m_GridSize, 1, 32))
                {
                    PopulateInstanceBuffer();
                }
            }
            {
                // Show diagnostics about deferred contexts so the user knows why worker threads may be unavailable
                int totalDeferred = static_cast<int>(m_pDeferredContexts.size());
                int validDeferred = 0;
                for (size_t i = 0; i < m_pDeferredContexts.size(); ++i)
                    if (m_pDeferredContexts[i])
                        ++validDeferred;

                ImGui::Text("Deferred contexts: %d total, %d valid", totalDeferred, validDeferred);
                if (validDeferred == 0)
                {
                    ImGui::TextWrapped("Worker threads are not available because no valid deferred contexts were provided by the engine for the current backend.\nIf you expect worker threads to be available, rebuild after enabling deferred contexts in ModifyEngineInitInfo or use a backend that supports deferred contexts (Vulkan/D3D12).\n");
                }

                // Allow the user to change the slider up to the total number of deferred contexts
                // even if some of them are null. Starting threads will only use valid contexts.
                ImGui::BeginDisabled(totalDeferred == 0);
                if (ImGui::SliderInt("Worker Threads", &m_NumWorkerThreads, 0, totalDeferred))
                {
                    StopWorkerThreads();
                    StartWorkerThreads(m_NumWorkerThreads);
                }
                ImGui::EndDisabled();
            }
            ImGui::End();
        }
    }

    void C6GERender::UpdateViewportUI()
    {
        {
            // Create a fullscreen dockspace
            auto *viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);

            ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

            ImGui::Begin("DockSpaceWindow", nullptr, window_flags);
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
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

            // Setup default docking layout on first run
            static bool first_time = true;
            if (first_time)
            {
                first_time = false;
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

            ImGui::End();

            // Create the viewport window (now docked but moveable)
            ImGuiWindowFlags viewport_flags = ImGuiWindowFlags_None;
            if (ImGui::Begin("Viewport", nullptr, viewport_flags))
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

    void C6GERender::CreatePipelineState(std::vector<StateTransitionDesc> &Barriers)
    {
        // Create a shader source stream factory to load shaders from files.
        RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
        m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);

        TexturedCube::CreatePSOInfo CubePsoCI;
        CubePsoCI.pDevice = m_pDevice;
        CubePsoCI.RTVFormat = m_pSwapChain->GetDesc().ColorBufferFormat;
        CubePsoCI.DSVFormat = m_pSwapChain->GetDesc().DepthBufferFormat;
        CubePsoCI.pShaderSourceFactory = pShaderSourceFactory;
        CubePsoCI.VSFilePath = "cube.vsh";
        CubePsoCI.PSFilePath = "cube.psh";
        CubePsoCI.Components = GEOMETRY_PRIMITIVE_VERTEX_FLAG_POS_TEX;

        m_pPSO = TexturedCube::CreatePipelineState(CubePsoCI, m_ConvertPSOutputToGamma);

        // Create dynamic uniform buffer that will store our transformation matrix
        // Dynamic buffers can be frequently updated by the CPU
        CreateUniformBuffer(m_pDevice, sizeof(float4x4) * 2, "VS constants CB", &m_VSConstants);
        CreateUniformBuffer(m_pDevice, sizeof(float4x4), "Instance constants CB", &m_InstanceConstants);
        // Explicitly transition the buffers to RESOURCE_STATE_CONSTANT_BUFFER state
        Barriers.emplace_back(m_VSConstants, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE);
        Barriers.emplace_back(m_InstanceConstants, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE);

        // Since we did not explicitly specify the type for 'Constants' and 'InstanceData' variables,
        // default type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables
        // never change and are bound directly to the pipeline state object.
        m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);
        m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "InstanceData")->Set(m_InstanceConstants);
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

    void C6GERender::CreateInstanceBuffer()
    {
        // Create instance data buffer that will store transformation matrices
        BufferDesc InstBuffDesc;
        InstBuffDesc.Name = "Instance data buffer";
        // Use default usage as this buffer will only be updated when grid size changes
        InstBuffDesc.Usage = USAGE_DEFAULT;
        InstBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
        InstBuffDesc.Size = sizeof(InstanceData) * MaxInstances;
        m_pDevice->CreateBuffer(InstBuffDesc, nullptr, &m_InstanceBuffer);
        PopulateInstanceBuffer();
    }

    void C6GERender::LoadTextures(std::vector<StateTransitionDesc> &Barriers)
    {
        // Load textures
        for (int tex = 0; tex < NumTextures; ++tex)
        {
            // Load current texture
            std::stringstream FileNameSS;
            FileNameSS << "C6GELogo" << tex << ".png";
            std::string FileName = FileNameSS.str();

            RefCntAutoPtr<ITexture> SrcTex = TexturedCube::LoadTexture(m_pDevice, FileName.c_str());
            // Get shader resource view from the texture
            m_TextureSRV[tex] = SrcTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
            // Transition textures to shader resource state
            Barriers.emplace_back(SrcTex, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_SHADER_RESOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE);
        }

        // Set texture SRV in the SRB
        for (int tex = 0; tex < NumTextures; ++tex)
        {
            // Create one Shader Resource Binding for every texture
            // http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/
            m_pPSO->CreateShaderResourceBinding(&m_SRB[tex], true);
            m_SRB[tex]->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TextureSRV[tex]);
        }
    }

    void C6GERender::PopulateInstanceBuffer()
    {
        const size_t zGridSize = static_cast<size_t>(m_GridSize);
        m_Instances.resize(zGridSize * zGridSize * zGridSize);
        // Populate instance data buffer
        float fGridSize = static_cast<float>(m_GridSize);

        std::mt19937 gen; // Standard mersenne_twister_engine. Use default seed
                          // to generate consistent distribution.

        std::uniform_real_distribution<float> scale_distr(0.3f, 1.0f);
        std::uniform_real_distribution<float> offset_distr(-0.15f, +0.15f);
        std::uniform_real_distribution<float> rot_distr(-PI_F, +PI_F);
        std::uniform_int_distribution<Int32> tex_distr(0, NumTextures - 1);

        float BaseScale = 0.6f / fGridSize;
        int instId = 0;
        for (int x = 0; x < m_GridSize; ++x)
        {
            for (int y = 0; y < m_GridSize; ++y)
            {
                for (int z = 0; z < m_GridSize; ++z)
                {
                    // Add random offset from central position in the grid
                    float xOffset = 2.f * (x + 0.5f + offset_distr(gen)) / fGridSize - 1.f;
                    float yOffset = 2.f * (y + 0.5f + offset_distr(gen)) / fGridSize - 1.f;
                    float zOffset = 2.f * (z + 0.5f + offset_distr(gen)) / fGridSize - 1.f;
                    // Random scale
                    float scale = BaseScale * scale_distr(gen);
                    // Random rotation
                    float4x4 rotation = float4x4::RotationX(rot_distr(gen));
                    rotation *= float4x4::RotationY(rot_distr(gen));
                    rotation *= float4x4::RotationZ(rot_distr(gen));
                    // Combine rotation, scale and translation
                    float4x4 matrix = rotation * float4x4::Scale(scale, scale, scale) * float4x4::Translation(xOffset, yOffset, zOffset);

                    InstanceData &CurrInst = m_Instances[instId++];
                    CurrInst.Matrix = matrix;
                    // Texture array index
                    CurrInst.TextureInd = tex_distr(gen);
                }
            }
        }
    }

    void C6GERender::StartWorkerThreads(size_t NumThreads)
    {
        // If the engine did not provide enough deferred contexts, clamp the number of worker threads.
        if (NumThreads > m_pDeferredContexts.size())
        {
            // Reduce the requested number of threads to the available deferred contexts.
            NumThreads = m_pDeferredContexts.size();
            // Keep m_NumWorkerThreads consistent with the actual started threads.
            m_NumWorkerThreads = static_cast<int>(NumThreads);
            // Log a warning to help debugging.
            std::cerr << "[C6GE] Warning: Requested worker threads exceed available deferred contexts. Clamping to " << NumThreads << "\n";
        }

    // If engine did not provide any deferred contexts, do not start worker threads.
    std::cerr << "[C6GE] m_pDeferredContexts.size()=" << m_pDeferredContexts.size() << "\n";
    if (m_pDeferredContexts.empty())
    {
        std::cerr << "[C6GE] No deferred contexts available from engine; skipping worker thread start\n";
        m_NumWorkerThreads = 0;
        return;
    }

    // Collect indices of available (non-null) deferred contexts
    std::vector<Uint32> AvailableCtxIndices;
    for (Uint32 i = 0; i < m_pDeferredContexts.size(); ++i)
    {
        if (m_pDeferredContexts[i])
            AvailableCtxIndices.push_back(i);
        else
            std::cerr << "[C6GE] Warning: m_pDeferredContexts[" << i << "] is null\n";
    }

    if (AvailableCtxIndices.empty())
    {
        std::cerr << "[C6GE] No valid deferred contexts found; skipping worker thread start\n";
        m_NumWorkerThreads = 0;
        return;
    }

    // Clamp requested threads to number of available deferred contexts
    size_t ThreadsToStart = std::min(NumThreads, AvailableCtxIndices.size());
    std::cerr << "[C6GE] Starting " << ThreadsToStart << " worker threads (requested " << NumThreads << ")\n";

    // Reset thread counters before launching worker threads
    m_NumThreadsCompleted.store(0);
    m_NumThreadsReady.store(0);

    m_WorkerThreads.resize(ThreadsToStart);
    // Prepare non-owning raw pointers to deferred contexts for worker threads
    m_WorkerDeferredCtxs.clear();
    m_WorkerDeferredCtxs.resize(ThreadsToStart, nullptr);
    // Resize command list vector to match actual started threads
    m_CmdLists.resize(ThreadsToStart);

    for (Uint32 t = 0; t < ThreadsToStart; ++t)
    {
        Uint32 ctxIdx = AvailableCtxIndices[t];
        m_WorkerDeferredCtxs[t] = m_pDeferredContexts[ctxIdx];
    std::cerr << "[C6GE] Launching worker thread " << t << " mapped to deferred context index " << ctxIdx
          << " ptr=" << (void*)m_WorkerDeferredCtxs[t] << "\n";
        m_WorkerThreads[t] = std::thread(&C6GERender::WorkerThreadFunc, this, t);
    }
    // Keep m_NumWorkerThreads consistent
    m_NumWorkerThreads = static_cast<int>(ThreadsToStart);
    }

    void C6GERender::StopWorkerThreads()
    {
        // Ensure the render-subset signal is in a known state before triggering workers to exit.
        if (m_RenderSubsetSignal.IsTriggered())
        {
            std::cerr << "[C6GE] Warning: RenderSubsetSignal already triggered when stopping worker threads - resetting\n";
            m_RenderSubsetSignal.Reset();
        }
        m_RenderSubsetSignal.Trigger(true, -1);

        for (std::thread &thread : m_WorkerThreads)
        {
            if (thread.joinable())
            {
                std::cerr << "[C6GE] Joining worker thread\n";
                thread.join();
            }
        }
        m_RenderSubsetSignal.Reset();
        m_WorkerThreads.clear();
        m_CmdLists.clear();
        m_WorkerDeferredCtxs.clear();
        std::cerr << "[C6GE] All worker threads stopped\n";
    }

    void C6GERender::WorkerThreadFunc(C6GERender *pThis, Uint32 ThreadId)
    {
        // Every thread should use its own deferred context. Prefer the per-worker
        // raw pointers populated in StartWorkerThreads, fallback to the base class
        // deferred contexts vector if necessary.
        IDeviceContext *pDeferredCtx = nullptr;
        if (ThreadId < pThis->m_WorkerDeferredCtxs.size())
            pDeferredCtx = pThis->m_WorkerDeferredCtxs[ThreadId];
        if (!pDeferredCtx && ThreadId < pThis->m_pDeferredContexts.size())
            pDeferredCtx = pThis->m_pDeferredContexts[ThreadId];
        const int NumWorkerThreads = static_cast<int>(pThis->m_WorkerThreads.size());
        // If no deferred context is available, exit the thread cleanly.
        if (!pDeferredCtx)
        {
            std::cerr << "[C6GE] WorkerThread " << ThreadId << " has no deferred context and will exit\n";
            return;
        }
        std::cerr << "[C6GE] WorkerThread " << ThreadId << " started. deferredCtx=" << (void*)pDeferredCtx << "\n";
        for (;;)
        {
            // Wait for the signal
            std::cerr << "[C6GE] WorkerThread " << ThreadId << " waiting on RenderSubsetSignal\n";
            int SignaledValue = pThis->m_RenderSubsetSignal.Wait(true, NumWorkerThreads);
            std::cerr << "[C6GE] WorkerThread " << ThreadId << " woke with SignaledValue=" << SignaledValue << "\n";
            if (SignaledValue < 0)
                return;

            pDeferredCtx->Begin(0);
            std::cerr << "[C6GE] WorkerThread " << ThreadId << " Begin() called\n";

            // Render current subset using the deferred context
            pThis->RenderSubset(pDeferredCtx, 1 + ThreadId);

            // Finish command list
            RefCntAutoPtr<ICommandList> pCmdList;
            pDeferredCtx->FinishCommandList(&pCmdList);
            pThis->m_CmdLists[ThreadId] = pCmdList;

            {
                // Atomically increment the number of completed threads
                const int NumThreadsCompleted = pThis->m_NumThreadsCompleted.fetch_add(1) + 1;
                if (NumThreadsCompleted == NumWorkerThreads)
                    pThis->m_ExecuteCommandListsSignal.Trigger();
            }

            pThis->m_GotoNextFrameSignal.Wait(true, NumWorkerThreads);

            // Call FinishFrame() to release dynamic resources allocated by deferred contexts
            // IMPORTANT: we must wait until the command lists are submitted for execution
            //            because FinishFrame() invalidates all dynamic resources.
            // IMPORTANT: In Metal backend FinishFrame must be called from the same
            //            thread that issued rendering commands.
            pDeferredCtx->FinishFrame();
            std::cerr << "[C6GE] WorkerThread " << ThreadId << " FinishFrame() called\n";

            pThis->m_NumThreadsReady.fetch_add(1);
                std::cerr << "[C6GE] WorkerThread " << ThreadId << " incremented NumThreadsReady\n";
            // We must wait until all threads reach this point, because
            // m_GotoNextFrameSignal must be unsignaled before we proceed to
            // RenderSubsetSignal to avoid one thread going through the loop twice in
            // a row.
            while (pThis->m_NumThreadsReady.load() < NumWorkerThreads)
                std::this_thread::yield();
            VERIFY_EXPR(!pThis->m_GotoNextFrameSignal.IsTriggered());
        }
    }

    void C6GERender::RenderSubset(IDeviceContext *pCtx, Uint32 Subset)
    {
        // Deferred contexts start in default state. We must bind everything to the context.
        // Render targets are set and transitioned to correct states by the main thread, here we only verify the states.
        ITextureView *pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
        // Use TRANSITION mode so deferred contexts can transition the back buffer and depth
        // from COPY_DEST (upload) to RENDER_TARGET/DEPTH_WRITE as needed.
        pCtx->SetRenderTargets(1, &pRTV, m_pSwapChain->GetDepthBufferDSV(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        {
            // Map the buffer and write current world-view-projection matrix

            // Since this is a dynamic buffer, it must be mapped in every context before
            // it can be used even though the matrices are the same.
            MapHelper<float4x4> CBConstants(pCtx, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
            CBConstants[0] = m_ViewProjMatrix;
            CBConstants[1] = m_RotationMatrix;
        }

        // Bind vertex and index buffers. This must be done for every context
        IBuffer *pBuffs[] = {m_CubeVertexBuffer};
        pCtx->SetVertexBuffers(0, _countof(pBuffs), pBuffs, nullptr, RESOURCE_STATE_TRANSITION_MODE_VERIFY, SET_VERTEX_BUFFERS_FLAG_RESET);
        pCtx->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_VERIFY);

        DrawIndexedAttribs DrawAttrs;    // This is an indexed draw call
        DrawAttrs.IndexType = VT_UINT32; // Index type
        DrawAttrs.NumIndices = 36;
        DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;

        // Set the pipeline state
        pCtx->SetPipelineState(m_pPSO);
        Uint32 NumSubsets = Uint32{1} + static_cast<Uint32>(m_WorkerThreads.size());
        Uint32 NumInstances = static_cast<Uint32>(m_Instances.size());
        Uint32 SusbsetSize = NumInstances / NumSubsets;
        Uint32 StartInst = SusbsetSize * Subset;
        Uint32 EndInst = (Subset < NumSubsets - 1) ? SusbsetSize * (Subset + 1) : NumInstances;
        for (size_t inst = StartInst; inst < EndInst; ++inst)
        {
            const InstanceData &CurrInstData = m_Instances[inst];
            // Shader resources have been explicitly transitioned to correct states, so
            // RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode is not needed.
            // Instead, we use RESOURCE_STATE_TRANSITION_MODE_VERIFY mode to
            // verify that all resources are in correct states. This mode only has effect
            // in debug and development builds.
            // Allow automatic transitions if the texture is still in COPY_DEST state
            // (e.g., just uploaded). TRANSITION will change the resource state to SHADER_RESOURCE.
            pCtx->CommitShaderResources(m_SRB[CurrInstData.TextureInd], RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            {
                // Map the buffer and write current world-view-projection matrix
                MapHelper<float4x4> InstData(pCtx, m_InstanceConstants, MAP_WRITE, MAP_FLAG_DISCARD);
                if (InstData == nullptr)
                {
                    LOG_ERROR_MESSAGE("Failed to map instance data buffer");
                    break;
                }
                *InstData = CurrInstData.Matrix;
            }

            pCtx->DrawIndexed(DrawAttrs);
        }
    }

    void C6GERender::ResizeFramebuffer(Uint32 Width, Uint32 Height)
    {

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

        // Render to the off-screen framebuffer for the ImGui viewport
        ITextureView *pRTV = m_pFramebufferRTV;
        ITextureView *pDSV = m_pFramebufferDSV;
        float4 ClearColor = {0.350f, 0.350f, 0.350f, 1.0f};
        if (m_ConvertPSOutputToGamma)
        {
            ClearColor = LinearToSRGB(ClearColor);
        }
        m_pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        {
            MapHelper<float4x4> CBConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
            CBConstants[0] = m_ViewProjMatrix;
            CBConstants[1] = m_RotationMatrix;
        }

        const Uint64 offsets[] = {0, 0};
        IBuffer *pBuffs[] = {m_CubeVertexBuffer, m_InstanceBuffer};

        m_pImmediateContext->SetVertexBuffers(0, (sizeof(pBuffs) / sizeof(pBuffs[0])), pBuffs, offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
        m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        m_pImmediateContext->SetPipelineState(m_pPSO);
        m_pImmediateContext->CommitShaderResources(m_SRB[0].RawPtr(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        DrawIndexedAttribs DrawAttrs;
        DrawAttrs.IndexType = VT_UINT32;
        DrawAttrs.NumIndices = 36;
        DrawAttrs.NumInstances = m_GridSize * m_GridSize * m_GridSize;
        DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;

        // Map the dynamic instance constant buffer per-frame to ensure Vulkan backend
        // allocates and updates dynamic buffer memory for this frame.
        if (m_InstanceConstants)
        {
            MapHelper<float4x4> PerFrameInstCB(m_pImmediateContext, m_InstanceConstants, MAP_WRITE, MAP_FLAG_DISCARD);
            // The actual instance transform will be set in RenderSubset for deferred contexts.
            // For the immediate context (main thread) we can leave it as identity or reuse current rotation.
            *PerFrameInstCB = m_RotationMatrix;
        }

        m_pImmediateContext->DrawIndexed(DrawAttrs);

        ITextureView *pBackBufferRTV = m_pSwapChain->GetCurrentBackBufferRTV();
        ITextureView *pBackBufferDSV = m_pSwapChain->GetDepthBufferDSV();
        // Clear the back buffer
        float4 BackClearColor = {0.350f, 0.350f, 0.350f, 1.0f};
        if (m_ConvertPSOutputToGamma)
        {
            // If manual gamma correction is required, we need to clear the render target with sRGB color
            BackClearColor = LinearToSRGB(BackClearColor);
        }
        // Bind the swap-chain back buffer and depth-stencil view before clearing.
        m_pImmediateContext->SetRenderTargets(1, &pBackBufferRTV, pBackBufferDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearRenderTarget(pBackBufferRTV, BackClearColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearDepthStencil(pBackBufferDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        if (!m_WorkerThreads.empty())
        {
            // Avoid double-triggering the signal; Trigger will ASSERT when the signal
            // value is non-zero or threads haven't been awakened and the signal wasn't reset.
            if (m_RenderSubsetSignal.IsTriggered())
            {
                std::cerr << "[C6GE] Warning: RenderSubsetSignal already triggered, skipping Trigger() this frame\n";
            }
            else
            {
                m_NumThreadsCompleted.store(0);
                m_RenderSubsetSignal.Trigger(true);
            }
        }

        RenderSubset(m_pImmediateContext, 0);

        if (!m_WorkerThreads.empty())
        {
            m_ExecuteCommandListsSignal.Wait(true, 1);

            m_CmdListPtrs.resize(m_CmdLists.size());
            for (Uint32 i = 0; i < m_CmdLists.size(); ++i)
                m_CmdListPtrs[i] = m_CmdLists[i];

            m_pImmediateContext->ExecuteCommandLists(static_cast<Uint32>(m_CmdListPtrs.size()), m_CmdListPtrs.data());

            for (auto &cmdList : m_CmdLists)
            {
                // Release command lists now to release all outstanding references.
                // In d3d11 mode, command lists hold references to the swap chain's back buffer
                // that cause swap chain resize to fail.
                cmdList.Release();
            }

            m_NumThreadsReady.store(0);
            m_GotoNextFrameSignal.Trigger(true);
        }
    }

    void C6GERender::Update(double CurrTime, double ElapsedTime, bool DoUpdateUI)
    {
        SampleBase::Update(CurrTime, ElapsedTime, DoUpdateUI);

        // Only update scene if playing
        if (!IsPlaying())
            return;

        // Set the cube view matrix
        float4x4 View = float4x4::RotationX(-0.6f) * float4x4::Translation(0.f, 0.f, 4.0f);

        // Get pretransform matrix that rotates the scene according the surface orientation
        float4x4 SrfPreTransform = GetSurfacePretransformMatrix(float3{0, 0, 1});

        // Get projection matrix adjusted to the current screen orientation
        float4x4 Proj = GetAdjustedProjectionMatrix(PI_F / 4.0f, 0.1f, 100.f);

        // Compute view-projection matrix
        m_ViewProjMatrix = View * SrfPreTransform * Proj;

        // Global rotation matrix
        m_RotationMatrix = float4x4::RotationY(static_cast<float>(CurrTime) * 1.0f) * float4x4::RotationX(-static_cast<float>(CurrTime) * 0.25f);
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

        // Resize framebuffer if dimensions changed
        if (Width != m_FramebufferWidth || Height != m_FramebufferHeight)
        {
            m_FramebufferWidth = Width;
            m_FramebufferHeight = Height;

            // Release old framebuffer resources
            m_pFramebufferTexture.Release();
            m_pFramebufferRTV.Release();
            m_pFramebufferSRV.Release();
            m_pFramebufferDepth.Release();
            m_pFramebufferDSV.Release();

            // Recreate framebuffer with new dimensions
            CreateFramebuffer();
        }
    }

} // namespace Diligent