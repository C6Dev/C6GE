#include "RenderCommon.hpp"

namespace Diligent
{

using RenderInternals::RTConstantsCPU;

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

} // namespace Diligent
