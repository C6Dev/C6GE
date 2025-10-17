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
#include <cstddef>

#ifndef _countof
#define _countof(arr) (sizeof(arr)/sizeof(arr[0]))
#endif
#include "MapHelper.hpp"
#include "FileSystem.hpp"
#include "ShaderMacroHelper.hpp"
#include "CommonlyUsedStates.h"
#include "StringTools.hpp"
#include "GraphicsUtilities.h"
#include "GraphicsAccessories.hpp"
#include "AdvancedMath.hpp"
#include "../../external/imgui/imgui.h"
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

using namespace Diligent;

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

    void C6GERender::CreateCubePSO() 
    {
        // Create a shader source stream factory to load shaders from files.
        RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
        m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);

        TexturedCube::CreatePSOInfo CubePsoCI;
        CubePsoCI.pDevice              = m_pDevice;
        CubePsoCI.RTVFormat            = m_pSwapChain->GetDesc().ColorBufferFormat;
        CubePsoCI.DSVFormat            = m_pSwapChain->GetDesc().DepthBufferFormat;
        CubePsoCI.pShaderSourceFactory = pShaderSourceFactory;
        CubePsoCI.VSFilePath           = "cube.vsh";
        CubePsoCI.PSFilePath           = "cube.psh";
        CubePsoCI.Components           = GEOMETRY_PRIMITIVE_VERTEX_FLAG_ALL;

    m_pCubePSO = TexturedCube::CreatePipelineState(CubePsoCI, m_ConvertPSOutputToGamma);

    // Since we did not explicitly specify the type for 'Constants' variable, default
    // type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables never
    // change and are bound directly through the pipeline state object.
    m_pCubePSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);

    // Since we are using mutable variable, we must create a shader resource binding object
    // http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/
    m_pCubePSO->CreateShaderResourceBinding(&m_CubeSRB, true);


    // Create shadow pass PSO
    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    PSOCreateInfo.PSODesc.Name = "Cube shadow PSO";

    // This is a graphics pipeline
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    // clang-format off
    // Shadow pass doesn't use any render target outputs
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets             = 0;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                = TEX_FORMAT_UNKNOWN;
    // The DSV format is the shadow map format
    PSOCreateInfo.GraphicsPipeline.DSVFormat                    = m_ShadowMapFormat;
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    // Cull back faces
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_BACK;
    // Enable depth testing
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;
    // clang-format on

    ShaderCreateInfo ShaderCI;
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    // Tell the system that the shader source code is in HLSL.
    // For OpenGL, the engine will convert this into GLSL under the hood.
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
    ShaderCI.Desc.UseCombinedTextureSamplers = true;
    // Pack matrices in row-major order
    ShaderCI.CompileFlags = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

    // Create shadow vertex shader
    RefCntAutoPtr<IShader> pShadowVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Cube Shadow VS";
        ShaderCI.FilePath        = "cube_shadow.vsh";
        m_pDevice->CreateShader(ShaderCI, &pShadowVS);
    }
    PSOCreateInfo.pVS = pShadowVS;

    // We don't use pixel shader as we are only interested in populating the depth buffer
    PSOCreateInfo.pPS = nullptr;

    // clang-format off
    // Define vertex shader input layout
    LayoutElement LayoutElems[] =
    {
        // Attribute 0 - vertex position
        LayoutElement{0, 0, 3, VT_FLOAT32, False},
        // Attribute 1 - normal
        LayoutElement{2, 0, 3, VT_FLOAT32, False},
        // Attribute 2 - texture coordinates
        LayoutElement{1, 0, 2, VT_FLOAT32, False}
    };
    // clang-format on

    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
        PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements    = sizeof(LayoutElems)/sizeof(LayoutElems[0]);

    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    if (m_pDevice->GetDeviceInfo().Features.DepthClamp)
    {
        // Disable depth clipping to render objects that are closer than near
        // clipping plane. This is not required for this tutorial, but real applications
        // will most likely want to do this.
        PSOCreateInfo.GraphicsPipeline.RasterizerDesc.DepthClipEnable = False;
    }

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pCubeShadowPSO);
    m_pCubeShadowPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);
    m_pCubeShadowPSO->CreateShaderResourceBinding(&m_CubeShadowSRB, true);
    }

    void C6GERender::CreatePlanePSO() 
    {
        GraphicsPipelineStateCreateInfo PSOCreateInfo;

    // Pipeline state name is used by the engine to report issues.
    // It is always a good idea to give objects descriptive names.
    PSOCreateInfo.PSODesc.Name = "Plane PSO";

    // This is a graphics pipeline
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    // clang-format off
    // This tutorial renders to a single render target
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets             = 1;
    // Set render target format which is the format of the swap chain's color buffer
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                = m_pSwapChain->GetDesc().ColorBufferFormat;
    // Set depth buffer format which is the format of the swap chain's back buffer
    PSOCreateInfo.GraphicsPipeline.DSVFormat                    = m_pSwapChain->GetDesc().DepthBufferFormat;
    // Primitive topology defines what kind of primitives will be rendered by this pipeline state
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    // No cull
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
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
        ShaderCI.Macros      = {Macros, sizeof(Macros)/sizeof(Macros[0])};

    // Create a shader source stream factory to load shaders from files.
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    // Create plane vertex shader
    RefCntAutoPtr<IShader> pPlaneVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Plane VS";
        ShaderCI.FilePath        = "plane.vsh";
        m_pDevice->CreateShader(ShaderCI, &pPlaneVS);
    }

    // Create plane pixel shader
    RefCntAutoPtr<IShader> pPlanePS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Plane PS";
        ShaderCI.FilePath        = "plane.psh";
        m_pDevice->CreateShader(ShaderCI, &pPlanePS);
    }

    PSOCreateInfo.pVS = pPlaneVS;
    PSOCreateInfo.pPS = pPlanePS;

    // Define variable type that will be used by default
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    // clang-format off
    // Shader variables should typically be mutable, which means they are expected
    // to change on a per-instance basis
    ShaderResourceVariableDesc Vars[] =
    {
        {SHADER_TYPE_PIXEL, "g_ShadowMap", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
    };
    // clang-format on
    PSOCreateInfo.PSODesc.ResourceLayout.Variables    = Vars;
        PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = sizeof(Vars)/sizeof(Vars[0]);

    // Define immutable comparison sampler for g_ShadowMap. Immutable samplers should be used whenever possible
    SamplerDesc ComparsionSampler;
    ComparsionSampler.ComparisonFunc = COMPARISON_FUNC_LESS;
    ComparsionSampler.MinFilter      = FILTER_TYPE_COMPARISON_LINEAR;
    ComparsionSampler.MagFilter      = FILTER_TYPE_COMPARISON_LINEAR;
    ComparsionSampler.MipFilter      = FILTER_TYPE_COMPARISON_LINEAR;
    // clang-format off
    ImmutableSamplerDesc ImtblSamplers[] =
    {
        {SHADER_TYPE_PIXEL, "g_ShadowMap", ComparsionSampler}
    };
    // clang-format on
    PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers    = ImtblSamplers;
        PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = sizeof(ImtblSamplers)/sizeof(ImtblSamplers[0]);

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPlanePSO);

    // Since we did not explicitly specify the type for 'Constants' variable, default
    // type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables never
    // change and are bound directly through the pipeline state object.
    m_pPlanePSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);
    }

    void C6GERender::CreateShadowMapVisPSO() 
    {
        GraphicsPipelineStateCreateInfo PSOCreateInfo;

    PSOCreateInfo.PSODesc.Name = "Shadow Map Vis PSO";

    // This is a graphics pipeline
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    // clang-format off
    // This tutorial renders to a single render target
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets             = 1;
    // Set render target format which is the format of the swap chain's color buffer
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                = m_pSwapChain->GetDesc().ColorBufferFormat;
    // Set depth buffer format which is the format of the swap chain's back buffer
    PSOCreateInfo.GraphicsPipeline.DSVFormat                    = m_pSwapChain->GetDesc().DepthBufferFormat;
    // Primitive topology defines what kind of primitives will be rendered by this pipeline state
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    // No cull
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
    // Disable depth testing
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;
    // clang-format on

    ShaderCreateInfo ShaderCI;
    // Tell the system that the shader source code is in HLSL.
    // For OpenGL, the engine will convert this into GLSL under the hood.
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
    ShaderCI.Desc.UseCombinedTextureSamplers = true;

    // Presentation engine always expects input in gamma space. Normally, pixel shader output is
    // converted from linear to gamma space by the GPU. However, some platforms (e.g. Android in GLES mode,
    // or Emscripten in WebGL mode) do not support gamma-correction. In this case the application
    // has to do the conversion manually.
    ShaderMacro Macros[] = {{"CONVERT_PS_OUTPUT_TO_GAMMA", m_ConvertPSOutputToGamma ? "1" : "0"}};
    ShaderCI.Macros      = {Macros, sizeof(Macros)/sizeof(Macros[0])};

    // Create a shader source stream factory to load shaders from files.
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    // Create shadow map visualization vertex shader
    RefCntAutoPtr<IShader> pShadowMapVisVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Shadow Map Vis VS";
        ShaderCI.FilePath        = "shadow_map_vis.vsh";
        m_pDevice->CreateShader(ShaderCI, &pShadowMapVisVS);
    }

    // Create shadow map visualization pixel shader
    RefCntAutoPtr<IShader> pShadowMapVisPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Shadow Map Vis PS";
        ShaderCI.FilePath        = "shadow_map_vis.psh";
        m_pDevice->CreateShader(ShaderCI, &pShadowMapVisPS);
    }

    PSOCreateInfo.pVS = pShadowMapVisVS;
    PSOCreateInfo.pPS = pShadowMapVisPS;

    // Define variable type that will be used by default
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

    ShaderResourceVariableDesc Variables[] =
        {
            {SHADER_TYPE_PIXEL, "g_ShadowMap", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE, SHADER_VARIABLE_FLAG_UNFILTERABLE_FLOAT_TEXTURE_WEBGPU},
        };
    PSOCreateInfo.PSODesc.ResourceLayout.Variables    = Variables;
    PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = sizeof(Variables)/sizeof(Variables[0]);

    // clang-format off
    SamplerDesc SamLinearClampDesc
    {
        FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
        TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP
    };
    ImmutableSamplerDesc ImtblSamplers[] =
    {
        {SHADER_TYPE_PIXEL, "g_ShadowMap", SamLinearClampDesc}
    };
    // clang-format on

    // Sampling depth textures is not supported on all GLES devices.
    // Setting non-comparison sampler for shadow map makes the Load() function
    // always return 0.
    if (m_pDevice->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_GLES)
    {
        PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers    = ImtblSamplers;
    PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = sizeof(ImtblSamplers)/sizeof(ImtblSamplers[0]);
    }

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pShadowMapVisPSO);
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

        std::vector<StateTransitionDesc> Barriers;
    // Create dynamic uniform buffer that will store our transformation matrices
    // Dynamic buffers can be frequently updated by the CPU
    CreateUniformBuffer(m_pDevice, sizeof(float4x4) * 2 + sizeof(float4), "VS constants CB", &m_VSConstants);
    Barriers.emplace_back(m_VSConstants, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE);

    CreateCubePSO();
    CreatePlanePSO();
    CreateShadowMapVisPSO();

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

        // Load cube

    // In this tutorial we need vertices with normals
    m_CubeVertexBuffer = TexturedCube::CreateVertexBuffer(m_pDevice, GEOMETRY_PRIMITIVE_VERTEX_FLAG_ALL);
    // Load index buffer
    m_CubeIndexBuffer = TexturedCube::CreateIndexBuffer(m_pDevice);
    // Explicitly transition vertex and index buffers to required states
    Barriers.emplace_back(m_CubeVertexBuffer, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_VERTEX_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE);
    Barriers.emplace_back(m_CubeIndexBuffer, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_INDEX_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE);
    // Load texture
    RefCntAutoPtr<ITexture> CubeTexture = TexturedCube::LoadTexture(m_pDevice, "C6GELogo.png");
    m_CubeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(CubeTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    // Transition the texture to shader resource state
    Barriers.emplace_back(CubeTexture, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_SHADER_RESOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE);

    CreateShadowMap();

    m_pImmediateContext->TransitionResourceStates(static_cast<Uint32>(Barriers.size()), Barriers.data());
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
        constexpr int MinShadowMapSize = 256;
        int           ShadowMapComboId = 0;
        while ((MinShadowMapSize << ShadowMapComboId) != static_cast<int>(m_ShadowMapSize))
            ++ShadowMapComboId;
        if (ImGui::Combo("Shadow map size", &ShadowMapComboId,
                         "256\0"
                         "512\0"
                         "1024\0\0"))
        {
            m_ShadowMapSize = MinShadowMapSize << ShadowMapComboId;
            CreateShadowMap();
        }
        ImGui::gizmo3D("##LightDirection", m_LightDirection, ImGui::GetTextLineHeight() * 10);
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

    void C6GERender::CreateShadowMap() 
    {
        TextureDesc SMDesc;
    SMDesc.Name      = "Shadow map";    
    SMDesc.Type      = RESOURCE_DIM_TEX_2D;
    SMDesc.Width     = m_ShadowMapSize;
    SMDesc.Height    = m_ShadowMapSize;
    SMDesc.Format    = m_ShadowMapFormat;
    SMDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_DEPTH_STENCIL;
    RefCntAutoPtr<ITexture> ShadowMap;
    m_pDevice->CreateTexture(SMDesc, nullptr, &ShadowMap);
    m_ShadowMapSRV = ShadowMap->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    m_ShadowMapDSV = ShadowMap->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);

    // Create SRBs that use shadow map as mutable variable
    m_PlaneSRB.Release();
    m_pPlanePSO->CreateShaderResourceBinding(&m_PlaneSRB, true);
    m_PlaneSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_ShadowMap")->Set(m_ShadowMapSRV);

    m_ShadowMapVisSRB.Release();
    m_pShadowMapVisPSO->CreateShaderResourceBinding(&m_ShadowMapVisSRB, true);
    m_ShadowMapVisSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_ShadowMap")->Set(m_ShadowMapSRV);
    }

    void C6GERender::RenderShadowMap() 
    {
        float3 f3LightSpaceX, f3LightSpaceY, f3LightSpaceZ;
    f3LightSpaceZ = normalize(m_LightDirection);

    float min_cmp = std::min(std::min(std::abs(m_LightDirection.x), std::abs(m_LightDirection.y)), std::abs(m_LightDirection.z));
    if (min_cmp == std::abs(m_LightDirection.x))
        f3LightSpaceX = float3(1, 0, 0);
    else if (min_cmp == std::abs(m_LightDirection.y))
        f3LightSpaceX = float3(0, 1, 0);
    else
        f3LightSpaceX = float3(0, 0, 1);

    f3LightSpaceY = cross(f3LightSpaceZ, f3LightSpaceX);
    f3LightSpaceX = cross(f3LightSpaceY, f3LightSpaceZ);
    f3LightSpaceX = normalize(f3LightSpaceX);
    f3LightSpaceY = normalize(f3LightSpaceY);

    float4x4 WorldToLightViewSpaceMatr = float4x4::ViewFromBasis(f3LightSpaceX, f3LightSpaceY, f3LightSpaceZ);

    // For this tutorial we know that the scene center is at (0,0,0).
    // Real applications will want to compute tight bounds

    float3 f3SceneCenter = float3(0, 0, 0);
    float  SceneRadius   = std::sqrt(3.f);
    float3 f3MinXYZ      = f3SceneCenter - float3(SceneRadius, SceneRadius, SceneRadius);
    float3 f3MaxXYZ      = f3SceneCenter + float3(SceneRadius, SceneRadius, SceneRadius * 5);
    float3 f3SceneExtent = f3MaxXYZ - f3MinXYZ;

    const RenderDeviceInfo& DevInfo = m_pDevice->GetDeviceInfo();

    const bool IsGL = DevInfo.IsGLDevice();
    float4     f4LightSpaceScale;
    f4LightSpaceScale.x = 2.f / f3SceneExtent.x;
    f4LightSpaceScale.y = 2.f / f3SceneExtent.y;
    f4LightSpaceScale.z = (IsGL ? 2.f : 1.f) / f3SceneExtent.z;
    // Apply bias to shift the extent to [-1,1]x[-1,1]x[0,1] for DX or to [-1,1]x[-1,1]x[-1,1] for GL
    // Find bias such that f3MinXYZ -> (-1,-1,0) for DX or (-1,-1,-1) for GL
    float4 f4LightSpaceScaledBias;
    f4LightSpaceScaledBias.x = -f3MinXYZ.x * f4LightSpaceScale.x - 1.f;
    f4LightSpaceScaledBias.y = -f3MinXYZ.y * f4LightSpaceScale.y - 1.f;
    f4LightSpaceScaledBias.z = -f3MinXYZ.z * f4LightSpaceScale.z + (IsGL ? -1.f : 0.f);

    float4x4 ScaleMatrix      = float4x4::Scale(f4LightSpaceScale.x, f4LightSpaceScale.y, f4LightSpaceScale.z);
    float4x4 ScaledBiasMatrix = float4x4::Translation(f4LightSpaceScaledBias.x, f4LightSpaceScaledBias.y, f4LightSpaceScaledBias.z);

    // Note: bias is applied after scaling!
    float4x4 ShadowProjMatr = ScaleMatrix * ScaledBiasMatrix;

    // Adjust the world to light space transformation matrix
    float4x4 WorldToLightProjSpaceMatr = WorldToLightViewSpaceMatr * ShadowProjMatr;

    const NDCAttribs& NDC           = DevInfo.GetNDCAttribs();
    float4x4          ProjToUVScale = float4x4::Scale(0.5f, NDC.YtoVScale, NDC.ZtoDepthScale);
    float4x4          ProjToUVBias  = float4x4::Translation(0.5f, 0.5f, NDC.GetZtoDepthBias());

    m_WorldToShadowMapUVDepthMatr = WorldToLightProjSpaceMatr * ProjToUVScale * ProjToUVBias;

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
        // Map the buffer and write current world-view-projection matrix
        MapHelper<Constants> CBConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        CBConstants->WorldViewProj = (m_CubeWorldMatrix * CameraViewProj);
        float4x4 NormalMatrix      = m_CubeWorldMatrix.RemoveTranslation().Inverse().Transpose();
        // We need to do inverse-transpose, but we also need to transpose the matrix
        // before writing it to the buffer
        CBConstants->NormalTranform = NormalMatrix;
        CBConstants->LightDirection = m_LightDirection;
    }

    // Bind vertex buffer
    IBuffer* pBuffs[] = {m_CubeVertexBuffer};
    // Note that since resources have been explicitly transitioned to required states, we use RESOURCE_STATE_TRANSITION_MODE_VERIFY flag
    m_pImmediateContext->SetVertexBuffers(0, 1, pBuffs, nullptr, RESOURCE_STATE_TRANSITION_MODE_VERIFY, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_VERIFY);

    // Set pipeline state and commit resources
    if (IsShadowPass)
    {
        m_pImmediateContext->SetPipelineState(m_pCubeShadowPSO);
        m_pImmediateContext->CommitShaderResources(m_CubeShadowSRB, RESOURCE_STATE_TRANSITION_MODE_VERIFY);
    }
    else
    {
        m_pImmediateContext->SetPipelineState(m_pCubePSO);
        m_pImmediateContext->CommitShaderResources(m_CubeSRB, RESOURCE_STATE_TRANSITION_MODE_VERIFY);
    }

    DrawIndexedAttribs DrawAttrs(36, VT_UINT32, DRAW_FLAG_VERIFY_ALL);
    m_pImmediateContext->DrawIndexed(DrawAttrs);
    }

    void C6GERender::RenderPlane() 
    {
        {
        struct Constants
        {
            float4x4 CameraViewProj;
            float4x4 WorldToShadowMapUVDepth;
            float4   LightDirection;
        };
        MapHelper<Constants> CBConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        CBConstants->CameraViewProj          = m_CameraViewProjMatrix;
        CBConstants->WorldToShadowMapUVDepth = m_WorldToShadowMapUVDepthMatr;
        CBConstants->LightDirection          = m_LightDirection;
    }

    m_pImmediateContext->SetPipelineState(m_pPlanePSO);
    // Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode
    // makes sure that resources are transitioned to required states.
    // Note that Vulkan requires shadow map to be transitioned to DEPTH_READ state, not SHADER_RESOURCE
    m_pImmediateContext->CommitShaderResources(m_PlaneSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawAttribs DrawAttrs(4, DRAW_FLAG_VERIFY_ALL);
    m_pImmediateContext->Draw(DrawAttrs);
    }

    void C6GERender::RenderShadowMapVis() 
    {
        m_pImmediateContext->SetPipelineState(m_pShadowMapVisPSO);
    m_pImmediateContext->CommitShaderResources(m_ShadowMapVisSRB, RESOURCE_STATE_TRANSITION_MODE_VERIFY);

    DrawAttribs DrawAttrs(4, DRAW_FLAG_VERIFY_ALL);
    m_pImmediateContext->Draw(DrawAttrs);
    }

    void C6GERender::DXSDKMESH_VERTEX_ELEMENTtoInputLayoutDesc(const DXSDKMESH_VERTEX_ELEMENT *VertexElement,
                                                               Uint32 Stride,
                                                               InputLayoutDesc &Layout,
                                                               std::vector<LayoutElement> &Elements)
    {
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
        ITextureView *pRTV_Framebuffer = m_pFramebufferRTV;
        ITextureView *pDSV_Framebuffer = m_pFramebufferDSV;
        float4 ClearColor_Framebuffer = {0.350f, 0.350f, 0.350f, 1.0f};
        if (m_ConvertPSOutputToGamma)
        {
            ClearColor_Framebuffer = LinearToSRGB(ClearColor_Framebuffer);
        }
        m_pImmediateContext->SetRenderTargets(1, &pRTV_Framebuffer, pDSV_Framebuffer, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearRenderTarget(pRTV_Framebuffer, ClearColor_Framebuffer.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearDepthStencil(pDSV_Framebuffer, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        // Render shadow map
        m_pImmediateContext->SetRenderTargets(0, nullptr, m_ShadowMapDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearDepthStencil(m_ShadowMapDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        RenderShadowMap();

        // Bind main back buffer
        ITextureView* pRTV_BackBuffer = m_pSwapChain->GetCurrentBackBufferRTV();
        ITextureView* pDSV_BackBuffer = m_pSwapChain->GetDepthBufferDSV();
        m_pImmediateContext->SetRenderTargets(1, &pRTV_BackBuffer, pDSV_BackBuffer, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        // Clear the back buffer
        float4 ClearColor_BackBuffer = {0.350f, 0.350f, 0.350f, 1.0f};
        if (m_ConvertPSOutputToGamma)
        {
            // If manual gamma correction is required, we need to clear the render target with sRGB color
            ClearColor_BackBuffer = LinearToSRGB(ClearColor_BackBuffer);
        }
        m_pImmediateContext->ClearRenderTarget(pRTV_BackBuffer, ClearColor_BackBuffer.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearDepthStencil(pDSV_BackBuffer, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        RenderCube(m_CameraViewProjMatrix, false);
        RenderPlane();
        RenderShadowMapVis();
    }

    void Diligent::C6GERender::Update(double CurrTime, double ElapsedTime, bool DoUpdateUI)
    {
        SampleBase::Update(CurrTime, ElapsedTime, DoUpdateUI);

        // Only update scene if playing
        if (!IsPlaying())
            return;

        // Animate the cube
        m_CubeWorldMatrix = float4x4::RotationY(static_cast<float>(CurrTime) * 1.0f);

        float4x4 CameraView = float4x4::Translation(0.f, -5.0f, -10.0f) * float4x4::RotationY(PI_F) * float4x4::RotationX(-PI_F * 0.2);

        // Get pretransform matrix that rotates the scene according the surface orientation
        float4x4 SrfPreTransform = this->GetSurfacePretransformMatrix(float3{0, 0, 1});

        // Get projection matrix adjusted to the current screen orientation
        float4x4 Proj = this->GetAdjustedProjectionMatrix(PI_F / 4.0f, 0.1f, 100.f);

        // Compute camera view-projection matrix
        m_CameraViewProjMatrix = CameraView * SrfPreTransform * Proj;
    }

    void Diligent::C6GERender::WindowResize(Uint32 Width, Uint32 Height)
    {
        if (!m_pSwapChain || !m_pDevice)
            return;

        float NearPlane = 0.1f;
        float FarPlane = 250.f;
        float AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
        m_Camera.SetProjAttribs(NearPlane, FarPlane, AspectRatio, Diligent::PI_F / 4.f,
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