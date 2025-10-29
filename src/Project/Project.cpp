#include "Project/Project.h"
#include <fstream>
#include <sstream>
#include <regex>

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
        o << "        \"name\": \"" << JsonEscape(it.name) << "\",\n";
        if (it.hasTransform) {
            o << "        \"transform\": {\n";
            o << "          \"position\": [" << it.tr.position[0] << ", " << it.tr.position[1] << ", " << it.tr.position[2] << "],\n";
            o << "          \"rotationEuler\": [" << it.tr.rotationEuler[0] << ", " << it.tr.rotationEuler[1] << ", " << it.tr.rotationEuler[2] << "],\n";
            o << "          \"scale\": [" << it.tr.scale[0] << ", " << it.tr.scale[1] << ", " << it.tr.scale[2] << "]\n";
            o << "        },\n";
        }
        // Mesh
        o << "        \"mesh\": {";
        if (it.meshKind == ECSWorldLike::MeshKind::StaticCube) {
            o << "\"kind\": \"Static\", \"staticType\": \"Cube\"";
        } else if (it.meshKind == ECSWorldLike::MeshKind::DynamicGLTF) {
            o << "\"kind\": \"Dynamic\", \"assetId\": \"" << JsonEscape(it.assetId) << "\"";
        } else {
            o << "\"kind\": \"None\"";
        }
        o << "}\n";
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
        pos = nextBrace+1;
        // parse one object
        string name;
        ECSWorldLike::TransformData t{}; bool hasTr=false;
        ECSWorldLike::MeshKind mk = ECSWorldLike::MeshKind::None; string assetId;

        // naive scan between braces until matching '}'
        size_t objEnd = s.find('}', pos);
        if (objEnd == string::npos) break;
        string objStr = s.substr(pos, objEnd-pos);

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

        // Create entity
        void* h = world.CreateObject(name.empty()? string("Object"):name);
        if (hasTr) world.SetTransform(h, t);
        if (mk != ECSWorldLike::MeshKind::None) world.SetMesh(h, mk, assetId);

        pos = objEnd+1;
        // skip comma
        if (pos < s.size() && s[pos] == ',') ++pos;
    }
    return true;
}

}} // namespace
