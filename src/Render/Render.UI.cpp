#include "RenderCommon.hpp"
#include <cfloat>
#include <cstring>
#include <system_error>

#ifndef GLFW_INCLUDE_NONE
#    define GLFW_INCLUDE_NONE
#endif
#include "GLFW/glfw3.h"

namespace Diligent
{
namespace
{
ImGuiID g_ProjectConsoleDockId      = 0;
bool    g_ProjectDockNeedsDock      = false;
bool    g_ConsoleDockNeedsDock      = false;
} // namespace

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
    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));
        float x = 0.0f, y = 0.0f;
        if (ImGui::FindWindowByName("Viewport"))
        {
            ImGuiWindow* vpWin = ImGui::FindWindowByName("Viewport");
            ImVec2 vpPos       = vpWin->Pos;
            ImVec2 vpSize      = vpWin->Size;
            const float iconSize  = 40.0f;
            const float windowPad = 12.0f;
            float barWidth        = iconSize + 2 * windowPad;
            x = vpPos.x + (vpSize.x - barWidth) * 0.5f;
            float menuBarHeight = ImGui::GetFrameHeight();
            y = vpPos.y + menuBarHeight + 4.0f;
            if (y < vpPos.y)
                y = vpPos.y;
        }
        ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
        ImGui::Begin("PlayPauseBar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);
        if (ImGui::FindWindowByName("PlayPauseBar"))
        {
            ImGui::BringWindowToDisplayFront(ImGui::FindWindowByName("PlayPauseBar"));
        }

        bool usedIcon        = false;
        const float iconSize = 40.0f;
        if (m_PlayIconSRV && m_PauseIconSRV)
        {
            auto playSRV  = m_PlayIconSRV.RawPtr();
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
            if (ImGui::Button(IsPaused() ? "Play" : "Pause", ImVec2(iconSize * 2, iconSize)))
            {
                TogglePlayState();
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(3);
    }
    catch (const std::exception& e)
    {
        std::cerr << "[C6GE] Exception in UpdateUI play/pause bar: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "[C6GE] Unknown exception in UpdateUI play/pause bar." << std::endl;
    }

    if (g_ProjectConsoleDockId != 0 && g_ProjectDockNeedsDock)
        ImGui::SetNextWindowDockID(g_ProjectConsoleDockId, ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(380.0f, 240.0f), ImVec2(FLT_MAX, FLT_MAX));
    if (ImGui::Begin("Project", nullptr, ImGuiWindowFlags_NoCollapse))
    {
        if (g_ProjectConsoleDockId != 0 && g_ProjectDockNeedsDock && ImGui::GetWindowDockID() == g_ProjectConsoleDockId)
            g_ProjectDockNeedsDock = false;
        if (m_Project)
        {
            const auto& cfg = m_Project->GetConfig();
            ImGui::Text("Project: %s", cfg.projectName.c_str());
            ImGui::TextDisabled("Engine %s", cfg.engineVersion.c_str());
            if (ImGui::Button("Save Project"))
            {
                m_Project->Save();
            }
            ImGui::SameLine();
            if (ImGui::Button("Save World"))
            {
                if (m_World)
                {
                    class Adapter : public ProjectSystem::ECSWorldLike
                    {
                        Diligent::ECS::World& W;

                    public:
                        explicit Adapter(Diligent::ECS::World& w) : W(w) {}
                        void Clear() override {}
                        void* CreateObject(const std::string&) override { return nullptr; }
                        void SetTransform(void*, const TransformData&) override {}
                        void SetMesh(void*, MeshKind, const std::string&) override {}
                        void SetDirectionalLight(void*, const DirectionalLightData&) override {}
                        void SetPointLight(void*, const PointLightData&) override {}
                        void SetSpotLight(void*, const SpotLightData&) override {}
                        void SetCamera(void*, const CameraData&) override {}
                        void SetSky(void*, const SkyData&) override {}
                        void SetFog(void*, const FogData&) override {}
                        void SetSkyLight(void*, const SkyLightData&) override {}
                        std::vector<ObjectViewItem> EnumerateObjects() const override
                        {
                            std::vector<ObjectViewItem> out;
                            auto& reg  = W.Registry();
                            auto  view = reg.view<ECS::Name>();
                            for (auto e : view)
                            {
                                ObjectViewItem it;
                                it.handle       = reinterpret_cast<void*>(static_cast<uintptr_t>(e));
                                it.name         = view.get<ECS::Name>(e).value;
                                it.hasTransform = reg.any_of<ECS::Transform>(e);
                                if (it.hasTransform)
                                {
                                    const auto& tr = reg.get<ECS::Transform>(e);
                                    it.tr.position[0]      = tr.position.x;
                                    it.tr.position[1]      = tr.position.y;
                                    it.tr.position[2]      = tr.position.z;
                                    it.tr.rotationEuler[0] = tr.rotationEuler.x;
                                    it.tr.rotationEuler[1] = tr.rotationEuler.y;
                                    it.tr.rotationEuler[2] = tr.rotationEuler.z;
                                    it.tr.scale[0]         = tr.scale.x;
                                    it.tr.scale[1]         = tr.scale.y;
                                    it.tr.scale[2]         = tr.scale.z;
                                }
                                it.meshKind = MeshKind::None;
                                it.assetId.clear();
                                if (reg.any_of<ECS::StaticMesh>(e))
                                    it.meshKind = MeshKind::StaticCube;
                                if (reg.any_of<ECS::Mesh>(e))
                                {
                                    const auto& m = reg.get<ECS::Mesh>(e);
                                    if (m.kind == ECS::Mesh::Kind::Static && m.staticType == ECS::Mesh::StaticType::Cube)
                                        it.meshKind = MeshKind::StaticCube;
                                    else if (m.kind == ECS::Mesh::Kind::Dynamic)
                                    {
                                        it.meshKind = MeshKind::DynamicGLTF;
                                        it.assetId  = m.assetId;
                                    }
                                }
                                it.hasDirectionalLight = reg.any_of<ECS::DirectionalLight>(e);
                                if (it.hasDirectionalLight)
                                {
                                    const auto& l = reg.get<ECS::DirectionalLight>(e);
                                    it.directionalLight.direction[0] = l.direction.x;
                                    it.directionalLight.direction[1] = l.direction.y;
                                    it.directionalLight.direction[2] = l.direction.z;
                                    it.directionalLight.intensity    = l.intensity;
                                }
                                it.hasPointLight = reg.any_of<ECS::PointLight>(e);
                                if (it.hasPointLight)
                                {
                                    const auto& l = reg.get<ECS::PointLight>(e);
                                    it.pointLight.color[0] = l.color.x;
                                    it.pointLight.color[1] = l.color.y;
                                    it.pointLight.color[2] = l.color.z;
                                    it.pointLight.intensity = l.intensity;
                                    it.pointLight.range     = l.range;
                                }
                                it.hasSpotLight = reg.any_of<ECS::SpotLight>(e);
                                if (it.hasSpotLight)
                                {
                                    const auto& l = reg.get<ECS::SpotLight>(e);
                                    it.spotLight.direction[0] = l.direction.x;
                                    it.spotLight.direction[1] = l.direction.y;
                                    it.spotLight.direction[2] = l.direction.z;
                                    it.spotLight.color[0]     = l.color.x;
                                    it.spotLight.color[1]     = l.color.y;
                                    it.spotLight.color[2]     = l.color.z;
                                    it.spotLight.intensity    = l.intensity;
                                    it.spotLight.range        = l.range;
                                    it.spotLight.angleDegrees = l.angleDegrees;
                                }
                                it.hasCamera = reg.any_of<ECS::Camera>(e);
                                if (it.hasCamera)
                                {
                                    const auto& cam = reg.get<ECS::Camera>(e);
                                    it.camera.fovYRadians = cam.fovYRadians;
                                    it.camera.nearZ       = cam.nearZ;
                                    it.camera.farZ        = cam.farZ;
                                }
                                it.hasSky = reg.any_of<ECS::Sky>(e);
                                if (it.hasSky)
                                {
                                    const auto& sky = reg.get<ECS::Sky>(e);
                                    it.sky.enabled         = sky.enabled;
                                    it.sky.environmentPath = sky.environmentPath;
                                    it.sky.intensity       = sky.intensity;
                                    it.sky.exposure        = sky.exposure;
                                    it.sky.rotationDegrees = sky.rotationDegrees;
                                    it.sky.showBackground  = sky.showBackground;
                                }
                                it.hasFog = reg.any_of<ECS::Fog>(e);
                                if (it.hasFog)
                                {
                                    const auto& fog = reg.get<ECS::Fog>(e);
                                    it.fog.enabled        = fog.enabled;
                                    it.fog.color[0]       = fog.color.x;
                                    it.fog.color[1]       = fog.color.y;
                                    it.fog.color[2]       = fog.color.z;
                                    it.fog.density        = fog.density;
                                    it.fog.startDistance  = fog.startDistance;
                                    it.fog.maxDistance    = fog.maxDistance;
                                    it.fog.heightFalloff  = fog.heightFalloff;
                                }
                                it.hasSkyLight = reg.any_of<ECS::SkyLight>(e);
                                if (it.hasSkyLight)
                                {
                                    const auto& skyLight = reg.get<ECS::SkyLight>(e);
                                    it.skyLight.enabled   = skyLight.enabled;
                                    it.skyLight.color[0]  = skyLight.color.x;
                                    it.skyLight.color[1]  = skyLight.color.y;
                                    it.skyLight.color[2]  = skyLight.color.z;
                                    it.skyLight.intensity = skyLight.intensity;
                                }
                                out.push_back(std::move(it));
                            }
                            return out;
                        }
                    } adapter(*m_World);
                    ProjectSystem::WorldIO::Save(cfg.startupWorld, adapter);
                }
            }
            ImGui::Separator();
            if (ImGui::CollapsingHeader("Worlds", ImGuiTreeNodeFlags_DefaultOpen))
            {
                auto worlds = m_Project->ListWorldFiles();
                for (auto& w : worlds)
                {
                    auto label = w.filename().string();
                    if (ImGui::Selectable(label.c_str(), false))
                    {
                        if (m_World)
                        {
                            class Adapter : public ProjectSystem::ECSWorldLike
                            {
                                Diligent::ECS::World& W;

                            public:
                                explicit Adapter(Diligent::ECS::World& w) : W(w) {}
                                void Clear() override
                                {
                                    auto& reg = W.Registry();
                                    std::vector<entt::entity> v;
                                    auto view = reg.view<ECS::Name>();
                                    for (auto e : view)
                                        v.push_back(e);
                                    for (auto e : v)
                                        W.DestroyEntity(e);
                                }
                                void* CreateObject(const std::string& name) override
                                {
                                    auto obj = W.CreateObject(name);
                                    return reinterpret_cast<void*>(static_cast<uintptr_t>(obj.Handle()));
                                }
                                void SetTransform(void* h, const ProjectSystem::ECSWorldLike::TransformData& t) override
                                {
                                    auto  e   = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(h));
                                    auto& reg = W.Registry();
                                    ECS::Transform tr;
                                    tr.position      = float3{t.position[0], t.position[1], t.position[2]};
                                    tr.rotationEuler = float3{t.rotationEuler[0], t.rotationEuler[1], t.rotationEuler[2]};
                                    tr.scale         = float3{t.scale[0], t.scale[1], t.scale[2]};
                                    reg.emplace_or_replace<ECS::Transform>(e, tr);
                                }
                                void SetMesh(void* h, ProjectSystem::ECSWorldLike::MeshKind kind, const std::string& assetId) override
                                {
                                    auto  e   = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(h));
                                    auto& reg = W.Registry();
                                    if (kind == ProjectSystem::ECSWorldLike::MeshKind::StaticCube)
                                        reg.emplace_or_replace<ECS::StaticMesh>(e, ECS::StaticMesh{ECS::StaticMesh::MeshType::Cube});
                                    else if (kind == ProjectSystem::ECSWorldLike::MeshKind::DynamicGLTF)
                                    {
                                        ECS::Mesh m;
                                        m.kind    = ECS::Mesh::Kind::Dynamic;
                                        m.assetId = assetId;
                                        reg.emplace_or_replace<ECS::Mesh>(e, m);
                                    }
                                }
                                void SetDirectionalLight(void* h, const DirectionalLightData& data) override
                                {
                                    auto  e   = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(h));
                                    auto& reg = W.Registry();
                                    ECS::DirectionalLight l;
                                    l.direction = float3{data.direction[0], data.direction[1], data.direction[2]};
                                    l.intensity = data.intensity;
                                    reg.emplace_or_replace<ECS::DirectionalLight>(e, l);
                                }
                                void SetPointLight(void* h, const PointLightData& data) override
                                {
                                    auto  e   = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(h));
                                    auto& reg = W.Registry();
                                    ECS::PointLight l;
                                    l.color     = float3{data.color[0], data.color[1], data.color[2]};
                                    l.intensity = data.intensity;
                                    l.range     = data.range;
                                    reg.emplace_or_replace<ECS::PointLight>(e, l);
                                }
                                void SetSpotLight(void* h, const SpotLightData& data) override
                                {
                                    auto  e   = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(h));
                                    auto& reg = W.Registry();
                                    ECS::SpotLight l;
                                    l.direction    = float3{data.direction[0], data.direction[1], data.direction[2]};
                                    l.color        = float3{data.color[0], data.color[1], data.color[2]};
                                    l.intensity    = data.intensity;
                                    l.range        = data.range;
                                    l.angleDegrees = data.angleDegrees;
                                    reg.emplace_or_replace<ECS::SpotLight>(e, l);
                                }
                                void SetCamera(void* h, const CameraData& data) override
                                {
                                    auto  e   = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(h));
                                    auto& reg = W.Registry();
                                    ECS::Camera cam;
                                    cam.fovYRadians = data.fovYRadians;
                                    cam.nearZ       = data.nearZ;
                                    cam.farZ        = data.farZ;
                                    reg.emplace_or_replace<ECS::Camera>(e, cam);
                                }
                                void SetSky(void* h, const SkyData& data) override
                                {
                                    auto  e   = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(h));
                                    auto& reg = W.Registry();
                                    ECS::Sky sky;
                                    sky.enabled         = data.enabled;
                                    sky.environmentPath = data.environmentPath;
                                    sky.intensity       = data.intensity;
                                    sky.exposure        = data.exposure;
                                    sky.rotationDegrees = data.rotationDegrees;
                                    sky.showBackground  = data.showBackground;
                                    reg.emplace_or_replace<ECS::Sky>(e, sky);
                                }
                                void SetFog(void* h, const FogData& data) override
                                {
                                    auto  e   = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(h));
                                    auto& reg = W.Registry();
                                    ECS::Fog fog;
                                    fog.enabled       = data.enabled;
                                    fog.color         = float3{data.color[0], data.color[1], data.color[2]};
                                    fog.density       = data.density;
                                    fog.startDistance = data.startDistance;
                                    fog.maxDistance   = data.maxDistance;
                                    fog.heightFalloff = data.heightFalloff;
                                    reg.emplace_or_replace<ECS::Fog>(e, fog);
                                }
                                void SetSkyLight(void* h, const SkyLightData& data) override
                                {
                                    auto  e   = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(h));
                                    auto& reg = W.Registry();
                                    ECS::SkyLight skyLight;
                                    skyLight.enabled   = data.enabled;
                                    skyLight.color     = float3{data.color[0], data.color[1], data.color[2]};
                                    skyLight.intensity = data.intensity;
                                    reg.emplace_or_replace<ECS::SkyLight>(e, skyLight);
                                }
                                std::vector<ProjectSystem::ECSWorldLike::ObjectViewItem> EnumerateObjects() const override { return {}; }
                            } adapter(*m_World);
                            ProjectSystem::WorldIO::Load(w, adapter);
                            m_Project->GetConfig().startupWorld = w;
                        }
                    }
                }
                static char worldName[128] = "NewWorld";
                ImGui::InputText("##WorldName", worldName, sizeof(worldName));
                ImGui::SameLine();
                if (ImGui::Button("New World"))
                {
                    auto path = cfg.worldsDir / (std::string(worldName) + ".world");
                    if (m_World)
                    {
                        class Adapter : public ProjectSystem::ECSWorldLike
                        {
                        public:
                            explicit Adapter(Diligent::ECS::World&) {}
                            void Clear() override {}
                            void* CreateObject(const std::string&) override { return nullptr; }
                            void SetTransform(void*, const TransformData&) override {}
                            void SetMesh(void*, MeshKind, const std::string&) override {}
                            void SetDirectionalLight(void*, const DirectionalLightData&) override {}
                            void SetPointLight(void*, const PointLightData&) override {}
                            void SetSpotLight(void*, const SpotLightData&) override {}
                            void SetCamera(void*, const CameraData&) override {}
                            void SetSky(void*, const SkyData&) override {}
                            void SetFog(void*, const FogData&) override {}
                            void SetSkyLight(void*, const SkyLightData&) override {}
                            std::vector<ObjectViewItem> EnumerateObjects() const override { return {}; }
                        } adapter(*m_World);
                        ProjectSystem::WorldIO::Save(path, adapter);
                        m_Project->GetConfig().startupWorld = path;
                    }
                }
            }
            if (ImGui::CollapsingHeader("Models", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ImGui::Button("Import glTF..."))
                {
                    std::string picked;
                    if (Platform::OpenFileDialogGLTF(picked))
                    {
                        auto out = m_Project->ConvertGLTFToC6M(picked);
                        (void)out;
                    }
                }
                auto models = m_Project->ListModelFiles();
                for (auto& m : models)
                {
                    bool sel = false;
                    ImGui::Selectable(m.filename().string().c_str(), &sel);
                    if (ImGui::BeginPopupContextItem())
                    {
                        if (ImGui::MenuItem("Reveal in Explorer"))
                        {
                        }
                        ImGui::EndPopup();
                    }
                }
            }
        }
        else
        {
            ImGui::TextDisabled("No project loaded");
        }
    }
    ImGui::End();

    if (RenderSettingsOpen)
    {
    ImGui::SetNextWindowSizeConstraints(ImVec2(420.0f, 300.0f), ImVec2(FLT_MAX, FLT_MAX));
    if (ImGui::Begin("Render Settings", &RenderSettingsOpen, ImGuiWindowFlags_NoCollapse))
        {
            if (ImGui::BeginTabBar("RenderSettingsTabs"))
            {
                if (ImGui::BeginTabItem("Raster"))
                {
                    ImGui::SliderFloat("Line Width", &m_LineWidth, 1.f, 10.f);

                    if (ImGui::CollapsingHeader("Anti-Aliasing", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        if (!m_SupportedSampleCounts.empty())
                        {
                            std::vector<const char*> sampleCountNames;
                            std::vector<Uint8>       sampleCountValues;

                            for (Uint8 sampleCount : m_SupportedSampleCounts)
                            {
                                static std::string names[8];
                                int                index = static_cast<int>(sampleCountNames.size());
                                if (sampleCount == 1)
                                    names[index] = "Disabled (1x)";
                                else
                                    names[index] = std::to_string(sampleCount) + "x MSAA";

                                sampleCountNames.push_back(names[index].c_str());
                                sampleCountValues.push_back(sampleCount);
                            }

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
                        static int shadowRes = static_cast<int>(m_ShadowMapSize);
                        if (ImGui::SliderInt("Shadow Resolution", &shadowRes, 512, 8192, "%d"))
                        {
                            if (shadowRes != static_cast<int>(m_ShadowMapSize))
                            {
                                m_ShadowMapSize = shadowRes;
                                CreateShadowMap();
                            }
                        }
                    }

                    if (ImGui::CollapsingHeader("Advanced", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        if (m_pMainRenderPass)
                        {
                            if (ImGui::Checkbox("Use Render Passes", &m_UseRenderPasses))
                            {
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
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Ray Tracing"))
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

                    ImGui::Separator();
                    ImGui::Text("Shadows");
                    ImGui::Checkbox("Soft Shadows", &m_SoftShadowsEnabled);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(?)");
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::BeginTooltip();
                        ImGui::Text("Soft shadows sample multiple jittered shadow rays within an angular cone.");
                        ImGui::EndTooltip();
                    }
                    if (m_SoftShadowsEnabled)
                    {
                        ImGui::SliderFloat("Light Angular Radius (rad)", &m_SoftShadowAngularRad, 0.0f, 0.2f, "%.3f");
                        ImGui::SliderInt("Soft Shadow Samples", &m_SoftShadowSamples, 1, 64);
                    }
                    else
                    {
                        ImGui::TextDisabled("Hard shadows active (1 sample, 0 cone radius)");
                    }

                    ImGui::SameLine();
                    ImGui::TextDisabled("(?)");
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::BeginTooltip();
                        ImGui::Text("Set samples to 1 and radius to 0 for hard shadows. Higher samples improve quality at a cost.");
                        ImGui::EndTooltip();
                    }

                    ImGui::Separator();
                    ImGui::Text("Reflections");
                    ImGui::Checkbox("Enable Reflections (Plane)", &m_EnableRTReflections);

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Post Processing"))
                {
                    ImGui::Checkbox("Enable Post Processing", &m_EnablePostProcessing);
                    if (!m_pPostGammaPSO)
                        ImGui::TextDisabled("Gamma PSO not available (using fallback)");
                    ImGui::Checkbox("Gamma Correction", &m_PostGammaCorrection);
                    if (m_TAA)
                    {
                        ImGui::Separator();
                        ImGui::Checkbox("Enable TAA (Temporal AA)", &m_EnableTAA);
                        auto& TAA = *reinterpret_cast<TAASettings*>(&m_TAASettingsStorage);
                        ImGui::SliderFloat("Temporal Stability", &TAA.TemporalStabilityFactor, 0.0f, 0.999f, "%.4f");
                        bool reset = (TAA.ResetAccumulation != 0);
                        if (ImGui::Checkbox("Reset Accumulation", &reset))
                            TAA.ResetAccumulation = reset ? TRUE : FALSE;
                        bool skip = (TAA.SkipRejection != 0);
                        if (ImGui::Checkbox("Skip Rejection", &skip))
                            TAA.SkipRejection = skip ? TRUE : FALSE;
                    }
                    ImGui::TextDisabled("This sample applies a fullscreen gamma-correction pass to the framebuffer.\nMore effects (tone mapping, bloom) can be added next.");
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }
}

void C6GERender::UpdateViewportUI()
{
    if (ImGui::GetCurrentContext() == nullptr)
    {
        std::cerr << "[C6GE] ImGui context not ready, skipping viewport UI update." << std::endl;
        return;
    }
    if (!(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable))
    {
        std::cerr << "[C6GE] ImGui docking not enabled, skipping viewport UI update." << std::endl;
        return;
    }

    auto* viewport = ImGui::GetMainViewport();
    const ImGuiStyle& style = ImGui::GetStyle();
    const float frameHeight = ImGui::GetFrameHeight();

    static bool  open_settings = false;
    static float font_size     = 16.0f;
    static ImVec4 bg_color     = ImVec4(0.07f, 0.07f, 0.07f, 1.0f);
    static ImVec4 text_color   = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

    if (ImGui::BeginMainMenuBar())
    {
        const float titleRowY = ImGui::GetCursorPosY();
        if (m_EditorLogoSRV)
        {
            const float logoHeight = frameHeight * 0.9f;
            const float logoWidth  = logoHeight;
            const float logoOffset = (frameHeight - logoHeight) * 0.5f;
            ImGui::SetCursorPosY(titleRowY + logoOffset);
            ImGui::Image(ImTextureRef(m_EditorLogoSRV.RawPtr()), ImVec2(logoWidth, logoHeight));
            ImGui::SameLine();
            const float versionScale = 1.3f;
            ImGui::SetWindowFontScale(versionScale);
            float versionTextHeight = ImGui::GetTextLineHeight();
            ImGui::SetCursorPosY(titleRowY + (frameHeight - versionTextHeight) * 0.5f);
            ImGui::TextUnformatted("2026.1");
            ImGui::SetWindowFontScale(1.0f);
        }
        else
        {
            const float versionScale = 1.3f;
            ImGui::SetWindowFontScale(versionScale);
            float versionTextHeight = ImGui::GetTextLineHeight();
            ImGui::SetCursorPosY(titleRowY + (frameHeight - versionTextHeight) * 0.5f);
            ImGui::TextUnformatted("2026.1");
            ImGui::SetWindowFontScale(1.0f);
        }
        ImGui::SameLine();
        ImGui::SetCursorPosY(titleRowY);
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Settings"))
            {
                open_settings = true;
            }
            if (ImGui::MenuItem("Exit"))
            {
                if (m_HostWindow != nullptr)
                    glfwSetWindowShouldClose(m_HostWindow, GLFW_TRUE);
            }
            ImGui::EndMenu();
        }

        const float baseY   = ImGui::GetCursorPosY();
        const float cursorX = ImGui::GetCursorPosX();
        const char* closeLabel    = "X";
        const char* minimizeLabel = "_";
        bool        isMaximized   = false;
        if (m_HostWindow != nullptr)
            isMaximized = glfwGetWindowAttrib(m_HostWindow, GLFW_MAXIMIZED) == GLFW_TRUE;
        const char* maximizeLabel = isMaximized ? "<>" : "[]";

        const bool hasMinimizeIcon = m_MinimizeIconSRV != nullptr;
        const bool hasMaximizeIcon = m_MaximizeIconSRV != nullptr;
        const bool hasCloseIcon    = m_CloseIconSRV != nullptr;

        const float iconScale = 0.54f;
        float       iconSide  = frameHeight * iconScale;
        if (iconSide < frameHeight * 0.48f)
            iconSide = frameHeight * 0.48f;
        const ImVec2 iconContentSize(iconSide, iconSide);
        float        padY = style.FramePadding.y - 2.0f;
        if (padY < 0.0f)
            padY = 0.0f;
        ImVec2       iconPadding(style.FramePadding.x, padY);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, iconPadding);
        const float buttonHeight      = iconContentSize.y + iconPadding.y * 2.0f;
        float       iconVerticalOffset = (frameHeight - buttonHeight) * 0.5f;
        if (iconVerticalOffset < 0.0f)
            iconVerticalOffset = 0.0f;
        const float iconButtonWidth = iconContentSize.x + iconPadding.x * 2.0f;

        const float closeWidth    = hasCloseIcon ? iconButtonWidth : ImGui::CalcTextSize(closeLabel).x + style.FramePadding.x * 2.0f;
        const float maximizeWidth = hasMaximizeIcon ? iconButtonWidth : ImGui::CalcTextSize(maximizeLabel).x + style.FramePadding.x * 2.0f;
        const float minimizeWidth = hasMinimizeIcon ? iconButtonWidth : ImGui::CalcTextSize(minimizeLabel).x + style.FramePadding.x * 2.0f;
        const float totalButtonsWidth = minimizeWidth + maximizeWidth + closeWidth + style.ItemSpacing.x * 2.0f;
        const float buttonsStart      = ImGui::GetWindowContentRegionMax().x - totalButtonsWidth;
        float       dragWidth         = buttonsStart - cursorX;
        if (dragWidth < 0.0f)
            dragWidth = 0.0f;

        if (dragWidth > 0.0f)
        {
            ImGui::SetCursorPos(ImVec2(cursorX, baseY));
            ImGui::InvisibleButton("##TitleDragZone", ImVec2(dragWidth, frameHeight));
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                if (m_HostWindow != nullptr)
                {
                    const ImVec2 delta = ImGui::GetIO().MouseDelta;
                    if (delta.x != 0.0f || delta.y != 0.0f)
                    {
                        int wx = 0;
                        int wy = 0;
                        glfwGetWindowPos(m_HostWindow, &wx, &wy);
                        glfwSetWindowPos(m_HostWindow,
                                         wx + static_cast<int>(delta.x),
                                         wy + static_cast<int>(delta.y));
                    }
                }
            }
            if (ImGui::IsItemHovered())
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        }
        else
        {
            ImGui::SetCursorPos(ImVec2(cursorX, baseY));
            ImGui::Dummy(ImVec2(0.0f, frameHeight));
        }

        float buttonCursor = buttonsStart;
        ImGui::SetCursorPos(ImVec2(buttonCursor, hasMinimizeIcon ? baseY + iconVerticalOffset : baseY));
        if (hasMinimizeIcon)
        {
            if (ImGui::ImageButton("##MinimizeEditorWindow", ImTextureRef(m_MinimizeIconSRV ? m_MinimizeIconSRV.RawPtr() : nullptr), iconContentSize))
            {
                if (m_HostWindow != nullptr)
                    glfwIconifyWindow(m_HostWindow);
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Minimize");
        }
        else if (ImGui::Button(minimizeLabel, ImVec2(minimizeWidth, frameHeight)))
        {
            if (m_HostWindow != nullptr)
                glfwIconifyWindow(m_HostWindow);
        }

        buttonCursor += minimizeWidth + style.ItemSpacing.x;
        ImGui::SetCursorPos(ImVec2(buttonCursor, hasMaximizeIcon ? baseY + iconVerticalOffset : baseY));
        bool paintedMaximizeState = false;
        if (hasMaximizeIcon)
        {
            if (isMaximized)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.46f, 0.82f, 0.90f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.54f, 0.92f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.40f, 0.70f, 1.0f));
                paintedMaximizeState = true;
            }
            if (ImGui::ImageButton("##MaximizeEditorWindow", ImTextureRef(m_MaximizeIconSRV ? m_MaximizeIconSRV.RawPtr() : nullptr), iconContentSize))
            {
                if (m_HostWindow != nullptr)
                {
                    if (isMaximized)
                        glfwRestoreWindow(m_HostWindow);
                    else
                        glfwMaximizeWindow(m_HostWindow);
                }
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::SetTooltip(isMaximized ? "Restore Down" : "Maximize");
            }
            if (paintedMaximizeState)
                ImGui::PopStyleColor(3);
        }
        else if (ImGui::Button(maximizeLabel, ImVec2(maximizeWidth, frameHeight)))
        {
            if (m_HostWindow != nullptr)
            {
                if (isMaximized)
                    glfwRestoreWindow(m_HostWindow);
                else
                    glfwMaximizeWindow(m_HostWindow);
            }
        }

        buttonCursor += maximizeWidth + style.ItemSpacing.x;
    ImGui::SetCursorPos(ImVec2(buttonCursor, hasCloseIcon ? baseY + iconVerticalOffset : baseY));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.82f, 0.24f, 0.24f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.33f, 0.33f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.70f, 0.18f, 0.18f, 1.0f));
        if (hasCloseIcon)
        {
            if (ImGui::ImageButton("##CloseEditorWindow", ImTextureRef(m_CloseIconSRV ? m_CloseIconSRV.RawPtr() : nullptr), iconContentSize))
            {
                if (m_HostWindow != nullptr)
                    glfwSetWindowShouldClose(m_HostWindow, GLFW_TRUE);
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Close");
        }
        else if (ImGui::Button(closeLabel, ImVec2(closeWidth, frameHeight)))
        {
            if (m_HostWindow != nullptr)
                glfwSetWindowShouldClose(m_HostWindow, GLFW_TRUE);
        }
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        ImGui::EndMainMenuBar();
    }

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags dockspace_window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpaceWindow", nullptr, dockspace_window_flags);
    ImGui::PopStyleVar(3);

    if (open_settings)
    {
        ImGui::SetNextWindowDockID(ImGui::GetID("MainDockSpace"), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(480, 360), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(420.0f, 300.0f), ImVec2(FLT_MAX, FLT_MAX));
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
                    if (ImGui::Button("Apply", ImVec2(0, 0)))
                    {
                        ImGui::GetStyle().Colors[ImGuiCol_WindowBg] = bg_color;
                        ImGui::GetStyle().Colors[ImGuiCol_Text]     = text_color;
                        auto& io                                   = ImGui::GetIO();
                        io.FontGlobalScale                         = font_size / 16.0f;
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

    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspace_id);
    if (node)
        node->LocalFlags &= ~ImGuiDockNodeFlags_AutoHideTabBar;

    static bool first_time = true;
    if (first_time)
    {
        first_time = false;
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

            auto dock_left  = ImGui::DockBuilderSplitNode(dockspace_id, (ImGuiDir)ImGuiDir_Left, 0.2f, nullptr, &dockspace_id);
            auto dock_right = ImGui::DockBuilderSplitNode(dockspace_id, (ImGuiDir)ImGuiDir_Right, 0.25f, nullptr, &dockspace_id);
            auto dock_down  = ImGui::DockBuilderSplitNode(dockspace_id, (ImGuiDir)ImGuiDir_Down, 0.3f, nullptr, &dockspace_id);
            g_ProjectConsoleDockId = dock_down;
            g_ProjectDockNeedsDock = true;
            g_ConsoleDockNeedsDock = true;

            ImGui::DockBuilderDockWindow("Viewport", dockspace_id);
            ImGui::DockBuilderDockWindow("Scene", dock_left);
            ImGui::DockBuilderDockWindow("Properties", dock_right);
            ImGui::DockBuilderDockWindow("Console", dock_down);
            ImGui::DockBuilderDockWindow("Project", dock_down);
            ImGui::DockBuilderDockWindow("Settings", dock_right);

            ImGui::DockBuilderFinish(dockspace_id);
        }
    }

    ImGui::End();

    ImGui::SetNextWindowSizeConstraints(ImVec2(480.0f, 320.0f), ImVec2(FLT_MAX, FLT_MAX));
    if (ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoCollapse))
    {
        if (m_pFramebufferSRV)
        {
            auto contentSize = ImGui::GetContentRegionAvail();

            Uint32 newWidth  = static_cast<Uint32>(std::max(1.0f, contentSize.x));
            Uint32 newHeight = static_cast<Uint32>(std::max(1.0f, contentSize.y));

            if (newWidth != m_FramebufferWidth || newHeight != m_FramebufferHeight)
            {
                ResizeFramebuffer(newWidth, newHeight);
                std::cout << "[C6GE] Resized framebuffer to " << newWidth << "x" << newHeight << std::endl;
            }

            ITextureView* pDisplaySRV = m_pViewportDisplaySRV ? m_pViewportDisplaySRV.RawPtr() : m_pFramebufferSRV.RawPtr();
            ImGui::Image(reinterpret_cast<void*>(pDisplaySRV), contentSize);

            const ImVec2 imgMin = ImGui::GetItemRectMin();
            const ImVec2 imgMax = ImGui::GetItemRectMax();
            const ImVec2 imgSize = ImVec2(imgMax.x - imgMin.x, imgMax.y - imgMin.y);

            const ImVec2 mousePos        = ImGui::GetIO().MousePos;
            const bool   mouseOverImage  = (mousePos.x >= imgMin.x && mousePos.x <= imgMax.x && mousePos.y >= imgMin.y && mousePos.y <= imgMax.y);

            if (mouseOverImage && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                const float ndcX = (mousePos.x - imgMin.x) / imgSize.x * 2.0f - 1.0f;
                const float ndcY = 1.0f - (mousePos.y - imgMin.y) / imgSize.y * 2.0f;
                const float4 ndcNear = float4{ndcX, ndcY, 0.0f, 1.0f};
                const float4 ndcFar  = float4{ndcX, ndcY, 1.0f, 1.0f};
                const auto   InvVP   = m_CameraViewProjMatrix.Inverse();
                float4       wNear   = ndcNear * InvVP;
                float4       wFar    = ndcFar * InvVP;
                if (wNear.w != 0)
                    wNear = wNear / wNear.w;
                if (wFar.w != 0)
                    wFar = wFar / wFar.w;
                const float3 RayOrigin = float3{wNear.x, wNear.y, wNear.z};
                const float3 RayDir    = normalize(float3{wFar.x - wNear.x, wFar.y - wNear.y, wFar.z - wNear.z});

                EnsureWorld();
                entt::entity best = entt::null;
                float        bestT = +FLT_MAX;
                if (m_World)
                {
                    auto& reg = m_World->Registry();
                    auto  view = reg.view<ECS::Transform, ECS::StaticMesh>();
                    for (auto e : view)
                    {
                        const auto& sm = view.get<ECS::StaticMesh>(e);
                        const auto& tr = view.get<ECS::Transform>(e);
                        const float4x4 World    = tr.WorldMatrix();
                        const float4x4 InvWorld = World.Inverse();
                        const float4   ro4      = float4{RayOrigin.x, RayOrigin.y, RayOrigin.z, 1.0f} * InvWorld;
                        const float4   rd4      = float4{RayDir.x, RayDir.y, RayDir.z, 0.0f} * InvWorld;
                        const float3   rayOObj  = float3{ro4.x, ro4.y, ro4.z};
                        const float3   rayDObj  = normalize(float3{rd4.x, rd4.y, rd4.z});

                        BoundBox AABB{};
                        if (sm.type == ECS::StaticMesh::MeshType::Cube)
                        {
                            AABB = BoundBox{float3{-1.f, -1.f, -1.f}, float3{+1.f, +1.f, +1.f}};
                        }
                        else if (sm.type == ECS::StaticMesh::MeshType::Plane)
                        {
                            AABB = BoundBox{float3{-1.f, -0.01f, -1.f}, float3{+1.f, +0.01f, +1.f}};
                        }
                        else
                        {
                            continue;
                        }
                        float tEnter = 0, tExit = 0;
                        if (IntersectRayAABB(rayOObj, rayDObj, AABB, tEnter, tExit))
                        {
                            if (tEnter >= 0 && tEnter < bestT)
                            {
                                bestT = tEnter;
                                best  = e;
                            }
                        }
                    }

                    auto viewGltf = reg.view<ECS::Transform, ECS::Mesh>();
                    for (auto e : viewGltf)
                    {
                        const auto& m = viewGltf.get<ECS::Mesh>(e);
                        if (m.kind == ECS::Mesh::Kind::Dynamic)
                        {
                            if (m.assetId.empty())
                                continue;
                            auto itAsset = m_GltfAssets.find(m.assetId);
                            if (itAsset == m_GltfAssets.end())
                                continue;
                            const BoundBox& AABB = itAsset->second.Bounds;
                            if (AABB.Min.x > AABB.Max.x || AABB.Min.y > AABB.Max.y || AABB.Min.z > AABB.Max.z)
                                continue;
                            const auto& tr = viewGltf.get<ECS::Transform>(e);
                            const float4x4 World    = tr.WorldMatrix();
                            const float4x4 InvWorld = World.Inverse();
                            const float4   ro4      = float4{RayOrigin.x, RayOrigin.y, RayOrigin.z, 1.0f} * InvWorld;
                            const float4   rd4      = float4{RayDir.x, RayDir.y, RayDir.z, 0.0f} * InvWorld;
                            const float3   rayOObj  = float3{ro4.x, ro4.y, ro4.z};
                            const float3   rayDObj  = normalize(float3{rd4.x, rd4.y, rd4.z});
                            float          tEnter = 0, tExit = 0;
                            if (IntersectRayAABB(rayOObj, rayDObj, AABB, tEnter, tExit))
                            {
                                if (tEnter >= 0 && tEnter < bestT)
                                {
                                    bestT = tEnter;
                                    best  = e;
                                }
                            }
                        }
                        else if (m.kind == ECS::Mesh::Kind::Static)
                        {
                            const auto& tr = viewGltf.get<ECS::Transform>(e);
                            const float4x4 World    = tr.WorldMatrix();
                            const float4x4 InvWorld = World.Inverse();
                            const float4   ro4      = float4{RayOrigin.x, RayOrigin.y, RayOrigin.z, 1.0f} * InvWorld;
                            const float4   rd4      = float4{RayDir.x, RayDir.y, RayDir.z, 0.0f} * InvWorld;
                            const float3   rayOObj  = float3{ro4.x, ro4.y, ro4.z};
                            const float3   rayDObj  = normalize(float3{rd4.x, rd4.y, rd4.z});
                            BoundBox       AABB;
                            if (m.staticType == ECS::Mesh::StaticType::Cube)
                                AABB = BoundBox{float3{-1.f, -1.f, -1.f}, float3{+1.f, +1.f, +1.f}};
                            else
                                AABB = BoundBox{float3{-1.f, -0.01f, -1.f}, float3{+1.f, +0.01f, +1.f}};
                            float tEnter = 0, tExit = 0;
                            if (IntersectRayAABB(rayOObj, rayDObj, AABB, tEnter, tExit))
                            {
                                if (tEnter >= 0 && tEnter < bestT)
                                {
                                    bestT = tEnter;
                                    best  = e;
                                }
                            }
                        }
                    }
                }
                if (best != entt::null)
                {
                    m_SelectedEntity = best;
                }
            }

            ImGuizmo::BeginFrame();
            ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
            ImGuizmo::SetOrthographic(false);
            ImGuizmo::SetRect(imgMin.x, imgMin.y, imgSize.x, imgSize.y);

            if (mouseOverImage && ImGui::IsWindowFocused())
            {
                if (ImGui::IsKeyPressed(ImGuiKey_W))
                    m_GizmoOperation = GizmoOperation::Translate;
                if (ImGui::IsKeyPressed(ImGuiKey_E))
                    m_GizmoOperation = GizmoOperation::Rotate;
                if (ImGui::IsKeyPressed(ImGuiKey_R))
                    m_GizmoOperation = GizmoOperation::Scale;
            }

            EnsureWorld();
            if (m_World)
            {
                auto& reg = m_World->Registry();
                if (m_SelectedEntity != entt::null && reg.valid(m_SelectedEntity) && reg.any_of<ECS::Transform>(m_SelectedEntity))
                {
                    auto& tr = reg.get<ECS::Transform>(m_SelectedEntity);

                    ImDrawList* dl  = ImGui::GetWindowDrawList();
                    const float pad = 6.0f;
                    ImVec2      tbPos = ImVec2(imgMin.x + pad, imgMin.y + pad);
                    ImGui::SetCursorScreenPos(tbPos);
                    ImGui::BeginGroup();
                    auto button = [&](const char* label, bool active) {
                        if (active)
                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.05f, 0.80f, 0.75f, 0.35f));
                        bool pressed = ImGui::Button(label);
                        if (active)
                            ImGui::PopStyleColor();
                        ImGui::SameLine(0, 4);
                        return pressed;
                    };
                    bool t = (m_GizmoOperation == GizmoOperation::Translate);
                    bool r = (m_GizmoOperation == GizmoOperation::Rotate);
                    bool s = (m_GizmoOperation == GizmoOperation::Scale);
                    if (button("T", t))
                        m_GizmoOperation = GizmoOperation::Translate;
                    if (button("R", r))
                        m_GizmoOperation = GizmoOperation::Rotate;
                    if (button("S", s))
                        m_GizmoOperation = GizmoOperation::Scale;
                    ImGui::EndGroup();

                    if (m_GizmoOperation == GizmoOperation::Rotate && m_LastGizmoOperation != GizmoOperation::Rotate)
                    {
                        const QuaternionF qx = QuaternionF::RotationFromAxisAngle(float3{1, 0, 0}, tr.rotationEuler.x);
                        const QuaternionF qy = QuaternionF::RotationFromAxisAngle(float3{0, 1, 0}, tr.rotationEuler.y);
                        const QuaternionF qz = QuaternionF::RotationFromAxisAngle(float3{0, 0, 1}, tr.rotationEuler.z);
                        const QuaternionF q  = qx * qy * qz;
                        float3             axis{};
                        float              angle = 0;
                        QuaternionF(q).GetAxisAngle(axis, angle);
                        if (length(axis) == 0)
                            axis = float3{1, 0, 0};
                        m_GizmoAxisAngle = float4{axis.x, axis.y, axis.z, angle};
                        m_GizmoEntityLast = m_SelectedEntity;
                    }

                    auto BuildRotationOnly = [](const float3& euler) -> float3x3 {
                        const float3x3 Rx = float3x3::RotationX(euler.x);
                        const float3x3 Ry = float3x3::RotationY(euler.y);
                        const float3x3 Rz = float3x3::RotationZ(euler.z);
                        return Rx * Ry * Rz;
                    };

                    const float3x3 R = BuildRotationOnly(tr.rotationEuler);
                    float3          ex{1, 0, 0};
                    float3          ey{0, 1, 0};
                    float3          ez{0, 0, 1};
                    const float3    axesLocalW[3] = {
                        normalize(m_GizmoLocal ? (R * ex) : ex),
                        normalize(m_GizmoLocal ? (R * ey) : ey),
                        normalize(m_GizmoLocal ? (R * ez) : ez)};

                    auto WorldToScreen = [&](const float3& p) -> ImVec2 {
                        const float4 hp = float4{p.x, p.y, p.z, 1.0f} * m_CameraViewProjMatrix;
                        float2       ndc = float2{hp.x, hp.y} / (hp.w != 0.0f ? hp.w : 1.0f);
                        ImVec2       uv  = ImVec2(0.5f * ndc.x + 0.5f, 0.5f * -ndc.y + 0.5f);
                        return ImVec2(imgMin.x + uv.x * imgSize.x, imgMin.y + uv.y * imgSize.y);
                    };

                    const float3 objPosW  = tr.position;
                    const ImVec2 originSS = WorldToScreen(objPosW);

                    const float axisLenPx = 90.0f;
                    ImU32       axisCol[3] = {IM_COL32(235, 70, 70, 255), IM_COL32(70, 235, 90, 255), IM_COL32(70, 140, 235, 255)};
                    ImU32       axisColHL[3] = {IM_COL32(255, 140, 140, 255), IM_COL32(140, 255, 170, 255), IM_COL32(140, 200, 255, 255)};

                    auto BuildRay = [&](const ImVec2& mp) {
                        const float ndcX = (mp.x - imgMin.x) / imgSize.x * 2.0f - 1.0f;
                        const float ndcY = 1.0f - (mp.y - imgMin.y) / imgSize.y * 2.0f;
                        const float4 ndcNear = float4{ndcX, ndcY, 0.0f, 1.0f};
                        const float4 ndcFar  = float4{ndcX, ndcY, 1.0f, 1.0f};
                        const auto   InvVP   = m_CameraViewProjMatrix.Inverse();
                        float4       wNear   = ndcNear * InvVP;
                        float4       wFar    = ndcFar * InvVP;
                        if (wNear.w != 0)
                            wNear = wNear / wNear.w;
                        if (wFar.w != 0)
                            wFar = wFar / wFar.w;
                        float3 ro = float3{wNear.x, wNear.y, wNear.z};
                        float3 rd = normalize(float3{wFar.x - wNear.x, wFar.y - wNear.y, wFar.z - wNear.z});
                        return std::pair<float3, float3>{ro, rd};
                    };

                    const float4x4 InvView = m_ViewMatrix.Inverse();
                    float4         CamOrigin4 = float4{0, 0, 0, 1} * InvView;
                    const float3   CamPos     = float3{CamOrigin4.x, CamOrigin4.y, CamOrigin4.z};
                    const float3   CamDir     = normalize(objPosW - CamPos);
                    float4         CamRight4  = float4{1, 0, 0, 0} * InvView;
                    const float3   CamRight   = normalize(float3{CamRight4.x, CamRight4.y, CamRight4.z});
                    float4         CamFwd4    = float4{0, 0, -1, 0} * InvView;
                    const float3   CamForward = normalize(float3{CamFwd4.x, CamFwd4.y, CamFwd4.z});

                    int   hoveredAxis = -1;
                    float bestDist    = 1e9f;
                    struct AxisVis
                    {
                        ImVec2 a, b;
                        ImU32  col;
                    } vis[3]{};
                    for (int i = 0; i < 3; ++i)
                    {
                        const ImVec2 p0 = originSS;
                        ImVec2       end;
                        if (i == 2)
                        {
                            end = ImVec2(p0.x + axisLenPx, p0.y);
                        }
                        else if (i == 0)
                        {
                            const ImVec2 p1 = WorldToScreen(objPosW + CamForward);
                            ImVec2       dir = ImVec2(p1.x - p0.x, p1.y - p0.y);
                            const float  len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                            if (len < 1e-3f)
                                dir = ImVec2(0, -1);
                            else
                            {
                                dir.x /= len;
                                dir.y /= len;
                            }
                            end = ImVec2(p0.x + dir.x * axisLenPx, p0.y + dir.y * axisLenPx);
                        }
                        else
                        {
                            const ImVec2 p1 = WorldToScreen(objPosW + axesLocalW[i]);
                            ImVec2       dir = ImVec2(p1.x - p0.x, p1.y - p0.y);
                            const float  len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                            if (len < 1e-3f)
                                dir = ImVec2(1, 0);
                            else
                            {
                                dir.x /= len;
                                dir.y /= len;
                            }
                            end = ImVec2(p0.x + dir.x * axisLenPx, p0.y + dir.y * axisLenPx);
                        }
                        vis[i] = {p0, end, axisCol[i]};
                        auto distPtSeg = [](ImVec2 p, ImVec2 a, ImVec2 b) {
                            ImVec2 ab{b.x - a.x, b.y - a.y};
                            ImVec2 ap{p.x - a.x, p.y - a.y};
                            float  t = (ab.x * ap.x + ab.y * ap.y) / (ab.x * ab.x + ab.y * ab.y + 1e-5f);
                            t        = std::max(0.0f, std::min(1.0f, t));
                            ImVec2 c{a.x + ab.x * t, a.y + ab.y * t};
                            ImVec2 d{p.x - c.x, p.y - c.y};
                            return std::sqrt(d.x * d.x + d.y * d.y);
                        };
                        const float d = distPtSeg(mousePos, p0, end);
                        if (mouseOverImage && d < 12.0f && d < bestDist)
                        {
                            bestDist   = d;
                            hoveredAxis = i;
                        }
                    }

                    if (!m_GizmoDragging && mouseOverImage && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hoveredAxis != -1)
                    {
                        m_GizmoDragging = true;
                        m_GizmoAxis     = hoveredAxis;
                        if (hoveredAxis == 2)
                            m_GizmoAxisDirW = CamRight;
                        else if (hoveredAxis == 0)
                            m_GizmoAxisDirW = CamForward;
                        else
                            m_GizmoAxisDirW = axesLocalW[hoveredAxis];
                        m_GizmoStartPosW = tr.position;
                        m_GizmoStartScale = tr.scale;
                        const float3 CamDir0 = normalize(objPosW - CamPos);
                        m_GizmoDragPlaneNormal = normalize(cross(m_GizmoAxisDirW, cross(CamDir0, m_GizmoAxisDirW)));
                        m_GizmoDragPlanePoint  = m_GizmoStartPosW;
                        auto [ro0, rd0]        = BuildRay(mousePos);
                        float denom            = dot(rd0, m_GizmoDragPlaneNormal);
                        float t                = 0.0f;
                        if (std::abs(denom) > 1e-4f)
                        {
                            t = dot((m_GizmoDragPlanePoint - ro0), m_GizmoDragPlaneNormal) / denom;
                        }
                        const float3 hit = ro0 + rd0 * t;
                        m_GizmoStartT = dot(hit - m_GizmoDragPlanePoint, m_GizmoAxisDirW);
                    }

                    if (m_GizmoDragging)
                    {
                        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                        {
                            m_GizmoDragging = false;
                        }
                        else
                        {
                            const float3 axis = m_GizmoAxisDirW;
                            auto          [ro, rd] = BuildRay(mousePos);
                            float denom            = dot(rd, m_GizmoDragPlaneNormal);
                            float t                = 0.0f;
                            if (std::abs(denom) > 1e-4f)
                            {
                                t = dot((m_GizmoDragPlanePoint - ro), m_GizmoDragPlaneNormal) / denom;
                            }
                            const float3 hit   = ro + rd * t;
                            const float  currT = dot(hit - m_GizmoDragPlanePoint, axis);
                            const float  deltaT = currT - m_GizmoStartT;
                            if (m_GizmoOperation == GizmoOperation::Translate)
                            {
                                tr.position = m_GizmoStartPosW + axis * deltaT;
                            }
                            else if (m_GizmoOperation == GizmoOperation::Scale)
                            {
                                float3 newScale = m_GizmoStartScale;
                                if (std::abs(axis.x) > 0.5f)
                                    newScale.x = std::max(0.0001f, m_GizmoStartScale.x + deltaT);
                                if (std::abs(axis.y) > 0.5f)
                                    newScale.y = std::max(0.0001f, m_GizmoStartScale.y + deltaT);
                                if (std::abs(axis.z) > 0.5f)
                                    newScale.z = std::max(0.0001f, m_GizmoStartScale.z + deltaT);
                                tr.scale = newScale;
                            }
                        }
                    }

                    if (m_GizmoOperation == GizmoOperation::Translate || m_GizmoOperation == GizmoOperation::Scale)
                    {
                        for (int i = 0; i < 3; ++i)
                        {
                            const bool active = (m_GizmoDragging && i == m_GizmoAxis) || (!m_GizmoDragging && i == hoveredAxis);
                            const ImU32 col   = active ? axisColHL[i] : axisCol[i];
                            dl->AddLine(vis[i].a, vis[i].b, IM_COL32(0, 0, 0, 120), 7.0f);
                            dl->AddLine(vis[i].a, vis[i].b, col, 5.0f);
                            ImVec2 ab{vis[i].b.x - vis[i].a.x, vis[i].b.y - vis[i].a.y};
                            float  L = std::sqrt(ab.x * ab.x + ab.y * ab.y);
                            if (L > 1.0f)
                            {
                                ImVec2 dir{ab.x / L, ab.y / L};
                                ImVec2 ort{-dir.y, dir.x};
                                float  ah = 14.0f;
                                ImVec2 p0 = ImVec2(vis[i].b.x, vis[i].b.y);
                                ImVec2 p1 = ImVec2(vis[i].b.x - dir.x * ah + ort.x * ah * 0.5f, vis[i].b.y - dir.y * ah + ort.y * ah * 0.5f);
                                ImVec2 p2 = ImVec2(vis[i].b.x - dir.x * ah - ort.x * ah * 0.5f, vis[i].b.y - dir.y * ah - ort.y * ah * 0.5f);
                                ImU32 shadow = IM_COL32(0, 0, 0, 120);
                                dl->AddTriangleFilled(p0, p1, p2, shadow);
                                dl->AddTriangleFilled(p0, p1, p2, col);
                            }
                        }
                    }
                    else if (m_GizmoOperation == GizmoOperation::Rotate || m_GizmoOperation == GizmoOperation::Scale)
                    {
                        auto ToFloatArray = [](const float4x4& M) {
                            std::array<float, 16> a{};
                            std::memcpy(a.data(), &M, sizeof(float) * 16);
                            return a;
                        };

                        auto viewM  = ToFloatArray(m_ViewMatrix);
                        auto projM  = ToFloatArray(m_ProjMatrix);
                        float4x4 world = tr.WorldMatrix();
                        auto     modelM = ToFloatArray(world);

                        ImGuizmo::OPERATION op = (m_GizmoOperation == GizmoOperation::Rotate) ? ImGuizmo::ROTATE : ImGuizmo::SCALE;
                        ImGuizmo::MODE      mode = m_GizmoLocal ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

                        ImGuizmo::Manipulate(viewM.data(), projM.data(), op, mode, modelM.data());

                        if (ImGuizmo::IsUsing())
                        {
                            float translation[3], rotationDeg[3], scale[3];
                            ImGuizmo::DecomposeMatrixToComponents(modelM.data(), translation, rotationDeg, scale);
                            tr.position = float3{translation[0], translation[1], translation[2]};
                            tr.scale    = float3{std::max(0.0001f, scale[0]), std::max(0.0001f, scale[1]), std::max(0.0001f, scale[2])};
                            const float deg2rad = PI_F / 180.0f;
                            tr.rotationEuler     = float3{rotationDeg[0] * deg2rad, rotationDeg[1] * deg2rad, rotationDeg[2] * deg2rad};
                        }
                    }

                    m_LastGizmoOperation = m_GizmoOperation;
                }
            }
        }
        else
        {
            ImGui::Text("Framebuffer not ready");
        }
    }
    ImGui::End();

    ImGui::SetNextWindowSizeConstraints(ImVec2(360.0f, 280.0f), ImVec2(FLT_MAX, FLT_MAX));
    if (ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::Text("Scene Hierarchy");
        ImGui::Separator();

        EnsureWorld();
        if (m_World)
        {
            static char newNameBuf[128] = "NewObject";
            ImGui::InputText("##NewName", newNameBuf, sizeof(newNameBuf));
            ImGui::SameLine();
            if (ImGui::Button("+ Empty"))
            {
                std::string base = (strlen(newNameBuf) > 0) ? std::string(newNameBuf) : std::string("Object");
                std::string name = base;
                int         suffix = 1;
                while (m_World->HasObject(name))
                {
                    name = base + " (" + std::to_string(suffix++) + ")";
                }
                try
                {
                    auto obj = m_World->CreateObject(name);
                    auto& reg = m_World->Registry();
                    reg.emplace_or_replace<ECS::Transform>(obj.Handle(), ECS::Transform{});
                    m_SelectedEntity = obj.Handle();
                }
                catch (...)
                {
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("+ Cube"))
            {
                std::string base = (strlen(newNameBuf) > 0) ? std::string(newNameBuf) : std::string("Cube");
                std::string name = base;
                int         suffix = 1;
                while (m_World->HasObject(name))
                {
                    name = base + " (" + std::to_string(suffix++) + ")";
                }
                try
                {
                    auto obj = m_World->CreateObject(name);
                    auto& reg = m_World->Registry();
                    reg.emplace_or_replace<ECS::Transform>(obj.Handle(), ECS::Transform{});
                    reg.emplace_or_replace<ECS::StaticMesh>(obj.Handle(), ECS::StaticMesh{ECS::StaticMesh::MeshType::Cube});
                    m_SelectedEntity = obj.Handle();
                }
                catch (...)
                {
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("+ Dir Light"))
            {
                std::string base = (strlen(newNameBuf) > 0) ? std::string(newNameBuf) : std::string("Directional Light");
                std::string name = base;
                int         suffix = 1;
                while (m_World->HasObject(name))
                {
                    name = base + " (" + std::to_string(suffix++) + ")";
                }
                try
                {
                    auto obj = m_World->CreateObject(name);
                    auto& reg = m_World->Registry();
                    reg.emplace_or_replace<ECS::Transform>(obj.Handle(), ECS::Transform{});
                    ECS::DirectionalLight dl;
                    dl.direction = float3{-0.5f, -1.0f, 0.5f};
                    dl.intensity = 1.0f;
                    reg.emplace_or_replace<ECS::DirectionalLight>(obj.Handle(), dl);
                    m_SelectedEntity = obj.Handle();
                }
                catch (...)
                {
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("+ Point Light"))
            {
                std::string base = (strlen(newNameBuf) > 0) ? std::string(newNameBuf) : std::string("Point Light");
                std::string name = base;
                int         suffix = 1;
                while (m_World->HasObject(name))
                {
                    name = base + " (" + std::to_string(suffix++) + ")";
                }
                try
                {
                    auto obj = m_World->CreateObject(name);
                    auto& reg = m_World->Registry();
                    ECS::Transform tr;
                    tr.position = float3{0, 2, 0};
                    reg.emplace_or_replace<ECS::Transform>(obj.Handle(), tr);
                    ECS::PointLight pl;
                    pl.intensity = 2.0f;
                    pl.range     = 10.0f;
                    pl.color     = float3{1, 1, 1};
                    reg.emplace_or_replace<ECS::PointLight>(obj.Handle(), pl);
                    m_SelectedEntity = obj.Handle();
                }
                catch (...)
                {
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("+ Spot Light"))
            {
                std::string base = (strlen(newNameBuf) > 0) ? std::string(newNameBuf) : std::string("Spot Light");
                std::string name = base;
                int         suffix = 1;
                while (m_World->HasObject(name))
                {
                    name = base + " (" + std::to_string(suffix++) + ")";
                }
                try
                {
                    auto obj = m_World->CreateObject(name);
                    auto& reg = m_World->Registry();
                    ECS::Transform tr;
                    tr.position = float3{0, 3, 0};
                    reg.emplace_or_replace<ECS::Transform>(obj.Handle(), tr);
                    ECS::SpotLight sl;
                    sl.direction     = float3{0, -1, 0};
                    sl.intensity     = 3.0f;
                    sl.angleDegrees  = 30.0f;
                    sl.range         = 15.0f;
                    sl.color         = float3{1, 1, 1};
                    reg.emplace_or_replace<ECS::SpotLight>(obj.Handle(), sl);
                    m_SelectedEntity = obj.Handle();
                }
                catch (...)
                {
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("+ Plane"))
            {
                std::string base = (strlen(newNameBuf) > 0) ? std::string(newNameBuf) : std::string("Plane");
                std::string name = base;
                int         suffix = 1;
                while (m_World->HasObject(name))
                {
                    name = base + " (" + std::to_string(suffix++) + ")";
                }
                try
                {
                    auto obj = m_World->CreateObject(name);
                    auto& reg = m_World->Registry();
                    ECS::Transform tr;
                    tr.position = float3{0, -2.0f, 0};
                    tr.scale    = float3{5, 1, 5};
                    reg.emplace_or_replace<ECS::Transform>(obj.Handle(), tr);
                    reg.emplace_or_replace<ECS::StaticMesh>(obj.Handle(), ECS::StaticMesh{ECS::StaticMesh::MeshType::Plane});
                    m_SelectedEntity = obj.Handle();
                }
                catch (...)
                {
                }
            }

            auto& reg = m_World->Registry();
            auto  view = reg.view<ECS::Name>();

            for (auto e : view)
            {
                const auto& name = view.get<ECS::Name>(e).value;
                bool         selected = (m_SelectedEntity == e);
                if (ImGui::Selectable(name.c_str(), selected))
                {
                    m_SelectedEntity = e;
                }
                if (ImGui::BeginPopupContextItem())
                {
                    if (ImGui::MenuItem("Deselect"))
                        m_SelectedEntity = entt::null;
                    if (ImGui::MenuItem("Delete"))
                    {
                        if (m_SelectedEntity == e)
                            m_SelectedEntity = entt::null;
                        m_World->DestroyEntity(e);
                    }
                    ImGui::EndPopup();
                }
            }
        }
        else
        {
            ImGui::TextDisabled("No world");
        }
    }
    ImGui::End();

    ImGui::SetNextWindowSizeConstraints(ImVec2(360.0f, 320.0f), ImVec2(FLT_MAX, FLT_MAX));
    if (ImGui::Begin("Properties", nullptr, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::Text("Inspector");
        ImGui::Separator();
        EnsureWorld();
        if (!m_World)
        {
            ImGui::TextDisabled("No world");
        }
        else
        {
            auto& reg = m_World->Registry();
            if (m_SelectedEntity == entt::null || !reg.valid(m_SelectedEntity))
            {
                ImGui::TextDisabled("No selection");
            }
            else
            {
                if (reg.any_of<ECS::Name>(m_SelectedEntity))
                {
                    auto& name = reg.get<ECS::Name>(m_SelectedEntity);
                    char  buf[256];
                    strncpy(buf, name.value.c_str(), sizeof(buf));
                    buf[sizeof(buf) - 1] = '\0';
                    if (ImGui::InputText("Name", buf, sizeof(buf)))
                    {
                        if (!m_World->RenameEntity(m_SelectedEntity, std::string(buf)))
                        {
                            ImGui::SameLine();
                            ImGui::TextDisabled("(name taken)");
                        }
                    }
                }

                if (ImGui::Button("Delete Object"))
                {
                    m_World->DestroyEntity(m_SelectedEntity);
                    m_SelectedEntity = entt::null;
                    ImGui::End();
                    goto end_properties_window;
                }
                ImGui::SameLine();
                if (ImGui::Button("Add Component"))
                    ImGui::OpenPopup("AddComponentPopup");
                if (ImGui::BeginPopup("AddComponentPopup"))
                {
                    if (!reg.any_of<ECS::Transform>(m_SelectedEntity))
                    {
                        if (ImGui::MenuItem("Transform"))
                        {
                            reg.emplace<ECS::Transform>(m_SelectedEntity, ECS::Transform{});
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    if (!reg.any_of<ECS::StaticMesh>(m_SelectedEntity))
                    {
                        if (ImGui::BeginMenu("Static Mesh"))
                        {
                            if (ImGui::MenuItem("Cube"))
                            {
                                reg.emplace<ECS::StaticMesh>(m_SelectedEntity, ECS::StaticMesh{ECS::StaticMesh::MeshType::Cube});
                                if (!reg.any_of<ECS::Transform>(m_SelectedEntity))
                                    reg.emplace<ECS::Transform>(m_SelectedEntity, ECS::Transform{});
                                ImGui::CloseCurrentPopup();
                            }
                            if (ImGui::MenuItem("Plane"))
                            {
                                reg.emplace<ECS::StaticMesh>(m_SelectedEntity, ECS::StaticMesh{ECS::StaticMesh::MeshType::Plane});
                                if (!reg.any_of<ECS::Transform>(m_SelectedEntity))
                                    reg.emplace<ECS::Transform>(m_SelectedEntity, ECS::Transform{});
                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::EndMenu();
                        }
                    }
                    if (!reg.any_of<ECS::Mesh>(m_SelectedEntity))
                    {
                        if (ImGui::MenuItem("Mesh"))
                        {
                            reg.emplace<ECS::Mesh>(m_SelectedEntity, ECS::Mesh{});
                            if (!reg.any_of<ECS::Transform>(m_SelectedEntity))
                                reg.emplace<ECS::Transform>(m_SelectedEntity, ECS::Transform{});
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    if (!reg.any_of<ECS::DirectionalLight>(m_SelectedEntity))
                    {
                        if (ImGui::MenuItem("Directional Light"))
                        {
                            reg.emplace<ECS::DirectionalLight>(m_SelectedEntity, ECS::DirectionalLight{});
                            if (!reg.any_of<ECS::Transform>(m_SelectedEntity))
                                reg.emplace<ECS::Transform>(m_SelectedEntity, ECS::Transform{});
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    if (!reg.any_of<ECS::PointLight>(m_SelectedEntity))
                    {
                        if (ImGui::MenuItem("Point Light"))
                        {
                            reg.emplace<ECS::PointLight>(m_SelectedEntity, ECS::PointLight{});
                            if (!reg.any_of<ECS::Transform>(m_SelectedEntity))
                                reg.emplace<ECS::Transform>(m_SelectedEntity, ECS::Transform{});
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    if (!reg.any_of<ECS::SpotLight>(m_SelectedEntity))
                    {
                        if (ImGui::MenuItem("Spot Light"))
                        {
                            reg.emplace<ECS::SpotLight>(m_SelectedEntity, ECS::SpotLight{});
                            if (!reg.any_of<ECS::Transform>(m_SelectedEntity))
                                reg.emplace<ECS::Transform>(m_SelectedEntity, ECS::Transform{});
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    if (!reg.any_of<ECS::Camera>(m_SelectedEntity))
                    {
                        if (ImGui::MenuItem("Camera"))
                        {
                            reg.emplace<ECS::Camera>(m_SelectedEntity, ECS::Camera{});
                            if (!reg.any_of<ECS::Transform>(m_SelectedEntity))
                                reg.emplace<ECS::Transform>(m_SelectedEntity, ECS::Transform{});
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    if (!reg.any_of<ECS::Sky>(m_SelectedEntity))
                    {
                        if (ImGui::MenuItem("Sky"))
                        {
                            reg.emplace<ECS::Sky>(m_SelectedEntity, ECS::Sky{});
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    if (!reg.any_of<ECS::Fog>(m_SelectedEntity))
                    {
                        if (ImGui::MenuItem("Fog"))
                        {
                            reg.emplace<ECS::Fog>(m_SelectedEntity, ECS::Fog{});
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    if (!reg.any_of<ECS::SkyLight>(m_SelectedEntity))
                    {
                        if (ImGui::MenuItem("Sky Light"))
                        {
                            reg.emplace<ECS::SkyLight>(m_SelectedEntity, ECS::SkyLight{});
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    ImGui::EndPopup();
                }

                if (reg.any_of<ECS::Transform>(m_SelectedEntity))
                {
                    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        auto& tr = reg.get<ECS::Transform>(m_SelectedEntity);
                        float  pos[3] = {tr.position.x, tr.position.y, tr.position.z};
                        float  rotDeg[3] = {tr.rotationEuler.x * 180.0f / PI_F, tr.rotationEuler.y * 180.0f / PI_F, tr.rotationEuler.z * 180.0f / PI_F};
                        float  scl[3] = {tr.scale.x, tr.scale.y, tr.scale.z};

                        if (ImGui::DragFloat3("Position", pos, 0.1f))
                        {
                            tr.position = float3{pos[0], pos[1], pos[2]};
                        }
                        if (ImGui::DragFloat3("Rotation (deg)", rotDeg, 0.5f))
                        {
                            tr.rotationEuler = float3{rotDeg[0] * PI_F / 180.0f, rotDeg[1] * PI_F / 180.0f, rotDeg[2] * PI_F / 180.0f};
                        }
                        if (ImGui::DragFloat3("Scale", scl, 0.05f))
                        {
                            for (int i = 0; i < 3; ++i)
                                if (scl[i] == 0.0f)
                                    scl[i] = 0.0001f;
                            tr.scale = float3{scl[0], scl[1], scl[2]};
                        }
                    }
                }

                if (reg.any_of<ECS::StaticMesh>(m_SelectedEntity))
                {
                    if (ImGui::CollapsingHeader("Static Mesh", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        const auto& sm    = reg.get<ECS::StaticMesh>(m_SelectedEntity);
                        const char* meshType = (sm.type == ECS::StaticMesh::MeshType::Cube) ? "Cube" : (sm.type == ECS::StaticMesh::MeshType::Plane ? "Plane" : "Unknown");
                        ImGui::Text("Type: %s", meshType);
                    }
                }

                if (reg.any_of<ECS::DirectionalLight>(m_SelectedEntity))
                {
                    if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        auto& dl = reg.get<ECS::DirectionalLight>(m_SelectedEntity);
                        float dir[3] = {dl.direction.x, dl.direction.y, dl.direction.z};
                        if (ImGui::DragFloat3("Direction", dir, 0.01f))
                        {
                            float3 d = float3{dir[0], dir[1], dir[2]};
                            dl.direction = (length(d) > 0.0001f) ? normalize(d) : float3{0, -1, 0};
                        }
                        ImGui::DragFloat("Intensity", &dl.intensity, 0.01f, 0.0f, 100.0f);
                    }
                }
                if (reg.any_of<ECS::PointLight>(m_SelectedEntity))
                {
                    if (ImGui::CollapsingHeader("Point Light", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        auto& pl = reg.get<ECS::PointLight>(m_SelectedEntity);
                        float  col[3] = {pl.color.x, pl.color.y, pl.color.z};
                        if (ImGui::ColorEdit3("Color", col))
                        {
                            pl.color = float3{col[0], col[1], col[2]};
                        }
                        ImGui::DragFloat("Intensity", &pl.intensity, 0.01f, 0.0f, 100.0f);
                        ImGui::DragFloat("Range", &pl.range, 0.1f, 0.0f, 1000.0f);
                    }
                }
                if (reg.any_of<ECS::SpotLight>(m_SelectedEntity))
                {
                    if (ImGui::CollapsingHeader("Spot Light", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        auto& sl = reg.get<ECS::SpotLight>(m_SelectedEntity);
                        float  dir[3] = {sl.direction.x, sl.direction.y, sl.direction.z};
                        if (ImGui::DragFloat3("Direction", dir, 0.01f))
                        {
                            float3 d = float3{dir[0], dir[1], dir[2]};
                            sl.direction = (length(d) > 0.0001f) ? normalize(d) : float3{0, -1, 0};
                        }
                        float col[3] = {sl.color.x, sl.color.y, sl.color.z};
                        if (ImGui::ColorEdit3("Color", col))
                        {
                            sl.color = float3{col[0], col[1], col[2]};
                        }
                        ImGui::DragFloat("Intensity", &sl.intensity, 0.01f, 0.0f, 100.0f);
                        ImGui::SliderFloat("Angle (deg)", &sl.angleDegrees, 1.0f, 90.0f);
                        ImGui::DragFloat("Range", &sl.range, 0.1f, 0.0f, 1000.0f);
                    }
                }

                if (reg.any_of<ECS::Camera>(m_SelectedEntity))
                {
                    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        auto& cam = reg.get<ECS::Camera>(m_SelectedEntity);
                        float  fovDeg = cam.fovYRadians * 180.0f / PI_F;
                        if (ImGui::SliderFloat("FOV (deg)", &fovDeg, 10.0f, 150.0f))
                        {
                            cam.fovYRadians = fovDeg * PI_F / 180.0f;
                        }
                        float  nearMax = std::max(cam.nearZ + 0.001f, cam.farZ - 0.01f);
                        nearMax        = std::max(nearMax, 0.01f);
                        if (ImGui::DragFloat("Near Clip", &cam.nearZ, 0.01f, 0.001f, nearMax))
                        {
                            cam.nearZ = std::clamp(cam.nearZ, 0.001f, cam.farZ - 0.01f);
                        }
                        float farMin = cam.nearZ + 0.01f;
                        if (ImGui::DragFloat("Far Clip", &cam.farZ, 1.0f, farMin, 20000.0f))
                        {
                            cam.farZ = std::max(farMin, cam.farZ);
                        }
                    }
                }

                if (reg.any_of<ECS::Sky>(m_SelectedEntity))
                {
                    if (ImGui::CollapsingHeader("Sky", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        auto& sky  = reg.get<ECS::Sky>(m_SelectedEntity);
                        bool   enabled = sky.enabled;
                        if (ImGui::Checkbox("Enabled##Sky", &enabled))
                            sky.enabled = enabled;

                        char envBuf[512];
                        std::strncpy(envBuf, sky.environmentPath.c_str(), sizeof(envBuf));
                        envBuf[sizeof(envBuf) - 1] = '\0';
                        if (ImGui::InputText("Environment Map", envBuf, sizeof(envBuf)))
                            sky.environmentPath = envBuf;

                        ImGui::SameLine();
                        if (ImGui::Button("Browse##SkyEnv"))
                        {
                            std::string picked;
                            if (Platform::OpenFileDialogEnvironmentMap(picked))
                            {
                                std::string finalPath = picked;
                                try
                                {
                                    namespace fs = std::filesystem;
                                    fs::path absPath = fs::absolute(fs::path{picked});
                                    finalPath        = absPath.u8string();
                                    if (m_Project)
                                    {
                                        const auto& cfg = m_Project->GetConfig();
                                        if (!cfg.rootDir.empty())
                                        {
                                            std::error_code ec;
                                            fs::path rootAbs = fs::absolute(cfg.rootDir);
                                            fs::path rel     = fs::relative(absPath, rootAbs, ec);
                                            if (!ec)
                                                finalPath = rel.generic_string();
                                        }
                                    }
                                }
                                catch (...)
                                {
                                    // fall back to picked path on failure
                                }
                                sky.environmentPath = finalPath;
                                if (!sky.environmentPath.empty())
                                    LoadEnvironmentMap(sky.environmentPath);
                            }
                        }

                        ImGui::SameLine();
                        if (ImGui::Button("Clear Path"))
                        {
                            sky.environmentPath.clear();
                            LoadEnvironmentMap("");
                        }

                        ImGui::DragFloat("Intensity", &sky.intensity, 0.01f, 0.0f, 50.0f);
                        ImGui::DragFloat("Exposure", &sky.exposure, 0.01f, 0.0f, 10.0f);
                        ImGui::SliderFloat("Rotation (deg)", &sky.rotationDegrees, -180.0f, 180.0f);

                        bool showBackground = sky.showBackground;
                        if (ImGui::Checkbox("Show Background", &showBackground))
                            sky.showBackground = showBackground;
                    }
                }

                if (reg.any_of<ECS::SkyLight>(m_SelectedEntity))
                {
                    if (ImGui::CollapsingHeader("Sky Light", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        auto& skyLight = reg.get<ECS::SkyLight>(m_SelectedEntity);
                        bool  enabled  = skyLight.enabled;
                        if (ImGui::Checkbox("Enabled##SkyLight", &enabled))
                            skyLight.enabled = enabled;

                        float color[3] = {skyLight.color.x, skyLight.color.y, skyLight.color.z};
                        if (ImGui::ColorEdit3("Color", color))
                            skyLight.color = float3{color[0], color[1], color[2]};

                        ImGui::DragFloat("Intensity", &skyLight.intensity, 0.01f, 0.0f, 50.0f);
                    }
                }

                if (reg.any_of<ECS::Fog>(m_SelectedEntity))
                {
                    if (ImGui::CollapsingHeader("Fog", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        auto& fog = reg.get<ECS::Fog>(m_SelectedEntity);
                        bool  enabled = fog.enabled;
                        if (ImGui::Checkbox("Enabled##Fog", &enabled))
                            fog.enabled = enabled;

                        float color[3] = {fog.color.x, fog.color.y, fog.color.z};
                        if (ImGui::ColorEdit3("Color", color))
                            fog.color = float3{color[0], color[1], color[2]};

                        ImGui::DragFloat("Density", &fog.density, 0.001f, 0.0f, 1.0f, "%.4f");
                        float maxStart = std::max(0.0f, fog.maxDistance - 1.0f);
                        if (ImGui::DragFloat("Start Distance", &fog.startDistance, 0.5f, 0.0f, maxStart))
                        {
                            fog.startDistance = std::clamp(fog.startDistance, 0.0f, fog.maxDistance - 1.0f);
                        }
                        float minMax = fog.startDistance + 1.0f;
                        if (ImGui::DragFloat("Max Distance", &fog.maxDistance, 0.5f, minMax, 10000.0f))
                        {
                            fog.maxDistance = std::max(minMax, fog.maxDistance);
                        }
                        ImGui::DragFloat("Height Falloff", &fog.heightFalloff, 0.001f, 0.0f, 1.0f, "%.4f");
                    }
                }

                if (reg.any_of<ECS::Mesh>(m_SelectedEntity))
                {
                    auto& mesh = reg.get<ECS::Mesh>(m_SelectedEntity);
                    if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        int kind = mesh.kind == ECS::Mesh::Kind::Static ? 0 : 1;
                        if (ImGui::Combo("Type", &kind, "Static\0Dynamic (glTF)\0"))
                        {
                            mesh.kind = (kind == 0) ? ECS::Mesh::Kind::Static : ECS::Mesh::Kind::Dynamic;
                        }
                        if (mesh.kind == ECS::Mesh::Kind::Static)
                        {
                            int staticType = (mesh.staticType == ECS::Mesh::StaticType::Cube) ? 0 : 1;
                            if (ImGui::Combo("Static Mesh", &staticType, "Cube\0Plane\0"))
                            {
                                mesh.staticType = (staticType == 0) ? ECS::Mesh::StaticType::Cube : ECS::Mesh::StaticType::Plane;
                            }
                        }
                        else
                        {
                            char pathBuf[512];
                            strncpy(pathBuf, mesh.assetId.c_str(), sizeof(pathBuf));
                            pathBuf[sizeof(pathBuf) - 1] = '\0';
                            ImGui::InputText("Asset Id / Path", pathBuf, sizeof(pathBuf));
                            if (ImGui::Button("Load from Path"))
                            {
                                mesh.assetId = pathBuf;
                                LoadGLTFAsset(mesh.assetId);
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("Browse..."))
                            {
                                std::string picked;
                                if (Platform::OpenFileDialogGLTF(picked))
                                {
                                    mesh.assetId = picked;
                                    LoadGLTFAsset(mesh.assetId);
                                }
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("Engine Models"))
                            {
                                ImGui::OpenPopup("EngineModelPopup");
                            }
                            if (ImGui::BeginPopup("EngineModelPopup"))
                            {
                                struct BuiltinEntry
                                {
                                    const char* Label;
                                    const char* RelPath;
                                };
                                static BuiltinEntry entries[] = {
                                    {"DamagedHelmet", "external/gltfassets/Models/DamagedHelmet/glTF/DamagedHelmet.gltf"},
                                    {"Duck", "external/gltfassets/Models/Duck/glTF/Duck.gltf"},
                                    {"Sponza", "external/gltfassets/Models/Sponza/glTF/Sponza.gltf"},
                                    {"Suzanne", "external/gltfassets/Models/Suzanne/glTF/Suzanne.gltf"},
                                    {"BoomBox", "external/gltfassets/Models/BoomBox/glTF/BoomBox.gltf"},
                                };
                                for (auto& e : entries)
                                {
                                    if (ImGui::Selectable(e.Label))
                                    {
                                        mesh.assetId = e.RelPath;
                                        LoadGLTFAsset(mesh.assetId);
                                        ImGui::CloseCurrentPopup();
                                    }
                                }
                                ImGui::EndPopup();
                            }
                        }
                    }
                }
            }
        }
    }
    ImGui::End();

    ImGui::SetNextWindowSizeConstraints(ImVec2(360.0f, 220.0f), ImVec2(FLT_MAX, FLT_MAX));
    if (g_ProjectConsoleDockId != 0 && g_ConsoleDockNeedsDock)
        ImGui::SetNextWindowDockID(g_ProjectConsoleDockId, ImGuiCond_Always);
    if (ImGui::Begin("Console", nullptr, ImGuiWindowFlags_NoCollapse))
    {
        if (g_ProjectConsoleDockId != 0 && g_ConsoleDockNeedsDock && ImGui::GetWindowDockID() == g_ProjectConsoleDockId)
            g_ConsoleDockNeedsDock = false;
        ImGui::Text("Console Output");
    }
    ImGui::End();

end_properties_window:
    return;
}

} // namespace Diligent
