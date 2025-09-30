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
#include "imgui.h"
#include "DiligentTools/ThirdParty/imGuIZMO.quat/imGuIZMO.h"
#include "ImGuiUtils.hpp"
#include "CallbackWrapper.hpp"
#include "Utilities/interface/DiligentFXShaderSourceStreamFactory.hpp"
#include "ShaderSourceFactoryUtils.hpp"

namespace Diligent
{

bool RenderShadows = true;

SampleBase* CreateSample()
{
    return new ShadowsSample();
}

ShadowsSample::~ShadowsSample()
{
}

void ShadowsSample::ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs)
{
    SampleBase::ModifyEngineInitInfo(Attribs);

    Attribs.EngineCI.Features.DepthClamp = DEVICE_FEATURE_STATE_OPTIONAL;

#if D3D12_SUPPORTED
    if (Attribs.DeviceType == RENDER_DEVICE_TYPE_D3D12)
    {
        EngineD3D12CreateInfo& D3D12CI          = static_cast<EngineD3D12CreateInfo&>(Attribs.EngineCI);
        D3D12CI.GPUDescriptorHeapSize[1]        = 1024; // Sampler descriptors
        D3D12CI.GPUDescriptorHeapDynamicSize[1] = 1024;
    }
#endif
}

void ShadowsSample::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    std::string MeshFileName = "Powerplant/Powerplant.sdkmesh";
    m_Mesh.Create(MeshFileName.c_str());
    std::string Directory;
    FileSystem::GetPathComponents(MeshFileName, &Directory, nullptr);
    m_Mesh.LoadGPUResources(Directory.c_str(), m_pDevice, m_pImmediateContext);

    m_LightAttribs.ShadowAttribs.iNumCascades     = 4;
    m_LightAttribs.ShadowAttribs.fFixedDepthBias  = 0.0025f;
    m_LightAttribs.ShadowAttribs.iFixedFilterSize = 5;
    m_LightAttribs.ShadowAttribs.fFilterWorldSize = 0.1f;

    m_LightAttribs.f4Direction    = float3(-0.522699475f, -0.481321275f, -0.703671455f);
    m_LightAttribs.f4Intensity    = float4(1, 0.8f, 0.5f, 1);
    m_LightAttribs.f4AmbientLight = float4(0.125f, 0.125f, 0.125f, 1);

    // Due to a bug, NVidia OpenGL driver crashes when compiling shaders with row-major matrices
    // in nested structures.
    m_PackMatrixRowMajor = !m_pDevice->GetDeviceInfo().IsGLDevice();

    m_Camera.SetPos(float3(70, 10, 0.f));
    m_Camera.SetRotation(PI_F / 2.f, 0);
    m_Camera.SetRotationSpeed(0.005f);
    m_Camera.SetMoveSpeed(5.f);
    m_Camera.SetSpeedUpScales(5.f, 10.f);

    RefCntAutoPtr<IRenderStateNotationParser> pRSNParser;
    {
        RefCntAutoPtr<IShaderSourceInputStreamFactory> pStreamFactory;
        m_pEngineFactory->CreateDefaultShaderSourceStreamFactory("render_states", &pStreamFactory);

        CreateRenderStateNotationParser({}, &pRSNParser);
        pRSNParser->ParseFile("RenderStates.json", pStreamFactory);
    }

    {
        RefCntAutoPtr<IShaderSourceInputStreamFactory> pStreamFactory;
        m_pEngineFactory->CreateDefaultShaderSourceStreamFactory("shaders", &pStreamFactory);

        RefCntAutoPtr<IShaderSourceInputStreamFactory> pCompoundFactory =
            CreateCompoundShaderSourceFactory({&DiligentFXShaderSourceStreamFactory::GetInstance(), pStreamFactory});

        CreateRenderStateNotationLoader({m_pDevice, pRSNParser, pCompoundFactory}, &m_pRSNLoader);
    }

    CreateUniformBuffer(m_pDevice, sizeof(CameraAttribs), "Camera attribs buffer", &m_CameraAttribsCB);
    CreateUniformBuffer(m_pDevice, sizeof(LightAttribs), "Light attribs buffer", &m_LightAttribsCB);
    CreatePipelineStates();

    // Feature: Shadows
    m_ShadowFeature = std::make_unique<ShadowsFeature>(this);
    if (RenderShadows)
    {
        m_ShadowFeature->InitShadows();
    }
    else
    {
        // Prepare material SRBs without shadow resources
        m_ShadowFeature->InitializeResourceBindings();
    }
}

void ShadowsSample::UpdateUI()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::gizmo3D("Light direction", reinterpret_cast<float3&>(m_LightAttribs.f4Direction), ImGui::GetTextLineHeight() * 10);

        {
            constexpr int MinShadowMapSize = 512;
            int           ShadowMapComboId = 0;
            while ((MinShadowMapSize << ShadowMapComboId) != static_cast<int>(m_ShadowSettings.Resolution))
                ++ShadowMapComboId;
            if (ImGui::Combo("Shadow map size", &ShadowMapComboId,
                             "512\0"
                             "1024\0"
                             "2048\0\0"))
            {
                m_ShadowSettings.Resolution = MinShadowMapSize << ShadowMapComboId;
                if (m_ShadowFeature)
                    m_ShadowFeature->InitShadows();
            }
        }

        // Toggle shadows with safe init/deinit
        static bool PrevRenderShadows = RenderShadows;
        if (ImGui::Checkbox("Shadows", &RenderShadows))
        {
            const bool TurningOn  = RenderShadows && !PrevRenderShadows;
            const bool TurningOff = !RenderShadows && PrevRenderShadows;

            if (TurningOn)
            {
                // Restore cascade count and initialize shadow resources
                if (m_LastShadowCascadeCount <= 0)
                    m_LastShadowCascadeCount = 4;
                m_LightAttribs.ShadowAttribs.iNumCascades = m_LastShadowCascadeCount;
                // Initialize shadow resources and rebuild SRBs to bind shadow maps
                if (m_ShadowFeature)
                {
                    m_ShadowFeature->InitShadows();
                    m_ShadowFeature->InitializeResourceBindings();
                }
            }
            else if (TurningOff)
            {
                // Save cascade count then set to zero to fully disable shadowing in shaders
                m_LastShadowCascadeCount = m_LightAttribs.ShadowAttribs.iNumCascades;
                m_LightAttribs.ShadowAttribs.iNumCascades = 0;
                // Reset shadow resources and rebuild SRBs without shadow bindings
                m_ShadowMapMgr = ShadowMapManager{};
                if (m_ShadowFeature)
                    m_ShadowFeature->InitializeResourceBindings();
            }

            PrevRenderShadows = RenderShadows;
        }

        if (ImGui::SliderInt("Num cascades", &m_LightAttribs.ShadowAttribs.iNumCascades, 1, 8))
        {
            if (m_ShadowFeature)
                m_ShadowFeature->InitShadows();
        }

        {
            int Is32Bit = m_ShadowSettings.Format == TEX_FORMAT_D16_UNORM ? 0 : 1;
            if (ImGui::Combo("Shadow map format", &Is32Bit,
                             "16-bit\0"
                             "32-bit\0\0"))
            {
                m_ShadowSettings.Format = Is32Bit == 0 ? TEX_FORMAT_D16_UNORM : TEX_FORMAT_D32_FLOAT;
                CreatePipelineStates();
                if (m_ShadowFeature)
                    m_ShadowFeature->InitShadows();
            }
        }

        {
            static_assert(SHADOW_MODE_PCF == 1 && SHADOW_MODE_VSM == 2 && SHADOW_MODE_EVSM2 == 3 && SHADOW_MODE_EVSM4 == 4, "Unexpected constant");
            // clang-format off
            const char* ShadowModes[]
            {
                "PCF",
                "VSM",
                "EVSM2",
                "EVSM4"
            };
            // clang-format on
            int iShadowModeComboItem = m_ShadowSettings.iShadowMode - 1;
            if (ImGui::Combo("Shadow mode", &iShadowModeComboItem, ShadowModes, _countof(ShadowModes)))
            {
                m_ShadowSettings.iShadowMode = iShadowModeComboItem + 1;
                CreatePipelineStates();
                if (m_ShadowFeature)
                    m_ShadowFeature->InitShadows();
            }
        }

        {
            // clang-format off
            const std::pair<int, const char*> FilterSizes[] =
            {
                {0, "World-constant"},
                {2, "Fixed 2x2"},
                {3, "Fixed 3x3"},
                {5, "Fixed 5x5"},
                {7, "Fixed 7x7"}
            };
            // clang-format on
            if (ImGui::Combo("Shadow filter size", &m_LightAttribs.ShadowAttribs.iFixedFilterSize, FilterSizes, _countof(FilterSizes)))
            {
                m_ShadowSettings.FilterAcrossCascades = m_LightAttribs.ShadowAttribs.iFixedFilterSize > 0;
                CreatePipelineStates();
            }
        }

        if (m_ShadowSettings.iShadowMode == SHADOW_MODE_VSM ||
            m_ShadowSettings.iShadowMode == SHADOW_MODE_EVSM2 ||
            m_ShadowSettings.iShadowMode == SHADOW_MODE_EVSM4)
        {
            if (ImGui::Checkbox("32-bit filterable Format", &m_ShadowSettings.Is32BitFilterableFmt))
            {
                if (m_ShadowFeature)
                    m_ShadowFeature->InitShadows();
            }
        }

        ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
        if (ImGui::TreeNode("Cascade allocation"))
        {
            if (ImGui::InputFloat("Partitioning Factor", &m_ShadowSettings.PartitioningFactor, 0.001f, 0.01f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue))
            {
                m_ShadowSettings.PartitioningFactor = clamp(m_ShadowSettings.PartitioningFactor, 0.f, 1.f);
            }
            // clang-format off
            ImGui::Checkbox("Snap cascades",     &m_ShadowSettings.SnapCascades);
            ImGui::Checkbox("Stabilize extents", &m_ShadowSettings.StabilizeExtents);
            ImGui::Checkbox("Equalize extents",  &m_ShadowSettings.EqualizeExtents);
            // clang-format on
            if (ImGui::Checkbox("Use best cascade", &m_ShadowSettings.SearchBestCascade))
            {
                CreatePipelineStates();
            }
            ImGui::TreePop();
        }

        ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
        if (ImGui::TreeNode("Filtering"))
        {
            if (ImGui::Checkbox("Filter across cascades", &m_ShadowSettings.FilterAcrossCascades))
                CreatePipelineStates();

            if (m_ShadowSettings.FilterAcrossCascades)
                ImGui::SliderFloat("Cascade transition region", &m_LightAttribs.ShadowAttribs.fCascadeTransitionRegion, 0, 0.5f);

            if (m_LightAttribs.ShadowAttribs.iFixedFilterSize == 0)
                ImGui::SliderFloat("Filter world size", &m_LightAttribs.ShadowAttribs.fFilterWorldSize, 0, 0.25f);

            if (m_ShadowSettings.iShadowMode == SHADOW_MODE_PCF)
            {
                ImGui::SliderFloat("Max depth bias slope", &m_LightAttribs.ShadowAttribs.fReceiverPlaneDepthBiasClamp, 0, 20);
                ImGui::SliderFloat("Fixed depth bias", &m_LightAttribs.ShadowAttribs.fFixedDepthBias, 0, 1, "%.4f", ImGuiSliderFlags_Logarithmic);
            }

            if (m_ShadowSettings.iShadowMode == SHADOW_MODE_EVSM2 ||
                m_ShadowSettings.iShadowMode == SHADOW_MODE_EVSM4)
                ImGui::SliderFloat("Positive EVSM Exponent", &m_LightAttribs.ShadowAttribs.fEVSMPositiveExponent, 0.1f, 40);

            if (m_ShadowSettings.iShadowMode == SHADOW_MODE_EVSM4)
                ImGui::SliderFloat("Negative EVSM Exponent", &m_LightAttribs.ShadowAttribs.fEVSMNegativeExponent, 0.1f, 40);

            if (m_ShadowSettings.iShadowMode == SHADOW_MODE_VSM ||
                m_ShadowSettings.iShadowMode == SHADOW_MODE_EVSM2 ||
                m_ShadowSettings.iShadowMode == SHADOW_MODE_EVSM4)
                ImGui::SliderFloat("Light bleeding reduction", &m_LightAttribs.ShadowAttribs.fVSMLightBleedingReduction, 0, 0.99f, "%.4f", ImGuiSliderFlags_Logarithmic);

            if (m_ShadowSettings.iShadowMode == SHADOW_MODE_VSM)
                ImGui::SliderFloat("VSM Bias", &m_LightAttribs.ShadowAttribs.fVSMBias, 0, 1, "%.4f", ImGuiSliderFlags_Logarithmic);

            ImGui::TreePop();
        }

        ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
        if (ImGui::TreeNode("Visualization"))
        {
            ImGui::Checkbox("Visualize cascades", &m_LightAttribs.ShadowAttribs.bVisualizeCascades);
            ImGui::Checkbox("Shadows only", &m_LightAttribs.ShadowAttribs.bVisualizeShadowing);
            ImGui::TreePop();
        }
    }
    ImGui::End();
}


void ShadowsSample::DXSDKMESH_VERTEX_ELEMENTtoInputLayoutDesc(const DXSDKMESH_VERTEX_ELEMENT* VertexElement,
                                                              Uint32                          Stride,
                                                              InputLayoutDesc&                Layout,
                                                              std::vector<LayoutElement>&     Elements)
{
    Elements.clear();
    for (Uint32 input_elem = 0; VertexElement[input_elem].Stream != 0xFF; ++input_elem)
    {
        const DXSDKMESH_VERTEX_ELEMENT& SrcElem = VertexElement[input_elem];

        Int32 InputIndex = -1;
        switch (SrcElem.Usage)
        {
            case DXSDKMESH_VERTEX_SEMANTIC_POSITION:
                InputIndex = 0;
                break;

            case DXSDKMESH_VERTEX_SEMANTIC_NORMAL:
                InputIndex = 1;
                break;

            case DXSDKMESH_VERTEX_SEMANTIC_TEXCOORD:
                InputIndex = 2;
                break;
        }

        if (InputIndex >= 0)
        {
            Uint32     NumComponents = 0;
            VALUE_TYPE ValueType     = VT_UNDEFINED;
            Bool       IsNormalized  = False;
            switch (SrcElem.Type)
            {
                case DXSDKMESH_VERTEX_DATA_TYPE_FLOAT2:
                    NumComponents = 2;
                    ValueType     = VT_FLOAT32;
                    IsNormalized  = False;
                    break;

                case DXSDKMESH_VERTEX_DATA_TYPE_FLOAT3:
                    NumComponents = 3;
                    ValueType     = VT_FLOAT32;
                    IsNormalized  = False;
                    break;

                default:
                    UNEXPECTED("Unsupported data type. Please add appropriate case statement.");
            }
            Elements.emplace_back(InputIndex, SrcElem.Stream, NumComponents, ValueType, IsNormalized, SrcElem.Offset, Stride);
        }
    }
    Layout.LayoutElements = Elements.data();
    Layout.NumElements    = static_cast<Uint32>(Elements.size());
}

void ShadowsSample::CreatePipelineStates()
{
    ShaderMacroHelper Macros;
    Macros.AddShaderMacro("SHADOW_MODE", m_ShadowSettings.iShadowMode);
    Macros.AddShaderMacro("PCF_FILTER_SIZE", m_LightAttribs.ShadowAttribs.iFixedFilterSize);
    Macros.AddShaderMacro("FILTER_ACROSS_CASCADES", m_ShadowSettings.FilterAcrossCascades);
    Macros.AddShaderMacro("BEST_CASCADE_SEARCH", m_ShadowSettings.SearchBestCascade);
    Macros.AddShaderMacro("CONVERT_PS_OUTPUT_TO_GAMMA", m_ConvertPSOutputToGamma);

    RefCntAutoPtr<IShader> pGeometryVS;
    RefCntAutoPtr<IShader> pGeometryPS;
    {
        auto ModifyCI = MakeCallback([&](ShaderCreateInfo& ShaderCI) {
            ShaderCI.Macros       = Macros;
            ShaderCI.CompileFlags = m_PackMatrixRowMajor ? SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR : SHADER_COMPILE_FLAG_NONE;
        });

        m_pRSNLoader->LoadShader({"Mesh VS", false, false, ModifyCI, ModifyCI}, &pGeometryVS);
        m_pRSNLoader->LoadShader({"Mesh PS", false, false, ModifyCI, ModifyCI}, &pGeometryPS);
    }

    Macros.AddShaderMacro("SHADOW_PASS", true);
    RefCntAutoPtr<IShader> pShadowVS;
    {
        auto ModifyCI = MakeCallback([&](ShaderCreateInfo& ShaderCI) {
            ShaderCI.Macros       = Macros;
            ShaderCI.CompileFlags = m_PackMatrixRowMajor ? SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR : SHADER_COMPILE_FLAG_NONE;
        });

        m_pRSNLoader->LoadShader({"Mesh VS", false, false, ModifyCI, ModifyCI}, &pShadowVS);
    }

    m_PSOIndex.resize(m_Mesh.GetNumVBs());
    m_RenderMeshPSO.clear();
    m_RenderMeshShadowPSO.clear();
    for (Uint32 vb = 0; vb < m_Mesh.GetNumVBs(); ++vb)
    {
        std::vector<LayoutElement> Elements;
        InputLayoutDesc            InputLayout{};
        DXSDKMESH_VERTEX_ELEMENTtoInputLayoutDesc(m_Mesh.VBElements(vb), m_Mesh.GetVertexStride(vb), InputLayout, Elements);

        //  Try to find PSO with the same layout
        Uint32 pso;
        for (pso = 0; pso < m_RenderMeshPSO.size(); ++pso)
        {
            const InputLayoutDesc& PSOLayout = m_RenderMeshPSO[pso]->GetGraphicsPipelineDesc().InputLayout;
            if (PSOLayout == InputLayout)
                break;
        }

        m_PSOIndex[vb] = pso;
        if (pso < static_cast<Uint32>(m_RenderMeshPSO.size()))
            continue;

        {
            ShaderResourceVariableDesc Vars[] = {
                {SHADER_TYPE_PIXEL, "g_tex2DDiffuse", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
                {
                    SHADER_TYPE_PIXEL,
                    m_ShadowSettings.iShadowMode == SHADOW_MODE_PCF ? "g_tex2DShadowMap" : "g_tex2DFilterableShadowMap",
                    SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE,
                    m_ShadowSettings.iShadowMode == SHADOW_MODE_PCF ? SHADER_VARIABLE_FLAG_UNFILTERABLE_FLOAT_TEXTURE_WEBGPU : SHADER_VARIABLE_FLAG_NONE,
                }};

            auto ModifyCI = MakeCallback([&](PipelineStateCreateInfo& PipelineCI) {
                GraphicsPipelineStateCreateInfo& GraphicsPipelineCI = static_cast<GraphicsPipelineStateCreateInfo&>(PipelineCI);

                GraphicsPipelineCI.PSODesc.ResourceLayout.Variables    = Vars;
                GraphicsPipelineCI.PSODesc.ResourceLayout.NumVariables = _countof(Vars);

                GraphicsPipelineCI.GraphicsPipeline.InputLayout      = InputLayout;
                GraphicsPipelineCI.GraphicsPipeline.RTVFormats[0]    = m_pSwapChain->GetDesc().ColorBufferFormat;
                GraphicsPipelineCI.GraphicsPipeline.DSVFormat        = m_pSwapChain->GetDesc().DepthBufferFormat;
                GraphicsPipelineCI.GraphicsPipeline.NumRenderTargets = 1;

                GraphicsPipelineCI.pVS = pGeometryVS;
                GraphicsPipelineCI.pPS = pGeometryPS;
            });

            RefCntAutoPtr<IPipelineState> pRenderMeshPSO;
            m_pRSNLoader->LoadPipelineState({"Mesh PSO", PIPELINE_TYPE_GRAPHICS, false, false, ModifyCI, ModifyCI}, &pRenderMeshPSO);

            pRenderMeshPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "cbCameraAttribs")->Set(m_CameraAttribsCB);
            pRenderMeshPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "cbLightAttribs")->Set(m_LightAttribsCB);
            pRenderMeshPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "cbLightAttribs")->Set(m_LightAttribsCB);
            m_RenderMeshPSO.emplace_back(std::move(pRenderMeshPSO));
        }

        {
            auto ModifyCI = MakeCallback([&](PipelineStateCreateInfo& PipelineCI) {
                GraphicsPipelineStateCreateInfo& GraphicsPipelineCI = static_cast<GraphicsPipelineStateCreateInfo&>(PipelineCI);

                GraphicsPipelineCI.GraphicsPipeline.InputLayout = InputLayout;
                GraphicsPipelineCI.GraphicsPipeline.DSVFormat   = m_ShadowSettings.Format;

                GraphicsPipelineCI.pVS = pShadowVS;
            });

            RefCntAutoPtr<IPipelineState> pRenderMeshShadowPSO;
            m_pRSNLoader->LoadPipelineState({"Mesh Shadow PSO", PIPELINE_TYPE_GRAPHICS, false, false, ModifyCI, ModifyCI}, &pRenderMeshShadowPSO);
            pRenderMeshShadowPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "cbCameraAttribs")->Set(m_CameraAttribsCB);
            m_RenderMeshShadowPSO.emplace_back(std::move(pRenderMeshShadowPSO));
        }
    }
}




// Render a frame
void ShadowsSample::Render()
{
    if (RenderShadows && m_ShadowFeature)
        m_ShadowFeature->RenderShadows();

    // Reset default framebuffer
    ITextureView* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    ITextureView* pDSV = m_pSwapChain->GetDepthBufferDSV();
    m_pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Clear the back buffer
    const float ClearColor[] = {0.23f, 0.5f, 0.74f, 1.0f};
    m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
        MapHelper<LightAttribs> LightData(m_pImmediateContext, m_LightAttribsCB, MAP_WRITE, MAP_FLAG_DISCARD);
        *LightData = m_LightAttribs;
    }

    // Get pretransform matrix that rotates the scene according the surface orientation
    float4x4 SrfPreTransform = GetSurfacePretransformMatrix(float3{0, 0, 1});

    const float4x4  CameraView     = m_Camera.GetViewMatrix() * SrfPreTransform;
    const float4x4& CameraWorld    = m_Camera.GetWorldMatrix();
    float3          CameraWorldPos = float3::MakeVector(CameraWorld[3]);
    const float4x4& Proj           = m_Camera.GetProjMatrix();

    float4x4 CameraViewProj = CameraView * Proj;

    {
        MapHelper<CameraAttribs> CamAttribs(m_pImmediateContext, m_CameraAttribsCB, MAP_WRITE, MAP_FLAG_DISCARD);
        WriteShaderMatrix(&CamAttribs->mProj, Proj, !m_PackMatrixRowMajor);
        WriteShaderMatrix(&CamAttribs->mViewProj, CameraViewProj, !m_PackMatrixRowMajor);
        WriteShaderMatrix(&CamAttribs->mViewProjInv, CameraViewProj.Inverse(), !m_PackMatrixRowMajor);
        CamAttribs->f4Position = float4(CameraWorldPos, 1);
    }

    ViewFrustumExt Frutstum;
    ExtractViewFrustumPlanesFromMatrix(CameraViewProj, Frutstum, m_pDevice->GetDeviceInfo().IsGLDevice());
    DrawMesh(m_pImmediateContext, false, Frutstum);
}


void ShadowsSample::DrawMesh(IDeviceContext* pCtx, bool bIsShadowPass, const ViewFrustumExt& Frustum)
{
    // Note that Vulkan requires shadow map to be transitioned to DEPTH_READ state, not SHADER_RESOURCE
    auto& SRBs = (bIsShadowPass ? m_ShadowSRBs : m_SRBs);
    if (SRBs.empty() || m_PSOIndex.empty() ||
        (bIsShadowPass ? m_RenderMeshShadowPSO.empty() : m_RenderMeshPSO.empty()))
    {
        return; // Nothing to draw or resources not initialized
    }

    pCtx->TransitionShaderResources(SRBs[0]);

    for (Uint32 meshIdx = 0; meshIdx < m_Mesh.GetNumMeshes(); ++meshIdx)
    {
        const DXSDKMESH_MESH& SubMesh = m_Mesh.GetMesh(meshIdx);
        BoundBox              BB;
        BB.Min = SubMesh.BoundingBoxCenter - SubMesh.BoundingBoxExtents * 0.5f;
        BB.Max = SubMesh.BoundingBoxCenter + SubMesh.BoundingBoxExtents * 0.5f;
        // Notice that for shadow pass we test against frustum with open near plane
        if (GetBoxVisibility(Frustum, BB, bIsShadowPass ? FRUSTUM_PLANE_FLAG_OPEN_NEAR : FRUSTUM_PLANE_FLAG_FULL_FRUSTUM) == BoxVisibility::Invisible)
            continue;

        IBuffer* pVBs[] = {m_Mesh.GetMeshVertexBuffer(meshIdx, 0)};
        pCtx->SetVertexBuffers(0, 1, pVBs, nullptr, RESOURCE_STATE_TRANSITION_MODE_VERIFY, SET_VERTEX_BUFFERS_FLAG_RESET);

        IBuffer*   pIB      = m_Mesh.GetMeshIndexBuffer(meshIdx);
        VALUE_TYPE IBFormat = m_Mesh.GetIBFormat(meshIdx);

        pCtx->SetIndexBuffer(pIB, 0, RESOURCE_STATE_TRANSITION_MODE_VERIFY);

        Uint32 VBIndex0 = SubMesh.VertexBuffers[0];
        if (VBIndex0 >= m_PSOIndex.size())
            continue;
        Uint32 PSOIndex = m_PSOIndex[VBIndex0];
        auto&  PSOArray = (bIsShadowPass ? m_RenderMeshShadowPSO : m_RenderMeshPSO);
        if (PSOIndex >= PSOArray.size())
            continue;
        auto&  pPSO     = PSOArray[PSOIndex];
        pCtx->SetPipelineState(pPSO);

        // Draw all subsets
        for (Uint32 subsetIdx = 0; subsetIdx < SubMesh.NumSubsets; ++subsetIdx)
        {
            const DXSDKMESH_SUBSET& Subset = m_Mesh.GetSubset(meshIdx, subsetIdx);
            if (Subset.MaterialID < SRBs.size())
                pCtx->CommitShaderResources(SRBs[Subset.MaterialID], RESOURCE_STATE_TRANSITION_MODE_VERIFY);
            else
                continue;

            DrawIndexedAttribs drawAttrs(static_cast<Uint32>(Subset.IndexCount), IBFormat, DRAW_FLAG_VERIFY_ALL);
            drawAttrs.FirstIndexLocation = static_cast<Uint32>(Subset.IndexStart);
            pCtx->DrawIndexed(drawAttrs);
        }
    }
}

void ShadowsSample::Update(double CurrTime, double ElapsedTime, bool DoUpdateUI)
{
    SampleBase::Update(CurrTime, ElapsedTime, DoUpdateUI);

    m_Camera.Update(m_InputController, static_cast<float>(ElapsedTime));
    {
        const MouseState& mouseState = m_InputController.GetMouseState();
        if (m_LastMouseState.PosX >= 0 &&
            m_LastMouseState.PosY >= 0 &&
            (m_LastMouseState.ButtonFlags & MouseState::BUTTON_FLAG_RIGHT) != 0)
        {
            constexpr float LightRotationSpeed = 0.001f;

            float   fYawDelta   = (mouseState.PosX - m_LastMouseState.PosX) * LightRotationSpeed;
            float   fPitchDelta = (mouseState.PosY - m_LastMouseState.PosY) * LightRotationSpeed;
            float3& f3LightDir  = reinterpret_cast<float3&>(m_LightAttribs.f4Direction);

            f3LightDir = float4(f3LightDir, 0) *
                float4x4::RotationArbitrary(m_Camera.GetWorldUp(), fYawDelta) *
                float4x4::RotationArbitrary(m_Camera.GetWorldRight(), fPitchDelta);
        }

        m_LastMouseState = mouseState;
    }

    if (RenderShadows && m_ShadowFeature)
    {
        m_ShadowFeature->UpdateShadows();
    }
}

void ShadowsSample::WindowResize(Uint32 Width, Uint32 Height)
{
    float NearPlane   = 0.1f;
    float FarPlane    = 250.f;
    float AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
    m_Camera.SetProjAttribs(NearPlane, FarPlane, AspectRatio, PI_F / 4.f,
                            m_pSwapChain->GetDesc().PreTransform, m_pDevice->GetDeviceInfo().NDC.MinZ == -1);
}

} // namespace Diligent
