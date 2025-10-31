#include "RenderCommon.hpp"

namespace Diligent
{

using RenderInternals::Constants;

void C6GERender::DXSDKMESH_VERTEX_ELEMENTtoInputLayoutDesc(const DXSDKMESH_VERTEX_ELEMENT* VertexElement,
                                                           Uint32                                   Stride,
                                                           InputLayoutDesc&                         Layout,
                                                           std::vector<LayoutElement>&              Elements)
{
}

void C6GERender::CreateCubePSO()
{
    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    PSOCreateInfo.PSODesc.Name         = "Cube PSO";
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    PSOCreateInfo.GraphicsPipeline.NumRenderTargets             = 1;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                = m_pSwapChain->GetDesc().ColorBufferFormat;
    PSOCreateInfo.GraphicsPipeline.DSVFormat                    = m_pSwapChain->GetDesc().DepthBufferFormat;
    PSOCreateInfo.GraphicsPipeline.SmplDesc.Count               = m_SampleCount;
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_BACK;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;

    CreateUniformBuffer(m_pDevice, sizeof(Constants), "Shader constants CB", &m_VSConstants);

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage                     = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.Desc.UseCombinedTextureSamplers    = true;
    ShaderCI.CompileFlags                       = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;
    ShaderMacro Macros[]                        = {{"CONVERT_PS_OUTPUT_TO_GAMMA", m_ConvertPSOutputToGamma ? "1" : "0"}};
    ShaderCI.Macros                             = {Macros, static_cast<Uint32>(_countof(Macros))};

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Cube VS";
        ShaderCI.FilePath        = "assets/cube.vsh";
        m_pDevice->CreateShader(ShaderCI, &pVS);
        if (!pVS)
        {
            std::cerr << "[C6GE] Error: Failed to compile Cube VS (assets/cube.vsh). Check shader log above." << std::endl;
        }
    }

    RefCntAutoPtr<IShader> pGS;
    if (m_pDevice->GetDeviceInfo().Features.GeometryShaders)
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_GEOMETRY;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Cube GS";
        ShaderCI.FilePath        = "assets/cube.gsh";
        m_pDevice->CreateShader(ShaderCI, &pGS);
    }

    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Cube PS";
        ShaderCI.FilePath        = "assets/cube.psh";
        m_pDevice->CreateShader(ShaderCI, &pPS);
        if (!pPS)
        {
            std::cerr << "[C6GE] Error: Failed to compile Cube PS (assets/cube.psh). Check shader log above." << std::endl;
        }
    }

    LayoutElement LayoutElems[] = {
        LayoutElement{0, 0, 3, VT_FLOAT32, False},
        LayoutElement{1, 0, 3, VT_FLOAT32, False},
        LayoutElement{2, 0, 2, VT_FLOAT32, False}};

    PSOCreateInfo.pVS = pVS;
    if (pGS)
        PSOCreateInfo.pGS = pGS;
    PSOCreateInfo.pPS = pPS;

    if (!PSOCreateInfo.pVS || !PSOCreateInfo.pPS)
    {
        std::cerr << "[C6GE] Error: Aborting Cube PSO creation due to missing shaders." << std::endl;
        return;
    }

    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements    = _countof(LayoutElems);

    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    ShaderResourceVariableDesc Vars[] = {
        {SHADER_TYPE_PIXEL, "g_Texture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}};
    PSOCreateInfo.PSODesc.ResourceLayout.Variables    = Vars;
    PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars);

    SamplerDesc          SamLinearClampDesc{FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
                                   TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP};
    ImmutableSamplerDesc ImtblSamplers[] = {{SHADER_TYPE_PIXEL, "g_Texture", SamLinearClampDesc}};
    PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers    = ImtblSamplers;
    PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pCubePSO);
    if (!m_pCubePSO)
    {
        std::cerr << "[C6GE] Error: CreateGraphicsPipelineState failed for Cube PSO. See previous shader compile messages." << std::endl;
    }

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

    m_pCubePSO->CreateShaderResourceBinding(&m_CubeSRB, true);

    {
        GraphicsPipelineStateCreateInfo ShadowPSOCI;
        ShadowPSOCI.PSODesc.Name                                = "Cube shadow PSO";
        ShadowPSOCI.PSODesc.PipelineType                        = PIPELINE_TYPE_GRAPHICS;
        ShadowPSOCI.GraphicsPipeline.NumRenderTargets           = 0;
        ShadowPSOCI.GraphicsPipeline.RTVFormats[0]              = TEX_FORMAT_UNKNOWN;
        ShadowPSOCI.GraphicsPipeline.DSVFormat                  = m_ShadowMapFormat;
        ShadowPSOCI.GraphicsPipeline.PrimitiveTopology          = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        ShadowPSOCI.GraphicsPipeline.RasterizerDesc.CullMode    = CULL_MODE_BACK;
        ShadowPSOCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;

        ShaderCreateInfo ShadowShaderCI;
        ShadowShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
        ShadowShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
        ShadowShaderCI.Desc.UseCombinedTextureSamplers = true;
        ShadowShaderCI.CompileFlags                    = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

        RefCntAutoPtr<IShader> pShadowVS;
        ShadowShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShadowShaderCI.EntryPoint      = "main";
        ShadowShaderCI.Desc.Name       = "Cube Shadow VS";
        ShadowShaderCI.FilePath        = "assets/cube_shadow.vsh";
        m_pDevice->CreateShader(ShadowShaderCI, &pShadowVS);

        ShadowPSOCI.pVS = pShadowVS;
        ShadowPSOCI.pPS = nullptr;

        LayoutElement ShadowLayoutElems[] = {
            LayoutElement{0, 0, 3, VT_FLOAT32, False},
            LayoutElement{1, 0, 3, VT_FLOAT32, False},
            LayoutElement{2, 0, 2, VT_FLOAT32, False}};
        ShadowPSOCI.GraphicsPipeline.InputLayout.LayoutElements = ShadowLayoutElems;
        ShadowPSOCI.GraphicsPipeline.InputLayout.NumElements    = _countof(ShadowLayoutElems);

        ShadowPSOCI.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

        if (m_pDevice->GetDeviceInfo().Features.DepthClamp)
            ShadowPSOCI.GraphicsPipeline.RasterizerDesc.DepthClipEnable = False;

        ShadowPSOCI.GraphicsPipeline.RasterizerDesc.DepthBias            = 0;
        ShadowPSOCI.GraphicsPipeline.RasterizerDesc.DepthBiasClamp       = 0.0f;
        ShadowPSOCI.GraphicsPipeline.RasterizerDesc.SlopeScaledDepthBias = 2.0f;

        m_pDevice->CreateGraphicsPipelineState(ShadowPSOCI, &m_pCubeShadowPSO);
        if (m_pCubeShadowPSO)
        {
            m_pCubeShadowPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);
            m_pCubeShadowPSO->CreateShaderResourceBinding(&m_CubeShadowSRB, true);
        }
    }

    if (m_CubeSRB && m_VSConstants)
    {
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

void C6GERender::CreatePlanePSO()
{
    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    PSOCreateInfo.PSODesc.Name         = "Plane PSO";
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    PSOCreateInfo.GraphicsPipeline.NumRenderTargets             = 1;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                = m_pSwapChain->GetDesc().ColorBufferFormat;
    PSOCreateInfo.GraphicsPipeline.DSVFormat                    = m_pSwapChain->GetDesc().DepthBufferFormat;
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage                  = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.Desc.UseCombinedTextureSamplers = true;
    ShaderCI.CompileFlags                    = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

    ShaderMacro Macros[] = {{"CONVERT_PS_OUTPUT_TO_GAMMA", m_ConvertPSOutputToGamma ? "1" : "0"}};
    ShaderCI.Macros      = {Macros, static_cast<Uint32>(_countof(Macros))};

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    RefCntAutoPtr<IShader> pPlaneVS;
    ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
    ShaderCI.EntryPoint      = "main";
    ShaderCI.Desc.Name       = "Plane VS";
    ShaderCI.FilePath        = "assets/plane.vsh";
    m_pDevice->CreateShader(ShaderCI, &pPlaneVS);

    RefCntAutoPtr<IShader> pPlanePS;
    ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
    ShaderCI.EntryPoint      = "main";
    ShaderCI.Desc.Name       = "Plane PS";
    ShaderCI.FilePath        = "assets/plane.psh";
    m_pDevice->CreateShader(ShaderCI, &pPlanePS);

    PSOCreateInfo.pVS = pPlaneVS;
    PSOCreateInfo.pPS = pPlanePS;

    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    ShaderResourceVariableDesc Vars[] = {
        {SHADER_TYPE_PIXEL, "g_ShadowMap", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}};
    PSOCreateInfo.PSODesc.ResourceLayout.Variables    = Vars;
    PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars);

    SamplerDesc ComparisonSampler;
    ComparisonSampler.ComparisonFunc = COMPARISON_FUNC_LESS;
    ComparisonSampler.MinFilter      = FILTER_TYPE_COMPARISON_LINEAR;
    ComparisonSampler.MagFilter      = FILTER_TYPE_COMPARISON_LINEAR;
    ComparisonSampler.MipFilter      = FILTER_TYPE_COMPARISON_LINEAR;

    ImmutableSamplerDesc ImtblSamplers[] = {
        {SHADER_TYPE_PIXEL, "g_ShadowMap", ComparisonSampler}};

    if (m_pDevice->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_GLES)
    {
        PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers    = ImtblSamplers;
        PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);
    }

    PSOCreateInfo.GraphicsPipeline.SmplDesc.Count = m_SampleCount;
    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPlanePSO);

    if (!m_pPlanePSO)
    {
        std::cerr << "[C6GE] Error: Failed to create Plane PSO." << std::endl;
    }
    else
    {
        auto* pVar = m_pPlanePSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants");
        if (pVar)
            pVar->Set(m_VSConstants);

        m_PlaneSRB.Release();
        m_pPlanePSO->CreateShaderResourceBinding(&m_PlaneSRB, true);
        if (m_PlaneSRB)
        {
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

    GraphicsPipelineStateCreateInfo NoShadowPSOCI = PSOCreateInfo;
    RefCntAutoPtr<IShader>          pNoShadowPS;
    ShaderCreateInfo                NoShadowCI = ShaderCI;
    NoShadowCI.Desc.ShaderType               = SHADER_TYPE_PIXEL;
    NoShadowCI.EntryPoint                    = "main";
    NoShadowCI.Desc.Name                     = "Plane NoShadow PS";
    NoShadowCI.FilePath                      = "assets/plane_no_shadow.psh";
    m_pDevice->CreateShader(NoShadowCI, &pNoShadowPS);
    NoShadowPSOCI.pVS = pPlaneVS;
    NoShadowPSOCI.pPS = pNoShadowPS;
    NoShadowPSOCI.PSODesc.Name                    = "Plane NoShadow PSO";
    NoShadowPSOCI.GraphicsPipeline.SmplDesc.Count = m_SampleCount;
    NoShadowPSOCI.PSODesc.ResourceLayout.NumVariables         = 0;
    NoShadowPSOCI.PSODesc.ResourceLayout.Variables            = nullptr;
    if (m_pDevice->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_GLES)
    {
        NoShadowPSOCI.PSODesc.ResourceLayout.NumImmutableSamplers = 0;
        NoShadowPSOCI.PSODesc.ResourceLayout.ImmutableSamplers    = nullptr;
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

void C6GERender::CreatePlaneMeshBuffers()
{
    struct Vtx
    {
        float px;
        float py;
        float pz;
        float nx;
        float ny;
        float nz;
        float u;
        float v;
    };
    const Vtx V[4] = {
        {-1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f},
        {-1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f},
        {1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f}};
    const Uint32 I[6] = {0, 1, 2, 2, 1, 3};

    BufferDesc VBDesc;
    VBDesc.Name      = "Plane vertex buffer";
    VBDesc.BindFlags = BIND_VERTEX_BUFFER;
    VBDesc.Size      = sizeof(V);
    VBDesc.Usage     = USAGE_IMMUTABLE;

    BufferData VBData;
    VBData.pData    = V;
    VBData.DataSize = VBDesc.Size;
    m_pDevice->CreateBuffer(VBDesc, &VBData, &m_PlaneVertexBuffer);

    BufferDesc IBDesc;
    IBDesc.Name      = "Plane index buffer";
    IBDesc.BindFlags = BIND_INDEX_BUFFER;
    IBDesc.Size      = sizeof(I);
    IBDesc.Usage     = USAGE_IMMUTABLE;

    BufferData IBData;
    IBData.pData    = I;
    IBData.DataSize = IBDesc.Size;
    m_pDevice->CreateBuffer(IBDesc, &IBData, &m_PlaneIndexBuffer);
}

void C6GERender::CreateGLTFShadowPSO()
{
    GraphicsPipelineStateCreateInfo PSOCreateInfo;
    PSOCreateInfo.PSODesc.Name         = "GLTF Shadow PSO";
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    PSOCreateInfo.GraphicsPipeline.NumRenderTargets             = 0;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                = TEX_FORMAT_UNKNOWN;
    PSOCreateInfo.GraphicsPipeline.DSVFormat                    = m_ShadowMapFormat;
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_BACK;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage                  = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.Desc.UseCombinedTextureSamplers = true;
    ShaderCI.CompileFlags                    = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    RefCntAutoPtr<IShader> pVS;
    ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
    ShaderCI.EntryPoint      = "main";
    ShaderCI.Desc.Name       = "GLTF Shadow VS";
    ShaderCI.FilePath        = "assets/gltf_shadow.vsh";
    m_pDevice->CreateShader(ShaderCI, &pVS);

    LayoutElement LayoutElems[] = {
        LayoutElement{0, 0, 3, VT_FLOAT32, False},
        LayoutElement{1, 0, 3, VT_FLOAT32, False},
        LayoutElement{2, 0, 2, VT_FLOAT32, False}};

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements    = _countof(LayoutElems);
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pGLTFShadowPSO);
    if (m_pGLTFShadowPSO)
        m_pGLTFShadowPSO->CreateShaderResourceBinding(&m_GLTFShadowSRB, true);
}

void C6GERender::CreateGLTFShadowSkinnedPSO()
{
    GraphicsPipelineStateCreateInfo PSOCreateInfo;
    PSOCreateInfo.PSODesc.Name         = "GLTF Shadow Skinned PSO";
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    PSOCreateInfo.GraphicsPipeline.NumRenderTargets             = 0;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                = TEX_FORMAT_UNKNOWN;
    PSOCreateInfo.GraphicsPipeline.DSVFormat                    = m_ShadowMapFormat;
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_BACK;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage                  = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.Desc.UseCombinedTextureSamplers = true;
    ShaderCI.CompileFlags                    = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    RefCntAutoPtr<IShader> pVS;
    ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
    ShaderCI.EntryPoint      = "main";
    ShaderCI.Desc.Name       = "GLTF Shadow Skinned VS";
    ShaderCI.FilePath        = "assets/gltf_shadow_skinned.vsh";
    m_pDevice->CreateShader(ShaderCI, &pVS);

    LayoutElement LayoutElems[] = {
        LayoutElement{0, 0, 3, VT_FLOAT32, False},
        LayoutElement{1, 0, 3, VT_FLOAT32, False},
        LayoutElement{2, 0, 2, VT_FLOAT32, False},
        LayoutElement{3, 0, 4, VT_FLOAT32, False},
        LayoutElement{4, 0, 4, VT_FLOAT32, False}};

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements    = _countof(LayoutElems);
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pGLTFShadowSkinnedPSO);
    if (m_pGLTFShadowSkinnedPSO)
        m_pGLTFShadowSkinnedPSO->CreateShaderResourceBinding(&m_GLTFShadowSkinnedSRB, true);
}

} // namespace Diligent