#pragma once

#include <entt/entt.hpp>
#include <string>
#include <unordered_map>
#include <memory>
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include "../external/bgfx.cmake/bgfx/examples/common/bgfx_utils.h"

namespace C6GE {

// Forward declarations
class TextureLoader;

// Component structs
struct Transform {
    bx::Vec3 position = {0.0f, 0.0f, 0.0f};
    bx::Vec3 rotation = {0.0f, 0.0f, 0.0f};
    bx::Vec3 scale = {1.0f, 1.0f, 1.0f};
    
    // Helper methods
    void setPosition(float x, float y, float z) { position = {x, y, z}; }
    void setRotation(float x, float y, float z) { rotation = {x, y, z}; }
    void setScale(float x, float y, float z) { scale = {x, y, z}; }
    void setScale(float uniform) { scale = {uniform, uniform, uniform}; }
};

struct Model {
    ::Mesh* mesh = nullptr;
    std::string meshPath;
    
    Model() = default;
    Model(::Mesh* m) : mesh(m) {}
    Model(const std::string& path) : meshPath(path) {}
};

struct Texture {
    bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
    std::string texturePath;
    
    Texture() = default;
    Texture(bgfx::TextureHandle h) : handle(h) {}
    Texture(const std::string& path) : texturePath(path) {}
};

struct Material {
    bgfx::ProgramHandle shader = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle textureUniform = BGFX_INVALID_HANDLE;
    uint64_t renderState = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS;
    
    Material() = default;
    Material(bgfx::ProgramHandle s, bgfx::UniformHandle tu) : shader(s), textureUniform(tu) {}
};

struct Instanced {
    bool enabled = false;
    uint32_t instanceCount = 1;
    std::vector<Transform> instances;
    
    Instanced() = default;
    Instanced(uint32_t count) : enabled(true), instanceCount(count) {
        instances.resize(count);
    }
};

// Object class that manages entities
class Object {
private:
    entt::entity entity;
    entt::registry& registry;
    std::string name;

public:
    Object(entt::entity e, entt::registry& reg, const std::string& n) 
        : entity(e), registry(reg), name(n) {}
    
    // Template method to add components
    template<typename T, typename... Args>
    T& AddComponent(Args&&... args) {
        return registry.emplace<T>(entity, std::forward<Args>(args)...);
    }
    
    // Template method to get components
    template<typename T>
    T* GetComponent() {
        return registry.try_get<T>(entity);
    }
    
    template<typename T>
    const T* GetComponent() const {
        return registry.try_get<T>(entity);
    }
    
    // Template method to remove components
    template<typename T>
    void RemoveComponent() {
        registry.remove<T>(entity);
    }
    
    // Check if entity has component
    template<typename T>
    bool HasComponent() const {
        return registry.all_of<T>(entity);
    }
    
    // Get entity
    entt::entity GetEntity() const { return entity; }
    
    // Get name
    const std::string& GetName() const { return name; }
    
    // Destroy the object
    void Destroy() {
        registry.destroy(entity);
    }
};

// ObjectManager class that manages all objects
class ObjectManager {
private:
    entt::registry registry;
    std::unordered_map<std::string, entt::entity> nameToEntity;
    std::unordered_map<entt::entity, std::string> entityToName;
    
    // Systems
    void updateTransforms();
    void updateInstancing();

public:
    ObjectManager() = default;
    ~ObjectManager() = default;
    
    // Create a new object
    Object CreateObject(const std::string& name);
    
    // Get an existing object
    Object GetObject(const std::string& name);
    
    // Check if object exists
    bool ObjectExists(const std::string& name) const;
    
    // Destroy an object
    void DestroyObject(const std::string& name);
    
    // Get the registry (for systems)
    entt::registry& GetRegistry() { return registry; }
    const entt::registry& GetRegistry() const { return registry; }
    
    // Update systems
    void Update();
    
    // Clear all objects
    void Clear();
};

} // namespace C6GE
