#include "RenderCommon.hpp"

namespace Diligent
{

bool RenderSettingsOpen = false;
bool RenderShadows      = false;

bool C6GERender::IsRuntime = false;
C6GERender::PlayState C6GERender::playState = C6GERender::PlayState::Paused;

void C6GERender::TogglePlayState()
{
    if (playState == PlayState::Paused)
        playState = PlayState::Playing;
    else
        playState = PlayState::Paused;
}

SampleBase* CreateSample()
{
    return new C6GERender();
}

C6GERender::~C6GERender()
{
    for (auto& srb : m_PostGammaSRBs)
        srb.Release();
    m_RTCompositeSRB.Release();
    m_RTAddCompositeSRB.Release();
    m_RTShadowSRB.Release();
    m_GLTFShadowSkinnedSRB.Release();
    m_GLTFShadowSRB.Release();
    m_PlaneNoShadowSRB.Release();
    m_PlaneSRB.Release();
    m_CubeShadowSRB.Release();
    m_CubeSRB.Release();

    m_PostGammaSRBCache.clear();
    m_FramebufferCache.clear();

    auto release_view = [](const char* name, RefCntAutoPtr<ITextureView>& v) {
        if (v)
        {
            printf("[C6GE][~C6GERender] Releasing view %s = %p\n", name, (void*)v.RawPtr());
            v.Release();
        }
    };
    auto release_tex = [](const char* name, RefCntAutoPtr<ITexture>& t) {
        if (t)
        {
            printf("[C6GE][~C6GERender] Releasing texture %s = %p\n", name, (void*)t.RawPtr());
            t.Release();
        }
    };

    release_view("m_pViewportDisplaySRV", m_pViewportDisplaySRV);
    release_view("m_PlayIconSRV", m_PlayIconSRV);
    release_view("m_PauseIconSRV", m_PauseIconSRV);
    release_view("m_MinimizeIconSRV", m_MinimizeIconSRV);
    release_view("m_MaximizeIconSRV", m_MaximizeIconSRV);
    release_view("m_CloseIconSRV", m_CloseIconSRV);
    release_view("m_EditorLogoSRV", m_EditorLogoSRV);

    release_view("m_pPostSRV", m_pPostSRV);
    release_view("m_pPostRTV", m_pPostRTV);

    release_view("m_pRTOutputSRV", m_pRTOutputSRV);
    release_view("m_pRTOutputUAV", m_pRTOutputUAV);

    release_view("m_pMSDepthDSV", m_pMSDepthDSV);
    release_view("m_pMSColorRTV", m_pMSColorRTV);
    release_view("m_pFramebufferDSV", m_pFramebufferDSV);
    release_view("m_pFramebufferRTV", m_pFramebufferRTV);
    release_view("m_pFramebufferSRV", m_pFramebufferSRV);

    release_view("m_ShadowMapSRV", m_ShadowMapSRV);
    release_view("m_ShadowMapDSV", m_ShadowMapDSV);

    release_view("m_TextureSRV", m_TextureSRV);

    release_tex("m_pMSDepthTex", m_pMSDepthTex);
    release_tex("m_pMSColorTex", m_pMSColorTex);
    release_tex("m_pFramebufferDepth", m_pFramebufferDepth);
    release_tex("m_pFramebufferTexture", m_pFramebufferTexture);
    release_tex("m_ShadowMapTex", m_ShadowMapTex);
    release_tex("m_FallbackWhiteTex", m_FallbackWhiteTex);
    release_tex("m_CubeTexture", m_CubeTexture);
    release_tex("m_pRTOutputTex", m_pRTOutputTex);
    release_tex("m_pPostTexture", m_pPostTexture);
    release_tex("m_PlayIconTex", m_PlayIconTex);
    release_tex("m_PauseIconTex", m_PauseIconTex);
    release_tex("m_MinimizeIconTex", m_MinimizeIconTex);
    release_tex("m_MaximizeIconTex", m_MaximizeIconTex);
    release_tex("m_CloseIconTex", m_CloseIconTex);
    release_tex("m_EditorLogoTex", m_EditorLogoTex);
}

void C6GERender::ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs)
{
    SampleBase::ModifyEngineInitInfo(Attribs);

    Attribs.EngineCI.Features.DepthClamp = DEVICE_FEATURE_STATE_OPTIONAL;

#if D3D12_SUPPORTED
    if (Attribs.DeviceType == RENDER_DEVICE_TYPE_D3D12)
    {
        EngineD3D12CreateInfo& D3D12CI = static_cast<EngineD3D12CreateInfo&>(Attribs.EngineCI);
        D3D12CI.GPUDescriptorHeapSize[1]        = 1024;
        D3D12CI.GPUDescriptorHeapDynamicSize[1] = 1024;
    }
#endif
}

void C6GERender::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    const TextureFormatInfoExt& ColorFmtInfo = m_pDevice->GetTextureFormatInfoExt(m_pSwapChain->GetDesc().ColorBufferFormat);
    const TextureFormatInfoExt& DepthFmtInfo = m_pDevice->GetTextureFormatInfoExt(m_pSwapChain->GetDesc().DepthBufferFormat);
    Uint32 supportedSampleCounts             = ColorFmtInfo.SampleCounts & DepthFmtInfo.SampleCounts;

    m_SupportedSampleCounts.clear();
    m_SupportedSampleCounts.push_back(1);
    if (supportedSampleCounts & SAMPLE_COUNT_2)
        m_SupportedSampleCounts.push_back(2);
    if (supportedSampleCounts & SAMPLE_COUNT_4)
        m_SupportedSampleCounts.push_back(4);
    if (supportedSampleCounts & SAMPLE_COUNT_8)
        m_SupportedSampleCounts.push_back(8);
    if (supportedSampleCounts & SAMPLE_COUNT_16)
        m_SupportedSampleCounts.push_back(16);

    if (supportedSampleCounts & SAMPLE_COUNT_4)
        m_SampleCount = 4;
    else if (supportedSampleCounts & SAMPLE_COUNT_2)
        m_SampleCount = 2;
    else
    {
        LOG_WARNING_MESSAGE(ColorFmtInfo.Name, " + ", DepthFmtInfo.Name, " pair does not allow multisampling on this device");
        m_SampleCount = 1;
    }

    TextureLoadInfo loadInfo;
    loadInfo.IsSRGB = true;
    try
    {
        CreateTextureFromFile("Editor/PlayIcon.png", loadInfo, m_pDevice, &m_PlayIconTex);
        if (m_PlayIconTex)
        {
            auto srv = m_PlayIconTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
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

    try
    {
        CreateTextureFromFile("Editor/PauseIcon.png", loadInfo, m_pDevice, &m_PauseIconTex);
        if (m_PauseIconTex)
        {
            auto srv = m_PauseIconTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
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

    auto loadWindowIcon = [&](const char* path,
                              RefCntAutoPtr<ITexture>& texture,
                              RefCntAutoPtr<ITextureView>& srv) {
        try
        {
            CreateTextureFromFile(path, loadInfo, m_pDevice, &texture);
            if (texture)
            {
                auto view = texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
                if (view)
                {
                    srv = view;
                }
                else
                {
                    std::cerr << "[C6GE] Failed to get SRV for " << path << std::endl;
                }
            }
            else
            {
                std::cerr << "[C6GE] Failed to load " << path << std::endl;
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "[C6GE] Exception loading " << path << ": " << e.what() << std::endl;
        }
    };

    loadWindowIcon("Editor/Minimize.png", m_MinimizeIconTex, m_MinimizeIconSRV);
    loadWindowIcon("Editor/Fullscreen.png", m_MaximizeIconTex, m_MaximizeIconSRV);
    loadWindowIcon("Editor/Exit.png", m_CloseIconTex, m_CloseIconSRV);
    loadWindowIcon("C6GELogo.png", m_EditorLogoTex, m_EditorLogoSRV);

    m_Camera.SetPos(float3(0.f, 0.f, 5.f));
    m_Camera.SetRotation(0.f, 0.f);

    {
        const auto& DevInfo   = m_pDevice->GetDeviceInfo();
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
    CreatePlanePSO();

    m_CubeVertexBuffer = TexturedCube::CreateVertexBuffer(m_pDevice, GEOMETRY_PRIMITIVE_VERTEX_FLAG_ALL);
    m_CubeIndexBuffer  = TexturedCube::CreateIndexBuffer(m_pDevice);
    CreatePlaneMeshBuffers();

    try
    {
        m_CubeTexture = TexturedCube::LoadTexture(m_pDevice, "C6GELogo.png");
        if (m_CubeTexture)
        {
            m_TextureSRV = m_CubeTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
            if (!m_TextureSRV)
            {
                std::cerr << "[C6GE] Failed to get SRV for C6GELogo.png" << std::endl;
            }
        }
        else
        {
            std::cerr << "[C6GE] Failed to load texture: C6GELogo.png" << std::endl;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "[C6GE] Exception loading texture: " << e.what() << std::endl;
    }

    if (m_CubeSRB && m_TextureSRV)
    {
        auto textureVar = m_CubeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture");
        if (textureVar)
            textureVar->Set(m_TextureSRV);
        else
            std::cerr << "[C6GE] Failed to find g_Texture variable in SRB" << std::endl;
    }
    else if (m_CubeSRB && !m_TextureSRV)
    {
        try
        {
            TextureDesc Desc;
            Desc.Name      = "Fallback White Texture";
            Desc.Type      = RESOURCE_DIM_TEX_2D;
            Desc.Width     = 1;
            Desc.Height    = 1;
            Desc.MipLevels = 1;
            Desc.Format    = TEX_FORMAT_RGBA8_UNORM;
            Desc.BindFlags = BIND_SHADER_RESOURCE;
            const Uint8 White[4] = {255, 255, 255, 255};
            TextureSubResData SubRes;
            SubRes.pData  = White;
            SubRes.Stride = 4;
            TextureData InitData;
            InitData.pSubResources  = &SubRes;
            InitData.NumSubresources = 1;
            m_pDevice->CreateTexture(Desc, &InitData, &m_FallbackWhiteTex);
            if (m_FallbackWhiteTex)
            {
                m_TextureSRV = m_FallbackWhiteTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
                if (auto* textureVar = m_CubeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture"))
                    textureVar->Set(m_TextureSRV);
            }
        }
        catch (...)
        {
        }
    }

    if (m_pSwapChain)
    {
        const auto& scDesc = m_pSwapChain->GetDesc();
        if (scDesc.Width > 0 && scDesc.Height > 0)
        {
            m_FramebufferWidth  = scDesc.Width;
            m_FramebufferHeight = scDesc.Height;
        }
        else
        {
            m_FramebufferWidth  = std::max(1u, m_FramebufferWidth);
            m_FramebufferHeight = std::max(1u, m_FramebufferHeight);
        }
    }
    else
    {
        std::cerr << "[C6GE] Warning: SwapChain is null during initialization" << std::endl;
        m_FramebufferWidth  = std::max(1u, m_FramebufferWidth);
        m_FramebufferHeight = std::max(1u, m_FramebufferHeight);
    }

    try
    {
        CreateFramebuffer();
    }
    catch (const std::exception& e)
    {
        std::cerr << "[C6GE] Exception creating framebuffer: " << e.what() << std::endl;
        throw;
    }

    try
    {
        CreatePostFXTargets();
        CreatePostFXPSOs();
    }
    catch (const std::exception& e)
    {
        std::cerr << "[C6GE] Exception creating post-processing pipeline: " << e.what() << std::endl;
    }

    try
    {
        m_PostFXContext = std::make_unique<PostFXContext>(m_pDevice, PostFXContext::CreateInfo{true, true});
        m_TAA           = std::make_unique<TemporalAntiAliasing>(m_pDevice, TemporalAntiAliasing::CreateInfo{true});

        auto& TAASettingsRef                         = *reinterpret_cast<TAASettings*>(&m_TAASettingsStorage);
        TAASettingsRef.TemporalStabilityFactor       = 0.9375f;
        TAASettingsRef.ResetAccumulation             = FALSE;
        TAASettingsRef.SkipRejection                 = FALSE;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[C6GE] Exception creating TAA: " << e.what() << std::endl;
        m_PostFXContext.reset();
        m_TAA.reset();
    }

    try
    {
        CreateMSAARenderTarget();
    }
    catch (const std::exception& e)
    {
        std::cerr << "[C6GE] Exception creating MSAA render target: " << e.what() << std::endl;
        throw;
    }

    try
    {
        CreateShadowMap();
    }
    catch (const std::exception& e)
    {
        std::cerr << "[C6GE] Exception creating shadow map: " << e.what() << std::endl;
        throw;
    }

    try
    {
        CreateMainRenderPass();
    }
    catch (const std::exception& e)
    {
        std::cerr << "[C6GE] Exception creating main render pass: " << e.what() << std::endl;
        m_UseRenderPasses = false;
    }

    StateTransitionDesc Barriers[] = {
        {m_VSConstants, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE},
        {m_CubeVertexBuffer, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_VERTEX_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE},
        {m_CubeIndexBuffer, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_INDEX_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE}
    };
    m_pImmediateContext->TransitionResourceStates(_countof(Barriers), Barriers);

    printf("[Initialize] Cube VB=%p IB=%p\n", (void*)m_CubeVertexBuffer.RawPtr(), (void*)m_CubeIndexBuffer.RawPtr());

    EnsureWorld();

    try
    {
        m_Project = std::make_unique<ProjectSystem::ProjectManager>();
        namespace fs = std::filesystem;
        fs::path start = fs::current_path();
        auto proj      = ProjectSystem::ProjectManager::FindNearestProject(start);
        if (!proj.empty())
        {
            if (m_Project->Load(proj))
            {
                std::cout << "[C6GE] Loaded project: " << m_Project->GetConfig().projectName << "\n";
            }
        }
        else
        {
            fs::path def = start / "C6GEProject.c6proj";
            if (m_Project->CreateDefault(def, "C6GE Project"))
            {
                std::cout << "[C6GE] Created default project at " << def.string() << "\n";
            }
        }
        if (m_Project)
        {
            auto sw = m_Project->GetConfig().startupWorld;
            if (!sw.empty() && fs::exists(sw) && m_World)
            {
                class Adapter : public ProjectSystem::ECSWorldLike
                {
                    Diligent::ECS::World& W;

                public:
                    explicit Adapter(Diligent::ECS::World& w) : W(w) {}
                    void Clear() override
                    {
                        auto& reg = W.Registry();
                        std::vector<entt::entity> to_destroy;
                        auto view = reg.view<ECS::Name>();
                        for (auto e : view)
                            to_destroy.push_back(e);
                        for (auto e : to_destroy)
                            W.DestroyEntity(e);
                    }
                    void* CreateObject(const std::string& name) override
                    {
                        auto obj = W.CreateObject(name);
                        return reinterpret_cast<void*>(static_cast<uintptr_t>(obj.Handle()));
                    }
                    void SetTransform(void* handle, const ProjectSystem::ECSWorldLike::TransformData& t) override
                    {
                        auto e = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(handle));
                        auto& reg = W.Registry();
                        ECS::Transform tr;
                        tr.position      = float3{t.position[0], t.position[1], t.position[2]};
                        tr.rotationEuler = float3{t.rotationEuler[0], t.rotationEuler[1], t.rotationEuler[2]};
                        tr.scale         = float3{t.scale[0], t.scale[1], t.scale[2]};
                        reg.emplace_or_replace<ECS::Transform>(e, tr);
                    }
                    void SetMesh(void* handle, ProjectSystem::ECSWorldLike::MeshKind kind, const std::string& assetId) override
                    {
                        auto e   = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(handle));
                        auto& reg = W.Registry();
                        if (kind == ProjectSystem::ECSWorldLike::MeshKind::StaticCube)
                        {
                            reg.emplace_or_replace<ECS::StaticMesh>(e, ECS::StaticMesh{ECS::StaticMesh::MeshType::Cube});
                        }
                        else if (kind == ProjectSystem::ECSWorldLike::MeshKind::DynamicGLTF)
                        {
                            ECS::Mesh m;
                            m.kind    = ECS::Mesh::Kind::Dynamic;
                            m.assetId = assetId;
                            reg.emplace_or_replace<ECS::Mesh>(e, m);
                        }
                    }
                    void SetDirectionalLight(void* handle, const ProjectSystem::ECSWorldLike::DirectionalLightData& data) override
                    {
                        auto e   = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(handle));
                        auto& reg = W.Registry();
                        ECS::DirectionalLight l;
                        l.direction = float3{data.direction[0], data.direction[1], data.direction[2]};
                        l.intensity = data.intensity;
                        reg.emplace_or_replace<ECS::DirectionalLight>(e, l);
                    }
                    void SetPointLight(void* handle, const ProjectSystem::ECSWorldLike::PointLightData& data) override
                    {
                        auto e   = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(handle));
                        auto& reg = W.Registry();
                        ECS::PointLight l;
                        l.color     = float3{data.color[0], data.color[1], data.color[2]};
                        l.intensity = data.intensity;
                        l.range     = data.range;
                        reg.emplace_or_replace<ECS::PointLight>(e, l);
                    }
                    void SetSpotLight(void* handle, const ProjectSystem::ECSWorldLike::SpotLightData& data) override
                    {
                        auto e   = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(handle));
                        auto& reg = W.Registry();
                        ECS::SpotLight l;
                        l.direction    = float3{data.direction[0], data.direction[1], data.direction[2]};
                        l.color        = float3{data.color[0], data.color[1], data.color[2]};
                        l.intensity    = data.intensity;
                        l.range        = data.range;
                        l.angleDegrees = data.angleDegrees;
                        reg.emplace_or_replace<ECS::SpotLight>(e, l);
                    }
                    void SetCamera(void* handle, const ProjectSystem::ECSWorldLike::CameraData& data) override
                    {
                        auto e   = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(handle));
                        auto& reg = W.Registry();
                        ECS::Camera cam;
                        cam.fovYRadians = data.fovYRadians;
                        cam.nearZ       = data.nearZ;
                        cam.farZ        = data.farZ;
                        reg.emplace_or_replace<ECS::Camera>(e, cam);
                    }
                    std::vector<ProjectSystem::ECSWorldLike::ObjectViewItem> EnumerateObjects() const override
                    {
                        std::vector<ProjectSystem::ECSWorldLike::ObjectViewItem> out;
                        return out;
                    }
                } adapter(*m_World);
                ProjectSystem::WorldIO::Load(sw, adapter);
            }
        }
    }
    catch (...)
    {
        std::cerr << "[C6GE] Project initialization failed (non-fatal)." << std::endl;
    }
}

void C6GERender::Update(double CurrTime, double ElapsedTime, bool DoUpdateUI)
{
    SampleBase::Update(CurrTime, ElapsedTime, DoUpdateUI);

    if (!IsPlaying())
        return;

    m_CubeWorldMatrix      = float4x4::Identity();
    m_SecondCubeWorldMatrix = float4x4::Identity();

    float4x4 View = float4x4::Translation(0.f, 0.0f, 5.0f);

    float4x4 SrfPreTransform      = GetSurfacePretransformMatrix(float3{0, 0, 1});
    float4x4 Proj                 = GetAdjustedProjectionMatrix(PI_F / 4.0f, 0.1f, 100.f);
    m_ViewMatrix                  = View * SrfPreTransform;
    m_ProjMatrix                  = Proj;
    m_CameraViewProjMatrix        = m_ViewMatrix * m_ProjMatrix;
}

void C6GERender::EnsureWorld()
{
    if (!m_World)
        m_World = std::make_unique<ECS::World>();
    if (!m_RenderSystem)
        m_RenderSystem = std::make_unique<C6GE::Systems::RenderSystem>(this);
}

void C6GERender::WindowResize(Uint32 Width, Uint32 Height)
{
    if (!m_pSwapChain || !m_pDevice)
        return;

    float NearPlane    = 0.1f;
    float FarPlane     = 250.f;
    float AspectRatio  = static_cast<float>(Width) / static_cast<float>(Height);
    m_Camera.SetProjAttribs(NearPlane, FarPlane, AspectRatio, PI_F / 4.f,
                            m_pSwapChain->GetDesc().PreTransform, m_pDevice->GetDeviceInfo().NDC.MinZ == -1);

    const auto& scDesc = m_pSwapChain->GetDesc();
    Uint32 targetW     = scDesc.Width;
    Uint32 targetH     = scDesc.Height;

    if (targetW != m_FramebufferWidth || targetH != m_FramebufferHeight)
    {
        ResizeFramebuffer(targetW, targetH);
        if (m_RayTracingSupported && m_EnableRayTracing)
        {
            m_PendingRTRestart = true;
        }
        m_TemporalFrameIndex = 0;
    }
}

} // namespace Diligent
