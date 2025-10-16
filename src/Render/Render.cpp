#include <atomic>

namespace Diligent {
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

    void Diligent::C6GERender::TogglePlayState() {
        if (playState == PlayState::Paused) playState = PlayState::Playing;
        else playState = PlayState::Paused;
    }
    extern bool RenderSettingsOpen;

    SampleBase *CreateSample()
    {
        return new C6GERender();
    }

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

        CreateFramebuffer();

        CreateInstanceBuffer();
    }

    void C6GERender::UpdateUI()
    {
        if(IsRuntime == true)
            return;

        try {
            if (!ImGui::GetCurrentContext()) {
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
            if (ImGui::FindWindowByName("Viewport")) {
                ImGuiWindow* vpWin = ImGui::FindWindowByName("Viewport");
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
            if (ImGui::FindWindowByName("PlayPauseBar")) {
                ImGui::BringWindowToDisplayFront(ImGui::FindWindowByName("PlayPauseBar"));
            }

            bool usedIcon = false;
            const float iconSize = 40.0f;
            if (m_PlayIconSRV && m_PauseIconSRV) {
                auto playSRV = m_PlayIconSRV.RawPtr();
                auto pauseSRV = m_PauseIconSRV.RawPtr();
                if (playSRV && pauseSRV) {
                    ImTextureRef icon = IsPaused() ? ImTextureRef(playSRV) : ImTextureRef(pauseSRV);
                    if (ImGui::ImageButton("##playpause", icon, ImVec2(iconSize, iconSize))) {
                        TogglePlayState();
                    }
                    usedIcon = true;
                }
            }
            if (!usedIcon) {
                // Fallback: use text button
                if (ImGui::Button(IsPaused() ? "Play" : "Pause", ImVec2(iconSize * 2, iconSize))) {
                    TogglePlayState();
                }
            }
            ImGui::End();
            ImGui::PopStyleVar(3);
        } catch (const std::exception& e) {
            std::cerr << "[C6GE] Exception in UpdateUI play/pause bar: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "[C6GE] Unknown exception in UpdateUI play/pause bar." << std::endl;
        }
    
    // Remove fixed positioning - let the window be freely movable and resizable

    if(RenderSettingsOpen) {
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
    auto* viewport = ImGui::GetMainViewport();
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
                        ImGui::ColorEdit4("Background Color", (float*)&bg_color);
                        ImGui::ColorEdit4("Text Color", (float*)&text_color);
                        if (ImGui::Button("Apply", ImVec2(0,0)))
                        {
                            ImGui::GetStyle().Colors[ImGuiCol_WindowBg] = bg_color;
                            ImGui::GetStyle().Colors[ImGuiCol_Text] = text_color;
                            auto& io = ImGui::GetIO();
                            io.FontGlobalScale = font_size / 16.0f;
                        }
                        if (ImGui::Button("Open Render Settings", ImVec2(0,0)))
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
                    reinterpret_cast<void*>(m_pFramebufferSRV.RawPtr()),
                    contentSize
                );
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

    void C6GERender::CreateInstanceBuffer()
    {
    // Create instance data buffer that will store transformation matrices
    BufferDesc InstBuffDesc;
    InstBuffDesc.Name = "Instance data buffer";
    // Use default usage as this buffer will only be updated when grid size changes
    InstBuffDesc.Usage     = USAGE_DEFAULT;
    InstBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
    InstBuffDesc.Size      = sizeof(float4x4) * MaxInstances;
    m_pDevice->CreateBuffer(InstBuffDesc, nullptr, &m_InstanceBuffer);
    PopulateInstanceBuffer();
    }

    void C6GERender::CreatePipelineStates()
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

        // In this tutorial, we will load shaders from file. To be able to do that,
        // we need to create a shader source stream factory
        RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
        m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
        // Create a vertex shader
        RefCntAutoPtr<IShader> pVS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
            ShaderCI.EntryPoint = "main";
            ShaderCI.Desc.Name = "Cube VS";
            ShaderCI.FilePath = "cube.vsh";
            m_pDevice->CreateShader(ShaderCI, &pVS);
            // Create dynamic uniform buffer that will store our transformation matrix
            // Dynamic buffers can be frequently updated by the CPU
            BufferDesc CBDesc;
            CBDesc.Name = "VS constants CB";
            CBDesc.Size = sizeof(float4x4) * 2; // Need space for ViewProj and Rotation matrices
            CBDesc.Usage = USAGE_DYNAMIC;
            CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
            CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
            m_pDevice->CreateBuffer(CBDesc, nullptr, &m_VSConstants);
        }

        // Create a pixel shader
        RefCntAutoPtr<IShader> pPS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
            ShaderCI.EntryPoint = "main";
            ShaderCI.Desc.Name = "Cube PS";
            ShaderCI.FilePath = "cube.psh";
            m_pDevice->CreateShader(ShaderCI, &pPS);

            ShaderResourceVariableDesc Vars[] = 
            {
                {SHADER_TYPE_PIXEL, "g_Texture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
            };
            PSOCreateInfo.PSODesc.ResourceLayout.Variables    = Vars;
            PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = sizeof(Vars)/sizeof(Vars[0]);
        }

        SamplerDesc SamLinearClampDesc
        {
            FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, 
            TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP
        };
        ImmutableSamplerDesc ImtblSamplers[] = 
        {
            {SHADER_TYPE_PIXEL, "g_Texture", SamLinearClampDesc}
        };
        PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers    = ImtblSamplers;
        PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = sizeof(ImtblSamplers)/sizeof(ImtblSamplers[0]);

    // clang-format off
    // Define vertex shader input layout
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
    LayoutElement{5, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE}
};
        // clang-format on
        PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
        PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = sizeof(LayoutElems)/sizeof(LayoutElems[0]);

        PSOCreateInfo.pVS = pVS;
        PSOCreateInfo.pPS = pPS;

        // Define variable type that will be used by default
        PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

        m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPSO);

        // Since we did not explicitly specify the type for 'Constants' variable, default
        // type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables never
        // change and are bound directly through the pipeline state object.
        m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);

        // Load Texture
        TextureLoadInfo loadInfo;
        loadInfo.IsSRGB = true;
        RefCntAutoPtr<ITexture> Tex;
        CreateTextureFromFile("C6GELogo.png", loadInfo, m_pDevice, &Tex);

        // Get shader resource view from the texture
        m_TextureSRV = Tex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

        // Create a shader resource binding object and bind all static resources in it
        m_pPSO->CreateShaderResourceBinding(&m_pSRB, true);

        // Bind the texture to the shader resource binding
        m_pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TextureSRV);

        // Layout of this structure matches the one we defined in the pipeline state
        struct Vertex
        {
            float3 pos;
            float2 uv;
        };

        // Cube vertices

        //      (-1,+1,+1)________________(+1,+1,+1)
        //               /|              /|
        //              / |             / |
        //             /  |            /  |
        //            /   |           /   |
        //(-1,-1,+1) /____|__________/(+1,-1,+1)
        //           |    |__________|____|
        //           |   /(-1,+1,-1) |    /(+1,+1,-1)
        //           |  /            |   /
        //           | /             |  /
        //           |/              | /
        //           /_______________|/
        //        (-1,-1,-1)       (+1,-1,-1)
        //

        constexpr Vertex CubeVerts[24] =
            {
                // Front face
                {float3{-1, -1, +1}, float2{0, 1}}, // Bottom-left
                {float3{+1, -1, +1}, float2{1, 1}}, // Bottom-right  
                {float3{+1, +1, +1}, float2{1, 0}}, // Top-right
                {float3{-1, +1, +1}, float2{0, 0}}, // Top-left
                
                // Back face
                {float3{+1, -1, -1}, float2{0, 1}}, // Bottom-left
                {float3{-1, -1, -1}, float2{1, 1}}, // Bottom-right
                {float3{-1, +1, -1}, float2{1, 0}}, // Top-right
                {float3{+1, +1, -1}, float2{0, 0}}, // Top-left
                
                // Left face
                {float3{-1, -1, -1}, float2{0, 1}}, // Bottom-left
                {float3{-1, -1, +1}, float2{1, 1}}, // Bottom-right
                {float3{-1, +1, +1}, float2{1, 0}}, // Top-right
                {float3{-1, +1, -1}, float2{0, 0}}, // Top-left
                
                // Right face
                {float3{+1, -1, +1}, float2{0, 1}}, // Bottom-left
                {float3{+1, -1, -1}, float2{1, 1}}, // Bottom-right
                {float3{+1, +1, -1}, float2{1, 0}}, // Top-right
                {float3{+1, +1, +1}, float2{0, 0}}, // Top-left
                
                // Top face
                {float3{-1, +1, +1}, float2{0, 1}}, // Bottom-left
                {float3{+1, +1, +1}, float2{1, 1}}, // Bottom-right
                {float3{+1, +1, -1}, float2{1, 0}}, // Top-right
                {float3{-1, +1, -1}, float2{0, 0}}, // Top-left
                
                // Bottom face
                {float3{-1, -1, -1}, float2{0, 1}}, // Bottom-left
                {float3{+1, -1, -1}, float2{1, 1}}, // Bottom-right
                {float3{+1, -1, +1}, float2{1, 0}}, // Top-right
                {float3{-1, -1, +1}, float2{0, 0}}, // Top-left
            };

        // Create a vertex buffer that stores cube vertices
        BufferDesc VertBuffDesc;
        VertBuffDesc.Name = "Cube vertex buffer";
        VertBuffDesc.Usage = USAGE_IMMUTABLE;
        VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
        VertBuffDesc.Size = sizeof(CubeVerts);
        BufferData VBData;
        VBData.pData = CubeVerts;
        VBData.DataSize = sizeof(CubeVerts);
        m_pDevice->CreateBuffer(VertBuffDesc, &VBData, &m_CubeVertexBuffer);

    // clang-format off
    constexpr Uint32 Indices[] =
    {
        // Front face
        0, 1, 2,  0, 2, 3,
        // Back face  
        4, 5, 6,  4, 6, 7,
        // Left face
        8, 9, 10,  8, 10, 11,
        // Right face
        12, 13, 14,  12, 14, 15,
        // Top face
        16, 17, 18,  16, 18, 19,
        // Bottom face
        20, 21, 22,  20, 22, 23
    };
        // clang-format on

        BufferDesc IndBuffDesc;
        IndBuffDesc.Name = "Cube index buffer";
        IndBuffDesc.Usage = USAGE_IMMUTABLE;
        IndBuffDesc.BindFlags = BIND_INDEX_BUFFER;
        IndBuffDesc.Size = sizeof(Indices);
        BufferData IBData;
        IBData.pData = Indices;
        IBData.DataSize = sizeof(Indices);
        m_pDevice->CreateBuffer(IndBuffDesc, &IBData, &m_CubeIndexBuffer);
    }

    void C6GERender::CreateFramebuffer()
    {
        // Create render target texture
        TextureDesc RTTexDesc;
        RTTexDesc.Name = "Framebuffer render target";
        RTTexDesc.Type = RESOURCE_DIM_TEX_2D;
        RTTexDesc.Width = m_FramebufferWidth;
        RTTexDesc.Height = m_FramebufferHeight;
        RTTexDesc.Format = m_pSwapChain->GetDesc().ColorBufferFormat; // Match swap chain format
        RTTexDesc.Usage = USAGE_DEFAULT;
        RTTexDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
        RTTexDesc.MipLevels = 1;
        RTTexDesc.SampleCount = 1;

        m_pDevice->CreateTexture(RTTexDesc, nullptr, &m_pFramebufferTexture);

        // Create render target view
        TextureViewDesc RTVDesc;
        RTVDesc.ViewType = TEXTURE_VIEW_RENDER_TARGET;
        RTVDesc.Format = RTTexDesc.Format;
        m_pFramebufferTexture->CreateView(RTVDesc, &m_pFramebufferRTV);

        // Create shader resource view
        TextureViewDesc SRVDesc;
        SRVDesc.ViewType = TEXTURE_VIEW_SHADER_RESOURCE;
        SRVDesc.Format = RTTexDesc.Format;
        m_pFramebufferTexture->CreateView(SRVDesc, &m_pFramebufferSRV);

        // Create depth buffer texture
        TextureDesc DepthTexDesc;
        DepthTexDesc.Name = "Framebuffer depth buffer";
        DepthTexDesc.Type = RESOURCE_DIM_TEX_2D;
        DepthTexDesc.Width = m_FramebufferWidth;
        DepthTexDesc.Height = m_FramebufferHeight;
        DepthTexDesc.Format = m_pSwapChain->GetDesc().DepthBufferFormat; // Match swap chain depth format
        DepthTexDesc.Usage = USAGE_DEFAULT;
        DepthTexDesc.BindFlags = BIND_DEPTH_STENCIL;
        DepthTexDesc.MipLevels = 1;
        DepthTexDesc.SampleCount = 1;

        m_pDevice->CreateTexture(DepthTexDesc, nullptr, &m_pFramebufferDepth);

        // Create depth-stencil view
        TextureViewDesc DSVDesc;
        DSVDesc.ViewType = TEXTURE_VIEW_DEPTH_STENCIL;
        DSVDesc.Format = DepthTexDesc.Format;
        m_pFramebufferDepth->CreateView(DSVDesc, &m_pFramebufferDSV);
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

    void C6GERender::PopulateInstanceBuffer()
    {
    // Populate instance data buffer
    const size_t          zGridSize = static_cast<size_t>(m_GridSize);
    std::vector<float4x4> InstanceData(zGridSize * zGridSize * zGridSize);

    float fGridSize = static_cast<float>(m_GridSize);

    std::mt19937 gen; // Standard mersenne_twister_engine. Use default seed
                      // to generate consistent distribution.

    std::uniform_real_distribution<float> scale_distr(0.3f, 1.0f);
    std::uniform_real_distribution<float> offset_distr(-0.15f, +0.15f);
    std::uniform_real_distribution<float> rot_distr(-PI_F, +PI_F);

    float BaseScale = 0.6f / fGridSize;
    int   instId    = 0;
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
                float4x4 matrix        = rotation * float4x4::Scale(scale, scale, scale) * float4x4::Translation(xOffset, yOffset, zOffset);
                InstanceData[instId++] = matrix;
            }
        }
    }
    // Update instance data buffer
    Uint32 DataSize = static_cast<Uint32>(sizeof(InstanceData[0]) * InstanceData.size());
    m_pImmediateContext->UpdateBuffer(m_InstanceBuffer, 0, DataSize, InstanceData.data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    // Render a frame
    void C6GERender::Render()
    {
    // Render to framebuffer instead of swap chain
    ITextureView* pRTV = m_pFramebufferRTV;
    ITextureView* pDSV = m_pFramebufferDSV;
    
    // Set the render targets BEFORE clearing them to avoid warnings
    m_pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    // Clear the framebuffer
    float4 ClearColor = {0.350f, 0.350f, 0.350f, 1.0f};
    if (m_ConvertPSOutputToGamma)
    {
        // If manual gamma correction is required, we need to clear the render target with sRGB color
        ClearColor = LinearToSRGB(ClearColor);
    }
    m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
        // Map the buffer and write current world-view-projection matrix
        MapHelper<float4x4> CBConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        CBConstants[0] = m_ViewProjMatrix;
        CBConstants[1] = m_RotationMatrix;
    }

    // Bind vertex, instance and index buffers
    const Uint64 offsets[] = {0, 0};
    IBuffer*     pBuffs[]  = {m_CubeVertexBuffer, m_InstanceBuffer};
    m_pImmediateContext->SetVertexBuffers(0, sizeof(pBuffs)/sizeof(pBuffs[0]), pBuffs, offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Set the pipeline state
    m_pImmediateContext->SetPipelineState(m_pPSO);
    // Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode
    // makes sure that resources are transitioned to required states.
    m_pImmediateContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawIndexedAttribs DrawAttrs;       // This is an indexed draw call
    DrawAttrs.IndexType    = VT_UINT32; // Index type
    DrawAttrs.NumIndices   = 36;
    DrawAttrs.NumInstances = m_GridSize * m_GridSize * m_GridSize; // The number of instances
    // Verify the state of vertex and index buffers
    DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->DrawIndexed(DrawAttrs);
    }

    void C6GERender::DrawMesh(IDeviceContext *pCtx, bool bIsShadowPass, const ViewFrustumExt &Frustum)
    {
    }

    void C6GERender::Update(double CurrTime, double ElapsedTime, bool DoUpdateUI)
    {
    SampleBase::Update(CurrTime, ElapsedTime, DoUpdateUI);

    // Only update scene if playing
    if (!IsPlaying()) return;

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
