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

        CreatePipelineStates();

        CreateInstanceBuffer();
        LoadTextures();

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

    void C6GERender::CreatePipelineStates()
    {
        // clang-format off
        // Define vertex shader input layout
        // This tutorial uses two types of input: per-vertex data and per-instance data.
        LayoutElement LayoutElems[] =
        {
            // Per-vertex data - first buffer slot
            // Attribute 0 - vertex position
            LayoutElement{0, 0, 3, VT_FLOAT32, False},
            // Attribute 1 - texture coordinates
            LayoutElement{1, 0, 2, VT_FLOAT32, False},

            // Per-instance data - second buffer slot
            // We will use four attributes to encode instance-specific 4x4 transformation matrix
            // Attribute 2 - first row
            LayoutElement{2, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
            // Attribute 3 - second row
            LayoutElement{3, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
            // Attribute 4 - third row
            LayoutElement{4, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
            // Attribute 5 - fourth row
            LayoutElement{5, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
            // Attribute 6 - texture array index
            LayoutElement{6, 1, 1, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
        };
        // clang-format on

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
        CubePsoCI.ExtraLayoutElements = LayoutElems;
        CubePsoCI.NumExtraLayoutElements = (sizeof(LayoutElems) / sizeof(LayoutElems[0]));

        m_pPSO = TexturedCube::CreatePipelineState(CubePsoCI, m_ConvertPSOutputToGamma);

        // Create dynamic uniform buffer that will store our transformation matrix
        // Dynamic buffers can be frequently updated by the CPU
        CreateUniformBuffer(m_pDevice, sizeof(float4x4) * 2, "VS constants CB", &m_VSConstants);

        // Since we did not explicitly specify the type for 'Constants' variable, default
        // type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables
        // never change and are bound directly to the pipeline state object.
        m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);

        // Since we are using mutable variable, we must create a shader resource binding object
        // http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/
        m_pPSO->CreateShaderResourceBinding(&m_pSRB, true);
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

    void C6GERender::LoadTextures()
    {
        std::vector<RefCntAutoPtr<ITextureLoader>> TexLoaders(NumTextures);
        // Load textures
        for (int tex = 0; tex < NumTextures; ++tex)
        {
            // Create loader for the current texture
            std::stringstream FileNameSS;
            FileNameSS << "C6GELogo" << tex << ".png";
            const auto FileName = FileNameSS.str();
            TextureLoadInfo LoadInfo;
            LoadInfo.IsSRGB = true;

            CreateTextureLoaderFromFile(FileName.c_str(), IMAGE_FILE_FORMAT_UNKNOWN, LoadInfo, &TexLoaders[tex]);
            VERIFY_EXPR(TexLoaders[tex]);
            VERIFY(tex == 0 || TexLoaders[tex]->GetTextureDesc() == TexLoaders[0]->GetTextureDesc(), "All textures must be same size");
        }

        TextureDesc TexArrDesc = TexLoaders[0]->GetTextureDesc();
        TexArrDesc.ArraySize = NumTextures;
        TexArrDesc.Type = RESOURCE_DIM_TEX_2D_ARRAY;
        TexArrDesc.Usage = USAGE_DEFAULT;
        TexArrDesc.BindFlags = BIND_SHADER_RESOURCE;

        // Prepare initialization data
        std::vector<TextureSubResData> SubresData(TexArrDesc.ArraySize * TexArrDesc.MipLevels);
        for (Uint32 slice = 0; slice < TexArrDesc.ArraySize; ++slice)
        {
            for (Uint32 mip = 0; mip < TexArrDesc.MipLevels; ++mip)
            {
                SubresData[slice * TexArrDesc.MipLevels + mip] = TexLoaders[slice]->GetSubresourceData(mip, 0);
            }
        }
        TextureData InitData{SubresData.data(), TexArrDesc.MipLevels * TexArrDesc.ArraySize};

        // Create the texture array
        RefCntAutoPtr<ITexture> pTexArray;
        m_pDevice->CreateTexture(TexArrDesc, &InitData, &pTexArray);

        // Get shader resource view from the texture array
        m_TextureSRV = pTexArray->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        // Set texture SRV in the SRB
        m_pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TextureSRV);
    }

    void C6GERender::PopulateInstanceBuffer()
    {
        // Populate instance data buffer
        const size_t zGridSize = static_cast<size_t>(m_GridSize);
        std::vector<InstanceData> Instances(zGridSize * zGridSize * zGridSize);

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

                    InstanceData &CurrInst = Instances[instId++];
                    CurrInst.Matrix = matrix;
                    // Texture array index
                    CurrInst.TextureInd = static_cast<float>(tex_distr(gen));
                }
            }
        }
        // Update instance data buffer
        Uint32 DataSize = static_cast<Uint32>(sizeof(Instances[0]) * Instances.size());
        m_pImmediateContext->UpdateBuffer(m_InstanceBuffer, 0, DataSize, Instances.data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
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
        m_pImmediateContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        DrawIndexedAttribs DrawAttrs;
        DrawAttrs.IndexType = VT_UINT32;
        DrawAttrs.NumIndices = 36;
        DrawAttrs.NumInstances = m_GridSize * m_GridSize * m_GridSize;
        DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
        m_pImmediateContext->DrawIndexed(DrawAttrs);
    }

    void C6GERender::Update(double CurrTime, double ElapsedTime, bool DoUpdateUI)
    {
        SampleBase::Update(CurrTime, ElapsedTime, DoUpdateUI);

        // Only update scene if playing
        if (!IsPlaying())
            return;

        // Set cube view matrix
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