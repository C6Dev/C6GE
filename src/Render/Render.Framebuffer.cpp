#include "RenderCommon.hpp"

namespace Diligent
{

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

    m_PlaneSRB.Release();
    if (m_pPlanePSO)
        m_pPlanePSO->CreateShaderResourceBinding(&m_PlaneSRB, true);
    if (m_PlaneSRB)
        m_PlaneSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_ShadowMap")->Set(m_ShadowMapSRV);
}

void C6GERender::CreateMainRenderPass()
{
    m_UseRenderPasses = false;

    constexpr Uint32 NumAttachments = 2;

    RenderPassAttachmentDesc Attachments[NumAttachments];

    Attachments[0].Format       = m_pSwapChain->GetDesc().ColorBufferFormat;
    Attachments[0].SampleCount  = m_SampleCount;
    Attachments[0].InitialState = RESOURCE_STATE_RENDER_TARGET;
    Attachments[0].FinalState   = RESOURCE_STATE_RENDER_TARGET;
    Attachments[0].LoadOp       = ATTACHMENT_LOAD_OP_CLEAR;
    Attachments[0].StoreOp      = ATTACHMENT_STORE_OP_STORE;

    Attachments[1].Format       = m_pSwapChain->GetDesc().DepthBufferFormat;
    Attachments[1].SampleCount  = m_SampleCount;
    Attachments[1].InitialState = RESOURCE_STATE_DEPTH_WRITE;
    Attachments[1].FinalState   = RESOURCE_STATE_DEPTH_WRITE;
    Attachments[1].LoadOp       = ATTACHMENT_LOAD_OP_CLEAR;
    Attachments[1].StoreOp      = ATTACHMENT_STORE_OP_DISCARD;

    SubpassDesc Subpasses[1];

    AttachmentReference RTAttachmentRef    = {0, RESOURCE_STATE_RENDER_TARGET};
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

    if (!m_pMainRenderPass)
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

    ITextureView* pKey = m_pFramebufferRTV;

    auto fb_it = m_FramebufferCache.find(pKey);
    if (fb_it != m_FramebufferCache.end())
    {
        return fb_it->second;
    }

    auto pFramebuffer = CreateMainFramebuffer();
    if (pFramebuffer)
    {
        auto it = m_FramebufferCache.emplace(pKey, pFramebuffer);
        return it.first->second;
    }
    return nullptr;
}

void C6GERender::CreateFramebuffer()
{
    TextureDesc RTTexDesc;
    RTTexDesc.Name       = "Framebuffer render target";
    RTTexDesc.Type       = RESOURCE_DIM_TEX_2D;
    RTTexDesc.Width      = std::max(1u, m_FramebufferWidth);
    RTTexDesc.Height     = std::max(1u, m_FramebufferHeight);
    RTTexDesc.Format     = m_pSwapChain->GetDesc().ColorBufferFormat;
    RTTexDesc.Usage      = USAGE_DEFAULT;
    RTTexDesc.BindFlags  = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
    RTTexDesc.MipLevels  = 1;
    RTTexDesc.SampleCount = 1;
    RTTexDesc.ArraySize  = 1;

    m_pDevice->CreateTexture(RTTexDesc, nullptr, &m_pFramebufferTexture);

    TextureViewDesc RTVDesc;
    RTVDesc.ViewType = TEXTURE_VIEW_RENDER_TARGET;
    RTVDesc.Format   = RTTexDesc.Format;
    if (m_pFramebufferTexture)
    {
        m_pFramebufferTexture->CreateView(RTVDesc, &m_pFramebufferRTV);
    }
    else
    {
        m_pFramebufferRTV.Release();
    }

    TextureViewDesc SRVDesc;
    SRVDesc.ViewType = TEXTURE_VIEW_SHADER_RESOURCE;
    SRVDesc.Format   = RTTexDesc.Format;
    if (m_pFramebufferTexture)
        m_pFramebufferTexture->CreateView(SRVDesc, &m_pFramebufferSRV);

    m_pViewportDisplaySRV = m_pFramebufferSRV;

    TextureDesc DepthTexDesc;
    DepthTexDesc.Name       = "Framebuffer depth buffer";
    DepthTexDesc.Type       = RESOURCE_DIM_TEX_2D;
    DepthTexDesc.Width      = std::max(1u, m_FramebufferWidth);
    DepthTexDesc.Height     = std::max(1u, m_FramebufferHeight);
    DepthTexDesc.Format     = m_pSwapChain->GetDesc().DepthBufferFormat;
    DepthTexDesc.Usage      = USAGE_DEFAULT;
    DepthTexDesc.BindFlags  = BIND_DEPTH_STENCIL;
    DepthTexDesc.MipLevels  = 1;
    DepthTexDesc.SampleCount = 1;
    DepthTexDesc.ArraySize  = 1;

    m_pDevice->CreateTexture(DepthTexDesc, nullptr, &m_pFramebufferDepth);

    TextureViewDesc DSVDesc;
    DSVDesc.ViewType = TEXTURE_VIEW_DEPTH_STENCIL;
    DSVDesc.Format   = DepthTexDesc.Format;
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
    if (m_pFramebufferSRV)
        m_ViewportTextureID = reinterpret_cast<ImTextureID>(m_pFramebufferSRV.RawPtr());
    else
        m_ViewportTextureID = 0;

    if (Width == m_FramebufferWidth && Height == m_FramebufferHeight)
        return;

    m_FramebufferWidth  = Width;
    m_FramebufferHeight = Height;

    m_pFramebufferTexture.Release();
    m_pFramebufferRTV.Release();
    m_pFramebufferSRV.Release();
    m_pFramebufferDepth.Release();
    m_pFramebufferDSV.Release();

    m_pMSColorRTV.Release();
    m_pMSDepthDSV.Release();

    m_FramebufferCache.clear();

    CreateFramebuffer();
    CreateMSAARenderTarget();

    DestroyPostFXTargets();
    CreatePostFXTargets();
}

void C6GERender::CreateMSAARenderTarget()
{
    if (m_SampleCount == 1)
        return;

    const SwapChainDesc& SCDesc = m_pSwapChain->GetDesc();
    TextureDesc          ColorDesc;
    ColorDesc.Name      = "Multisampled render target";
    ColorDesc.Type      = RESOURCE_DIM_TEX_2D;
    ColorDesc.BindFlags = BIND_RENDER_TARGET;
    ColorDesc.Width     = m_FramebufferWidth;
    ColorDesc.Height    = m_FramebufferHeight;
    ColorDesc.MipLevels = 1;
    ColorDesc.Format    = SCDesc.ColorBufferFormat;
    bool NeedsSRGBConversion = m_pDevice->GetDeviceInfo().IsD3DDevice() &&
                               (ColorDesc.Format == TEX_FORMAT_RGBA8_UNORM_SRGB || ColorDesc.Format == TEX_FORMAT_BGRA8_UNORM_SRGB);
    if (NeedsSRGBConversion)
    {
        ColorDesc.Format = ColorDesc.Format == TEX_FORMAT_RGBA8_UNORM_SRGB ? TEX_FORMAT_RGBA8_TYPELESS : TEX_FORMAT_BGRA8_TYPELESS;
    }

    ColorDesc.SampleCount              = m_SampleCount;
    ColorDesc.ClearValue.Format        = SCDesc.ColorBufferFormat;
    ColorDesc.ClearValue.Color[0]      = 0.125f;
    ColorDesc.ClearValue.Color[1]      = 0.125f;
    ColorDesc.ClearValue.Color[2]      = 0.125f;
    ColorDesc.ClearValue.Color[3]      = 1.f;

    RefCntAutoPtr<ITexture> pColor;
    m_pDevice->CreateTexture(ColorDesc, nullptr, &pColor);
    m_pMSColorTex = pColor;

    m_pMSColorRTV.Release();
    if (NeedsSRGBConversion)
    {
        TextureViewDesc RTVDesc;
        RTVDesc.ViewType = TEXTURE_VIEW_RENDER_TARGET;
        RTVDesc.Format   = SCDesc.ColorBufferFormat;
        if (m_pMSColorTex)
            m_pMSColorTex->CreateView(RTVDesc, &m_pMSColorRTV);
    }
    else if (m_pMSColorTex)
    {
        m_pMSColorRTV = m_pMSColorTex->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
    }

    TextureDesc DepthDesc = ColorDesc;
    DepthDesc.Name        = "Multisampled depth buffer";
    DepthDesc.Format      = SCDesc.DepthBufferFormat;
    DepthDesc.BindFlags   = BIND_DEPTH_STENCIL;
    DepthDesc.ClearValue.Format               = DepthDesc.Format;
    DepthDesc.ClearValue.DepthStencil.Depth   = 1;
    DepthDesc.ClearValue.DepthStencil.Stencil = 0;

    RefCntAutoPtr<ITexture> pDepth;
    m_pDevice->CreateTexture(DepthDesc, nullptr, &pDepth);
    m_pMSDepthTex = pDepth;
    if (m_pMSDepthTex)
        m_pMSDepthDSV = m_pMSDepthTex->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
}

void C6GERender::CreatePostFXTargets()
{
    DestroyPostFXTargets();

    if (m_FramebufferWidth == 0 || m_FramebufferHeight == 0)
        return;

    TextureDesc Desc;
    Desc.Name      = "PostFX Color";
    Desc.Type      = RESOURCE_DIM_TEX_2D;
    Desc.Width     = m_FramebufferWidth;
    Desc.Height    = m_FramebufferHeight;
    Desc.MipLevels = 1;
    auto SCFmt     = m_pSwapChain->GetDesc().ColorBufferFormat;
    switch (SCFmt)
    {
        case TEX_FORMAT_RGBA8_UNORM_SRGB: Desc.Format = TEX_FORMAT_RGBA8_UNORM; break;
        case TEX_FORMAT_BGRA8_UNORM_SRGB: Desc.Format = TEX_FORMAT_BGRA8_UNORM; break;
        default: Desc.Format = SCFmt; break;
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
    m_PostGammaSRBCache.clear();
}

void C6GERender::CreatePostFXPSOs()
{
    ShaderCreateInfo CI;
    CI.SourceLanguage                  = SHADER_SOURCE_LANGUAGE_HLSL;
    CI.Desc.UseCombinedTextureSamplers = true;
    CI.CompileFlags                    = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    CI.pShaderSourceStreamFactory = pShaderSourceFactory;

    RefCntAutoPtr<IShader> pVS;
    RefCntAutoPtr<IShader> pPS;
    {
        CI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        CI.EntryPoint      = "main";
        CI.Desc.Name       = "PostFX VS";
        CI.FilePath        = "assets/rt_composite.vsh";
        m_pDevice->CreateShader(CI, &pVS);
    }
    {
        CI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        CI.EntryPoint      = "main";
        CI.Desc.Name       = "PostFX Gamma PS";
        CI.FilePath        = "assets/post_gamma.psh";
        ShaderMacro Macros[] = {{"APPLY_MANUAL_GAMMA", "1"}};
        CI.Macros          = {Macros, static_cast<Uint32>(_countof(Macros))};
        m_pDevice->CreateShader(CI, &pPS);
    }

    GraphicsPipelineStateCreateInfo PSOCI;
    PSOCI.PSODesc.Name                       = "PostFX Gamma PSO";
    PSOCI.PSODesc.PipelineType               = PIPELINE_TYPE_GRAPHICS;
    PSOCI.pVS                                = pVS;
    PSOCI.pPS                                = pPS;
    PSOCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCI.GraphicsPipeline.NumRenderTargets  = 1;
    PSOCI.GraphicsPipeline.RTVFormats[0]     = m_pPostTexture ? m_pPostTexture->GetDesc().Format : m_pSwapChain->GetDesc().ColorBufferFormat;
    PSOCI.GraphicsPipeline.DSVFormat         = TEX_FORMAT_UNKNOWN;
    PSOCI.GraphicsPipeline.DepthStencilDesc.DepthEnable      = False;
    PSOCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = False;
    PSOCI.GraphicsPipeline.RasterizerDesc.CullMode           = CULL_MODE_NONE;

    ShaderResourceVariableDesc Vars[] = {{SHADER_TYPE_PIXEL, "g_TextureColor", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};
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
    ImmutableSamplerDesc Imtbl[] = {{SHADER_TYPE_PIXEL, "g_TextureColor", Smpl}};
    PSOCI.PSODesc.ResourceLayout.ImmutableSamplers    = Imtbl;
    PSOCI.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(Imtbl);

    m_pDevice->CreateGraphicsPipelineState(PSOCI, &m_pPostGammaPSO);
    if (m_pPostGammaPSO)
        m_PostGammaSRBCache.clear();
}

} // namespace Diligent
