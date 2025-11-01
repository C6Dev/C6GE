#include "Project/Project.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <cctype>

#include "Runtime/ECS/World.h"
#include "Runtime/ECS/Components.h"

using namespace std;

namespace fs = std::filesystem;

namespace Diligent { namespace ProjectSystem {

static string ReadAllText(const fs::path& p)
{
    ifstream f(p, ios::in | ios::binary);
    if (!f)
        return {};
    stringstream ss; ss << f.rdbuf();
    return ss.str();
}

static bool WriteAllText(const fs::path& p, const string& s)
{
    fs::create_directories(p.parent_path());
    ofstream f(p, ios::out | ios::binary | ios::trunc);
    if (!f)
        return false;
    f << s;
    return true;
}

static string JsonEscape(const string& in)
{
    string out; out.reserve(in.size()+8);
    for (char c : in) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}

bool ProjectManager::CreateDefault(const fs::path& projFile, const string& name)
{
    m_Config.projectFile = fs::absolute(projFile);
    m_Config.rootDir     = m_Config.projectFile.parent_path();
    m_Config.projectName = name;
    m_Config.engineVersion = "2026.1";
    m_Config.assetsDir   = m_Config.rootDir / "Assets";
    m_Config.worldsDir   = m_Config.rootDir / "Worlds";
    m_Config.modelsDir   = m_Config.assetsDir / "Models";
    m_Config.startupWorld = m_Config.worldsDir / "Default.world";

    fs::create_directories(m_Config.assetsDir);
    fs::create_directories(m_Config.worldsDir);
    fs::create_directories(m_Config.modelsDir);

    // Write the project file JSON
    ostringstream o;
    o << "{\n";
    o << "  \"engineVersion\": \"" << JsonEscape(m_Config.engineVersion) << "\",\n";
    o << "  \"projectName\": \"" << JsonEscape(m_Config.projectName) << "\",\n";
    // Paths as relative to root
    o << "  \"paths\": {\n";
    o << "    \"assets\": \"Assets\",\n";
    o << "    \"worlds\": \"Worlds\",\n";
    o << "    \"models\": \"Assets/Models\"\n";
    o << "  },\n";
    o << "  \"startupWorld\": \"Worlds/Default.world\"\n";
    o << "}\n";
    return WriteAllText(m_Config.projectFile, o.str());
}

bool ProjectManager::Load(const fs::path& projFile)
{
    auto abs = fs::absolute(projFile);
    string txt = ReadAllText(abs);
    if (txt.empty())
        return false;

    // extremely small JSON field extraction (expects double quotes and simple structure)
    auto find_str = [&](const char* key)->string{
        // Build a regex that looks like: "key"\s*:\s*"value"
        std::string pattern = std::string("\"") + key + "\"\\s*:\\s*\"([^\\\"]*)\"";
        std::regex rg(pattern);
        std::smatch m;
        if (std::regex_search(txt, m, rg) && m.size() > 1)
            return m[1].str();
        return {};
    };
    string engine = find_str("engineVersion");
    string name   = find_str("projectName");
    string assets = find_str("assets");
    string worlds = find_str("worlds");
    string models = find_str("models");
    string startup = find_str("startupWorld");

    m_Config.projectFile = abs;
    m_Config.rootDir = abs.parent_path();
    m_Config.engineVersion = engine.empty() ? string("2026.1") : engine;
    m_Config.projectName   = name.empty() ? string("Untitled") : name;
    m_Config.assetsDir = m_Config.rootDir / (assets.empty() ? fs::path("Assets") : fs::path(assets));
    m_Config.worldsDir = m_Config.rootDir / (worlds.empty() ? fs::path("Worlds") : fs::path(worlds));
    m_Config.modelsDir = m_Config.rootDir / (models.empty() ? fs::path("Assets/Models") : fs::path(models));
    if (!startup.empty())
        m_Config.startupWorld = m_Config.rootDir / startup;
    else
        m_Config.startupWorld = m_Config.worldsDir / "Default.world";

    // Ensure dirs exist
    fs::create_directories(m_Config.assetsDir);
    fs::create_directories(m_Config.worldsDir);
    fs::create_directories(m_Config.modelsDir);
    return true;
}

bool ProjectManager::Save()
{
    if (m_Config.projectFile.empty()) return false;
    // write relative paths
    auto rel = [&](const fs::path& p){ return fs::relative(p, m_Config.rootDir).generic_string(); };
    ostringstream o;
    o << "{\n";
    o << "  \"engineVersion\": \"" << JsonEscape(m_Config.engineVersion) << "\",\n";
    o << "  \"projectName\": \"" << JsonEscape(m_Config.projectName) << "\",\n";
    o << "  \"paths\": {\n";
    o << "    \"assets\": \"" << JsonEscape(rel(m_Config.assetsDir)) << "\",\n";
    o << "    \"worlds\": \"" << JsonEscape(rel(m_Config.worldsDir)) << "\",\n";
    o << "    \"models\": \"" << JsonEscape(rel(m_Config.modelsDir)) << "\"\n";
    o << "  },\n";
    o << "  \"startupWorld\": \"" << JsonEscape(rel(m_Config.startupWorld)) << "\"\n";
    o << "}\n";
    return WriteAllText(m_Config.projectFile, o.str());
}

vector<fs::path> ProjectManager::ListWorldFiles() const
{
    vector<fs::path> out;
    if (!m_Config.worldsDir.empty() && fs::exists(m_Config.worldsDir))
        for (auto& e : fs::recursive_directory_iterator(m_Config.worldsDir))
            if (e.is_regular_file() && e.path().extension() == ".world")
                out.push_back(e.path());
    return out;
}

vector<fs::path> ProjectManager::ListModelFiles() const
{
    vector<fs::path> out;
    if (!m_Config.modelsDir.empty() && fs::exists(m_Config.modelsDir))
        for (auto& e : fs::recursive_directory_iterator(m_Config.modelsDir))
            if (e.is_regular_file() && e.path().extension() == ".c6m")
                out.push_back(e.path());
    return out;
}

vector<fs::path> ProjectManager::ListGLTFFiles() const
{
    vector<fs::path> out;
    if (!m_Config.assetsDir.empty() && fs::exists(m_Config.assetsDir))
        for (auto& e : fs::recursive_directory_iterator(m_Config.assetsDir))
            if (e.is_regular_file()) {
                auto ext = e.path().extension().string();
                for (auto& c : ext) c = (char)tolower(c);
                if (ext == ".gltf" || ext == ".glb")
                    out.push_back(e.path());
            }
    return out;
}

fs::path ProjectManager::ConvertGLTFToC6M(const fs::path& gltfPath) const
{
    if (gltfPath.empty()) return {};
    fs::path fileName = gltfPath.filename();
    fs::path outName = fileName;
    outName.replace_extension(".c6m");
    fs::path outPath = m_Config.modelsDir / outName;

    // Write tiny JSON wrapper
    ostringstream o;
    o << "{\n";
    o << "  \"type\": \"c6m\",\n";
    o << "  \"version\": 1,\n";
    // store project-relative path if under project root, else absolute
    fs::path canonical = fs::absolute(gltfPath);
    string sourceStr;
    try {
        sourceStr = fs::relative(canonical, m_Config.rootDir).generic_string();
    } catch (...) {
        sourceStr = canonical.generic_string();
    }
    o << "  \"source\": \"" << JsonEscape(sourceStr) << "\"\n";
    o << "}\n";
    if (!WriteAllText(outPath, o.str()))
        return {};
    return outPath;
}

fs::path ProjectManager::FindNearestProject(const fs::path& startDir)
{
    fs::path cur = fs::absolute(startDir);
    for (int i=0; i<20; ++i) {
        if (fs::exists(cur) && fs::is_directory(cur))
        {
            for (auto& e : fs::directory_iterator(cur))
                if (e.is_regular_file() && e.path().extension() == ".c6proj")
                    return e.path();
        }
        auto parent = cur.parent_path();
        if (parent == cur) break;
        cur = parent;
    }
    return {};
}

// ---------------- World IO -----------------

namespace {
    // Simple helpers that assume the JSON we wrote in Save(); this is not a general-purpose parser.
    static bool TryFindNext(const string& s, size_t& pos, const string& token)
    {
        size_t p = s.find(token, pos);
        if (p == string::npos) return false;
        pos = p + token.size();
        return true;
    }
    static bool TryReadQuotedString(const string& s, size_t& pos, string& out)
    {
        size_t q1 = s.find('"', pos);
        if (q1 == string::npos) return false;
        size_t q2 = s.find('"', q1+1);
        if (q2 == string::npos) return false;
        out = s.substr(q1+1, q2-(q1+1));
        pos = q2+1;
        return true;
    }
    static bool TryReadFloatArray3(const string& s, size_t& pos, float out[3])
    {
        size_t l = s.find('[', pos); if (l == string::npos) return false; pos = l+1;
        for (int i=0;i<3;++i) {
            size_t start = pos;
            // skip spaces
            while (start < s.size() && isspace((unsigned char)s[start])) ++start;
            size_t end = start;
            while (end < s.size() && (isdigit((unsigned char)s[end]) || s[end]=='-' || s[end]=='+' || s[end]=='.' || s[end]=='e' || s[end]=='E')) ++end;
            if (end==start) return false;
            out[i] = static_cast<float>(atof(s.substr(start, end-start).c_str()));
            pos = end;
            if (i<2) { size_t comma = s.find(',', pos); if (comma==string::npos) return false; pos = comma+1; }
        }
        size_t r = s.find(']', pos); if (r == string::npos) return false; pos = r+1;
        return true;
    }
    static bool TryReadFloat(const string& s, size_t& pos, float& out)
    {
        size_t start = pos;
        while (start < s.size() && isspace(static_cast<unsigned char>(s[start])))
            ++start;
        size_t end = start;
        while (end < s.size() && (isdigit(static_cast<unsigned char>(s[end])) || s[end]=='-' || s[end]=='+' || s[end]=='.' || s[end]=='e' || s[end]=='E'))
            ++end;
        if (end == start)
            return false;
        out = static_cast<float>(atof(s.substr(start, end - start).c_str()));
        pos = end;
        return true;
    }
    static bool TryReadBool(const string& s, size_t& pos, bool& out)
    {
        size_t start = pos;
        while (start < s.size() && isspace(static_cast<unsigned char>(s[start])))
            ++start;
        if (start >= s.size())
            return false;
        if (s.compare(start, 4, "true") == 0)
        {
            out = true;
            pos = start + 4;
            return true;
        }
        if (s.compare(start, 5, "false") == 0)
        {
            out = false;
            pos = start + 5;
            return true;
        }
        return false;
    }
}

// Concrete adapter around ECS::World to satisfy ECSWorldLike
class ECSWorldAdapter : public ECSWorldLike {
public:
    explicit ECSWorldAdapter(ECS::World& w) : W(w) {}
    void Clear() override {
        // brute force: destroy all named entities (project objects have Name)
        auto& reg = W.Registry();
        vector<entt::entity> to_destroy;
        auto view = reg.view<ECS::Name>();
        for (auto e : view) to_destroy.push_back(e);
        for (auto e : to_destroy) W.DestroyEntity(e);
    }
    void* CreateObject(const string& name) override {
        auto obj = W.CreateObject(name);
        return reinterpret_cast<void*>(static_cast<uintptr_t>(obj.Handle()));
    }
    void SetTransform(void* handle, const TransformData& t) override {
        auto e = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(handle));
        auto& reg = W.Registry();
        ECS::Transform tr;
        tr.position = float3{t.position[0], t.position[1], t.position[2]};
        tr.rotationEuler = float3{t.rotationEuler[0], t.rotationEuler[1], t.rotationEuler[2]};
        tr.scale    = float3{t.scale[0], t.scale[1], t.scale[2]};
        reg.emplace_or_replace<ECS::Transform>(e, tr);
    }
    void SetMesh(void* handle, MeshKind kind, const string& assetId) override {
        auto e = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(handle));
        auto& reg = W.Registry();
        if (kind == MeshKind::StaticCube) {
            reg.emplace_or_replace<ECS::StaticMesh>(e, ECS::StaticMesh{ECS::StaticMesh::MeshType::Cube});
        } else if (kind == MeshKind::DynamicGLTF) {
            ECS::Mesh m; m.kind = ECS::Mesh::Kind::Dynamic; m.staticType = ECS::Mesh::StaticType::Cube; m.assetId = assetId;
            reg.emplace_or_replace<ECS::Mesh>(e, m);
        }
    }
    void SetDirectionalLight(void* handle, const DirectionalLightData& data) override {
        auto e = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(handle));
        auto& reg = W.Registry();
        ECS::DirectionalLight l;
        l.direction = float3{data.direction[0], data.direction[1], data.direction[2]};
        l.intensity = data.intensity;
        reg.emplace_or_replace<ECS::DirectionalLight>(e, l);
    }
    void SetPointLight(void* handle, const PointLightData& data) override {
        auto e = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(handle));
        auto& reg = W.Registry();
        ECS::PointLight l;
        l.color     = float3{data.color[0], data.color[1], data.color[2]};
        l.intensity = data.intensity;
        l.range     = data.range;
        reg.emplace_or_replace<ECS::PointLight>(e, l);
    }
    void SetSpotLight(void* handle, const SpotLightData& data) override {
        auto e = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(handle));
        auto& reg = W.Registry();
        ECS::SpotLight l;
        l.direction    = float3{data.direction[0], data.direction[1], data.direction[2]};
        l.angleDegrees = data.angleDegrees;
        l.color        = float3{data.color[0], data.color[1], data.color[2]};
        l.intensity    = data.intensity;
        l.range        = data.range;
        reg.emplace_or_replace<ECS::SpotLight>(e, l);
    }
    void SetCamera(void* handle, const CameraData& data) override {
        auto e = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(handle));
        auto& reg = W.Registry();
        ECS::Camera cam;
        cam.fovYRadians = data.fovYRadians;
        cam.nearZ       = data.nearZ;
        cam.farZ        = data.farZ;
        reg.emplace_or_replace<ECS::Camera>(e, cam);
    }
    void SetSky(void* handle, const SkyData& data) override {
        auto e = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(handle));
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
    void SetFog(void* handle, const FogData& data) override {
        auto e = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(handle));
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
    void SetSkyLight(void* handle, const SkyLightData& data) override {
        auto e = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(handle));
        auto& reg = W.Registry();
        ECS::SkyLight skyLight;
        skyLight.enabled   = data.enabled;
        skyLight.color     = float3{data.color[0], data.color[1], data.color[2]};
        skyLight.intensity = data.intensity;
        reg.emplace_or_replace<ECS::SkyLight>(e, skyLight);
    }
    vector<ObjectViewItem> EnumerateObjects() const override {
        vector<ObjectViewItem> out;
        auto& reg = W.Registry();
        auto view = reg.view<ECS::Name>();
        for (auto e : view) {
            ObjectViewItem it; it.handle = reinterpret_cast<void*>(static_cast<uintptr_t>(e));
            it.name = view.get<ECS::Name>(e).value;
            it.hasTransform = reg.any_of<ECS::Transform>(e);
            if (it.hasTransform) {
                const auto& tr = reg.get<ECS::Transform>(e);
                it.tr.position[0]=tr.position.x; it.tr.position[1]=tr.position.y; it.tr.position[2]=tr.position.z;
                it.tr.rotationEuler[0]=tr.rotationEuler.x; it.tr.rotationEuler[1]=tr.rotationEuler.y; it.tr.rotationEuler[2]=tr.rotationEuler.z;
                it.tr.scale[0]=tr.scale.x; it.tr.scale[1]=tr.scale.y; it.tr.scale[2]=tr.scale.z;
            }
            it.meshKind = MeshKind::None;
            if (reg.any_of<ECS::StaticMesh>(e)) {
                it.meshKind = MeshKind::StaticCube;
            }
            if (reg.any_of<ECS::Mesh>(e)) {
                const auto& m = reg.get<ECS::Mesh>(e);
                if (m.kind == ECS::Mesh::Kind::Static && m.staticType == ECS::Mesh::StaticType::Cube) {
                    it.meshKind = MeshKind::StaticCube;
                } else if (m.kind == ECS::Mesh::Kind::Dynamic) {
                    it.meshKind = MeshKind::DynamicGLTF; it.assetId = m.assetId;
                }
            }
            it.hasDirectionalLight = reg.any_of<ECS::DirectionalLight>(e);
            if (it.hasDirectionalLight) {
                const auto& l = reg.get<ECS::DirectionalLight>(e);
                it.directionalLight.direction[0] = l.direction.x;
                it.directionalLight.direction[1] = l.direction.y;
                it.directionalLight.direction[2] = l.direction.z;
                it.directionalLight.intensity    = l.intensity;
            }
            it.hasPointLight = reg.any_of<ECS::PointLight>(e);
            if (it.hasPointLight) {
                const auto& l = reg.get<ECS::PointLight>(e);
                it.pointLight.color[0] = l.color.x;
                it.pointLight.color[1] = l.color.y;
                it.pointLight.color[2] = l.color.z;
                it.pointLight.intensity = l.intensity;
                it.pointLight.range     = l.range;
            }
            it.hasSpotLight = reg.any_of<ECS::SpotLight>(e);
            if (it.hasSpotLight) {
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
            if (it.hasCamera) {
                const auto& cam = reg.get<ECS::Camera>(e);
                it.camera.fovYRadians = cam.fovYRadians;
                it.camera.nearZ       = cam.nearZ;
                it.camera.farZ        = cam.farZ;
            }
            it.hasSky = reg.any_of<ECS::Sky>(e);
            if (it.hasSky) {
                const auto& sky = reg.get<ECS::Sky>(e);
                it.sky.enabled          = sky.enabled;
                it.sky.environmentPath  = sky.environmentPath;
                it.sky.intensity        = sky.intensity;
                it.sky.exposure         = sky.exposure;
                it.sky.rotationDegrees  = sky.rotationDegrees;
                it.sky.showBackground   = sky.showBackground;
            }
            it.hasFog = reg.any_of<ECS::Fog>(e);
            if (it.hasFog) {
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
            if (it.hasSkyLight) {
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
private:
    ECS::World& W;
};

bool WorldIO::Save(const fs::path& path, ECSWorldLike& world)
{
    auto items = world.EnumerateObjects();
    ostringstream o;
    o << "{\n  \"world\": {\n    \"objects\": [\n";
    for (size_t i=0;i<items.size();++i) {
        const auto& it = items[i];
        o << "      {\n";
        std::vector<std::string> props;
        {
            std::ostringstream nameProp;
            nameProp << "        \"name\": \"" << JsonEscape(it.name) << "\"";
            props.push_back(nameProp.str());
        }
        if (it.hasTransform) {
            std::ostringstream trProp;
            trProp << "        \"transform\": {\n";
            trProp << "          \"position\": [" << it.tr.position[0] << ", " << it.tr.position[1] << ", " << it.tr.position[2] << "],\n";
            trProp << "          \"rotationEuler\": [" << it.tr.rotationEuler[0] << ", " << it.tr.rotationEuler[1] << ", " << it.tr.rotationEuler[2] << "],\n";
            trProp << "          \"scale\": [" << it.tr.scale[0] << ", " << it.tr.scale[1] << ", " << it.tr.scale[2] << "]\n";
            trProp << "        }";
            props.push_back(trProp.str());
        }
        {
            std::ostringstream meshProp;
            meshProp << "        \"mesh\": {";
            if (it.meshKind == ECSWorldLike::MeshKind::StaticCube) {
                meshProp << "\"kind\": \"Static\", \"staticType\": \"Cube\"";
            } else if (it.meshKind == ECSWorldLike::MeshKind::DynamicGLTF) {
                meshProp << "\"kind\": \"Dynamic\", \"assetId\": \"" << JsonEscape(it.assetId) << "\"";
            } else {
                meshProp << "\"kind\": \"None\"";
            }
            meshProp << "}";
            props.push_back(meshProp.str());
        }
        if (it.hasDirectionalLight) {
            std::ostringstream dlProp;
            dlProp << "        \"directionalLight\": {\n";
            dlProp << "          \"direction\": [" << it.directionalLight.direction[0] << ", " << it.directionalLight.direction[1] << ", " << it.directionalLight.direction[2] << "],\n";
            dlProp << "          \"intensity\": " << it.directionalLight.intensity << "\n";
            dlProp << "        }";
            props.push_back(dlProp.str());
        }
        if (it.hasPointLight) {
            std::ostringstream plProp;
            plProp << "        \"pointLight\": {\n";
            plProp << "          \"color\": [" << it.pointLight.color[0] << ", " << it.pointLight.color[1] << ", " << it.pointLight.color[2] << "],\n";
            plProp << "          \"intensity\": " << it.pointLight.intensity << ",\n";
            plProp << "          \"range\": " << it.pointLight.range << "\n";
            plProp << "        }";
            props.push_back(plProp.str());
        }
        if (it.hasSpotLight) {
            std::ostringstream slProp;
            slProp << "        \"spotLight\": {\n";
            slProp << "          \"direction\": [" << it.spotLight.direction[0] << ", " << it.spotLight.direction[1] << ", " << it.spotLight.direction[2] << "],\n";
            slProp << "          \"color\": [" << it.spotLight.color[0] << ", " << it.spotLight.color[1] << ", " << it.spotLight.color[2] << "],\n";
            slProp << "          \"intensity\": " << it.spotLight.intensity << ",\n";
            slProp << "          \"range\": " << it.spotLight.range << ",\n";
            slProp << "          \"angleDegrees\": " << it.spotLight.angleDegrees << "\n";
            slProp << "        }";
            props.push_back(slProp.str());
        }
        if (it.hasCamera) {
            std::ostringstream camProp;
            camProp << "        \"camera\": {\n";
            camProp << "          \"fovYRadians\": " << it.camera.fovYRadians << ",\n";
            camProp << "          \"nearZ\": " << it.camera.nearZ << ",\n";
            camProp << "          \"farZ\": " << it.camera.farZ << "\n";
            camProp << "        }";
            props.push_back(camProp.str());
        }
        if (it.hasSky) {
            std::ostringstream skyProp;
            skyProp << "        \"sky\": {\n";
            skyProp << "          \"enabled\": " << (it.sky.enabled ? "true" : "false") << ",\n";
            skyProp << "          \"environmentPath\": \"" << JsonEscape(it.sky.environmentPath) << "\",\n";
            skyProp << "          \"intensity\": " << it.sky.intensity << ",\n";
            skyProp << "          \"exposure\": " << it.sky.exposure << ",\n";
            skyProp << "          \"rotationDegrees\": " << it.sky.rotationDegrees << ",\n";
            skyProp << "          \"showBackground\": " << (it.sky.showBackground ? "true" : "false") << "\n";
            skyProp << "        }";
            props.push_back(skyProp.str());
        }
        if (it.hasFog) {
            std::ostringstream fogProp;
            fogProp << "        \"fog\": {\n";
            fogProp << "          \"enabled\": " << (it.fog.enabled ? "true" : "false") << ",\n";
            fogProp << "          \"color\": [" << it.fog.color[0] << ", " << it.fog.color[1] << ", " << it.fog.color[2] << "],\n";
            fogProp << "          \"density\": " << it.fog.density << ",\n";
            fogProp << "          \"startDistance\": " << it.fog.startDistance << ",\n";
            fogProp << "          \"maxDistance\": " << it.fog.maxDistance << ",\n";
            fogProp << "          \"heightFalloff\": " << it.fog.heightFalloff << "\n";
            fogProp << "        }";
            props.push_back(fogProp.str());
        }
        if (it.hasSkyLight) {
            std::ostringstream skyLightProp;
            skyLightProp << "        \"skyLight\": {\n";
            skyLightProp << "          \"enabled\": " << (it.skyLight.enabled ? "true" : "false") << ",\n";
            skyLightProp << "          \"color\": [" << it.skyLight.color[0] << ", " << it.skyLight.color[1] << ", " << it.skyLight.color[2] << "],\n";
            skyLightProp << "          \"intensity\": " << it.skyLight.intensity << "\n";
            skyLightProp << "        }";
            props.push_back(skyLightProp.str());
        }

        for (size_t p = 0; p < props.size(); ++p) {
            o << props[p];
            if (p + 1 < props.size())
                o << ",\n";
            else
                o << "\n";
        }
        o << "      }" << (i+1<items.size()? ",":"") << "\n";
    }
    o << "    ]\n  }\n}\n";
    return WriteAllText(path, o.str());
}

bool WorldIO::Load(const fs::path& path, ECSWorldLike& world)
{
    string s = ReadAllText(path);
    if (s.empty()) return false;
    world.Clear();

    size_t pos = 0;
    if (!TryFindNext(s, pos, "\"objects\"")) return false;
    if (!TryFindNext(s, pos, "[")) return false;

    while (true) {
        // find next object or end
        size_t nextBrace = s.find('{', pos);
        size_t endList = s.find(']', pos);
        if (endList != string::npos && (nextBrace == string::npos || endList < nextBrace))
            break; // end of array
        if (nextBrace == string::npos) break;
        size_t objStart = nextBrace + 1;
        size_t scanPos = objStart;
        int braceDepth = 1;
        while (scanPos < s.size() && braceDepth > 0)
        {
            char ch = s[scanPos];
            if (ch == '{')
                ++braceDepth;
            else if (ch == '}')
                --braceDepth;
            ++scanPos;
        }
        if (braceDepth != 0)
            break;

        size_t objEnd = scanPos - 1; // position of closing '}'
        string objStr = s.substr(objStart, objEnd - objStart);
        pos = scanPos;

        // parse one object
        string name;
        ECSWorldLike::TransformData t{}; bool hasTr=false;
    ECSWorldLike::MeshKind mk = ECSWorldLike::MeshKind::None; string assetId;
    ECSWorldLike::DirectionalLightData dirLight{}; bool hasDirLight = false;
    ECSWorldLike::PointLightData pointLight{}; bool hasPointLight = false;
    ECSWorldLike::SpotLightData spotLight{}; bool hasSpotLight = false;
    ECSWorldLike::CameraData camera{}; bool hasCamera = false;
    ECSWorldLike::SkyData sky{}; bool hasSky = false;
    ECSWorldLike::FogData fog{}; bool hasFog = false;
    ECSWorldLike::SkyLightData skyLight{}; bool hasSkyLight = false;

        // name
        size_t p2 = 0;
        TryFindNext(objStr, p2, "\"name\"");
        TryFindNext(objStr, p2, ":");
        TryReadQuotedString(objStr, p2, name);

        // transform
        size_t pt = objStr.find("\"transform\"");
        if (pt != string::npos) {
            size_t p = pt;
            if (TryFindNext(objStr, p, "\"position\"")) {
                TryFindNext(objStr, p, ":"); hasTr = true; TryReadFloatArray3(objStr, p, t.position);
            }
            if (TryFindNext(objStr, p, "\"rotationEuler\"")) {
                TryFindNext(objStr, p, ":"); TryReadFloatArray3(objStr, p, t.rotationEuler);
            }
            if (TryFindNext(objStr, p, "\"scale\"")) {
                TryFindNext(objStr, p, ":"); TryReadFloatArray3(objStr, p, t.scale);
            }
        }
        // mesh
        size_t pm = objStr.find("\"mesh\"");
        if (pm != string::npos) {
            size_t p = pm;
            string kind;
            if (TryFindNext(objStr, p, "\"kind\"")) { TryFindNext(objStr, p, ":"); TryReadQuotedString(objStr, p, kind); }
            if (kind == "Static") mk = ECSWorldLike::MeshKind::StaticCube;
            else if (kind == "Dynamic") {
                mk = ECSWorldLike::MeshKind::DynamicGLTF;
                if (TryFindNext(objStr, p, "\"assetId\"")) { TryFindNext(objStr, p, ":"); TryReadQuotedString(objStr, p, assetId); }
            }
        }

        size_t pdl = objStr.find("\"directionalLight\"");
        if (pdl != string::npos) {
            size_t p = pdl;
            if (TryFindNext(objStr, p, "\"direction\"")) {
                TryFindNext(objStr, p, ":");
                TryReadFloatArray3(objStr, p, dirLight.direction);
            }
            if (TryFindNext(objStr, p, "\"intensity\"")) {
                TryFindNext(objStr, p, ":");
                TryReadFloat(objStr, p, dirLight.intensity);
            }
            hasDirLight = true;
        }

        size_t ppl = objStr.find("\"pointLight\"");
        if (ppl != string::npos) {
            size_t p = ppl;
            if (TryFindNext(objStr, p, "\"color\"")) {
                TryFindNext(objStr, p, ":");
                TryReadFloatArray3(objStr, p, pointLight.color);
            }
            if (TryFindNext(objStr, p, "\"intensity\"")) {
                TryFindNext(objStr, p, ":");
                TryReadFloat(objStr, p, pointLight.intensity);
            }
            if (TryFindNext(objStr, p, "\"range\"")) {
                TryFindNext(objStr, p, ":");
                TryReadFloat(objStr, p, pointLight.range);
            }
            hasPointLight = true;
        }

        size_t psl = objStr.find("\"spotLight\"");
        if (psl != string::npos) {
            size_t p = psl;
            if (TryFindNext(objStr, p, "\"direction\"")) {
                TryFindNext(objStr, p, ":");
                TryReadFloatArray3(objStr, p, spotLight.direction);
            }
            if (TryFindNext(objStr, p, "\"color\"")) {
                TryFindNext(objStr, p, ":");
                TryReadFloatArray3(objStr, p, spotLight.color);
            }
            if (TryFindNext(objStr, p, "\"intensity\"")) {
                TryFindNext(objStr, p, ":");
                TryReadFloat(objStr, p, spotLight.intensity);
            }
            if (TryFindNext(objStr, p, "\"range\"")) {
                TryFindNext(objStr, p, ":");
                TryReadFloat(objStr, p, spotLight.range);
            }
            if (TryFindNext(objStr, p, "\"angleDegrees\"")) {
                TryFindNext(objStr, p, ":");
                TryReadFloat(objStr, p, spotLight.angleDegrees);
            }
            hasSpotLight = true;
        }

        size_t pcam = objStr.find("\"camera\"");
        if (pcam != string::npos) {
            size_t p = pcam;
            if (TryFindNext(objStr, p, "\"fovYRadians\"")) {
                TryFindNext(objStr, p, ":");
                TryReadFloat(objStr, p, camera.fovYRadians);
            }
            if (TryFindNext(objStr, p, "\"nearZ\"")) {
                TryFindNext(objStr, p, ":");
                TryReadFloat(objStr, p, camera.nearZ);
            }
            if (TryFindNext(objStr, p, "\"farZ\"")) {
                TryFindNext(objStr, p, ":");
                TryReadFloat(objStr, p, camera.farZ);
            }
            hasCamera = true;
        }

        size_t psky = objStr.find("\"sky\"");
        if (psky != string::npos) {
            size_t p = psky;
            if (TryFindNext(objStr, p, "\"enabled\"")) {
                TryFindNext(objStr, p, ":");
                TryReadBool(objStr, p, sky.enabled);
            }
            if (TryFindNext(objStr, p, "\"exposure\"")) {
                TryFindNext(objStr, p, ":");
                TryReadFloat(objStr, p, sky.exposure);
            }
            if (TryFindNext(objStr, p, "\"environmentPath\"")) {
                TryFindNext(objStr, p, ":");
                TryReadQuotedString(objStr, p, sky.environmentPath);
            }
            if (TryFindNext(objStr, p, "\"intensity\"")) {
                TryFindNext(objStr, p, ":");
                TryReadFloat(objStr, p, sky.intensity);
            }
            if (TryFindNext(objStr, p, "\"rotationDegrees\"")) {
                TryFindNext(objStr, p, ":");
                TryReadFloat(objStr, p, sky.rotationDegrees);
            }
            if (TryFindNext(objStr, p, "\"showBackground\"")) {
                TryFindNext(objStr, p, ":");
                TryReadBool(objStr, p, sky.showBackground);
            }
            hasSky = true;
        }

        size_t pfog = objStr.find("\"fog\"");
        if (pfog != string::npos) {
            size_t p = pfog;
            if (TryFindNext(objStr, p, "\"enabled\"")) {
                TryFindNext(objStr, p, ":");
                TryReadBool(objStr, p, fog.enabled);
            }
            if (TryFindNext(objStr, p, "\"color\"")) {
                TryFindNext(objStr, p, ":");
                TryReadFloatArray3(objStr, p, fog.color);
            }
            if (TryFindNext(objStr, p, "\"density\"")) {
                TryFindNext(objStr, p, ":");
                TryReadFloat(objStr, p, fog.density);
            }
            if (TryFindNext(objStr, p, "\"startDistance\"")) {
                TryFindNext(objStr, p, ":");
                TryReadFloat(objStr, p, fog.startDistance);
            }
            if (TryFindNext(objStr, p, "\"maxDistance\"")) {
                TryFindNext(objStr, p, ":");
                TryReadFloat(objStr, p, fog.maxDistance);
            }
            if (TryFindNext(objStr, p, "\"heightFalloff\"")) {
                TryFindNext(objStr, p, ":");
                TryReadFloat(objStr, p, fog.heightFalloff);
            }
            hasFog = true;
        }

        size_t pslight = objStr.find("\"skyLight\"");
        if (pslight != string::npos) {
            size_t p = pslight;
            if (TryFindNext(objStr, p, "\"enabled\"")) {
                TryFindNext(objStr, p, ":");
                TryReadBool(objStr, p, skyLight.enabled);
            }
            if (TryFindNext(objStr, p, "\"color\"")) {
                TryFindNext(objStr, p, ":");
                TryReadFloatArray3(objStr, p, skyLight.color);
            }
            if (TryFindNext(objStr, p, "\"intensity\"")) {
                TryFindNext(objStr, p, ":");
                TryReadFloat(objStr, p, skyLight.intensity);
            }
            hasSkyLight = true;
        }

        // Create entity
        void* h = world.CreateObject(name.empty()? string("Object"):name);
        if (hasTr) world.SetTransform(h, t);
        if (mk != ECSWorldLike::MeshKind::None) world.SetMesh(h, mk, assetId);
        if (hasDirLight) world.SetDirectionalLight(h, dirLight);
        if (hasPointLight) world.SetPointLight(h, pointLight);
        if (hasSpotLight) world.SetSpotLight(h, spotLight);
        if (hasCamera) world.SetCamera(h, camera);
        if (hasSky) world.SetSky(h, sky);
        if (hasFog) world.SetFog(h, fog);
        if (hasSkyLight) world.SetSkyLight(h, skyLight);

        // skip trailing commas and whitespace
        while (pos < s.size() && isspace(static_cast<unsigned char>(s[pos])))
            ++pos;
        if (pos < s.size() && s[pos] == ',')
            ++pos;
    }
    return true;
}

}} // namespace
