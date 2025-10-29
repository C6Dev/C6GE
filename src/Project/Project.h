// Single, canonical header contents
#pragma once
#include <string>
#include <vector>
#include <filesystem>

namespace Diligent { namespace ProjectSystem {

struct ProjectConfig
{
    // Absolute paths and metadata
    std::filesystem::path projectFile;   // path to .c6proj
    std::filesystem::path rootDir;       // directory that contains the .c6proj
    std::string           projectName {"Untitled"};
    std::string           engineVersion {"2026.1"};
    std::filesystem::path assetsDir;     // defaults to root/Assets
    std::filesystem::path worldsDir;     // defaults to root/Worlds
    std::filesystem::path modelsDir;     // defaults to assetsDir/Models
    std::filesystem::path startupWorld;  // optional; defaults to worldsDir/Default.world
};

// Very small project manager with JSON-on-disk (minimal parser/writer for known fields)
class ProjectManager
{
public:
    const ProjectConfig& GetConfig() const { return m_Config; }
    ProjectConfig&       GetConfig()       { return m_Config; }

    bool CreateDefault(const std::filesystem::path& projFile, const std::string& name);
    bool Load(const std::filesystem::path& projFile);
    bool Save();

    // Discovery helpers
    std::vector<std::filesystem::path> ListWorldFiles() const;
    std::vector<std::filesystem::path> ListModelFiles() const; // .c6m files
    std::vector<std::filesystem::path> ListGLTFFiles() const;  // .gltf/.glb within assets

    // Asset conversion: packs a glTF path into a small .c6m JSON pointing to source.
    // Returns path to the created .c6m (or existing) or empty on failure.
    std::filesystem::path ConvertGLTFToC6M(const std::filesystem::path& gltfPath) const;

    // Utility: find any .c6proj in startDir or parents. Returns empty if none found.
    static std::filesystem::path FindNearestProject(const std::filesystem::path& startDir);

private:
    ProjectConfig m_Config{};
};

// An abstract view over ECS world to decouple IO
class ECSWorldLike
{
public:
    virtual ~ECSWorldLike() = 0; // Make class polymorphic in this header
    struct TransformData { float position[3]{0,0,0}; float rotationEuler[3]{0,0,0}; float scale[3]{1,1,1}; };
    enum class MeshKind { None, StaticCube, DynamicGLTF };
    struct ObjectViewItem {
        void*        handle = nullptr; // opaque
        std::string  name;
        bool         hasTransform = false;
        TransformData tr{};
        MeshKind     meshKind = MeshKind::None;
        std::string  assetId; // for DynamicGLTF
    };
    // for Save
    virtual std::vector<ObjectViewItem> EnumerateObjects() const = 0;
    // for Load
    virtual void Clear() = 0;
    virtual void* CreateObject(const std::string& name) = 0;
    virtual void SetTransform(void* handle, const TransformData& t) = 0;
    virtual void SetMesh(void* handle, MeshKind kind, const std::string& assetId) = 0;
};

inline ECSWorldLike::~ECSWorldLike() = default;

struct WorldIO
{
    static bool Save(const std::filesystem::path& path, ECSWorldLike& world);
    static bool Load(const std::filesystem::path& path, ECSWorldLike& world);
};

}} // namespace Diligent::ProjectSystem
