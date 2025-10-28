#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <entt/entt.hpp>

#include "Runtime/ECS/Components.h"

namespace Diligent { namespace ECS {

class GameObject;

class World {
public:
    World() = default;
    entt::registry& Registry() { return registry_; }

    // Low-level helpers
    entt::entity CreateEntity() { return registry_.create(); }
    void DestroyEntity(entt::entity e) { registry_.destroy(e); }

    // High-level object API
    GameObject CreateObject(const std::string& name);
    GameObject GetObject(const std::string& name);
    bool HasObject(const std::string& name) const { return name_to_entity_.count(name) > 0; }

private:
    friend class GameObject;
    entt::registry registry_;
    std::unordered_map<std::string, entt::entity> name_to_entity_;
};

class GameObject {
public:
    GameObject() = default;
    GameObject(World* world, entt::entity e, const std::string& name)
        : world_(world), entity_(e), name_(name) {}

    template<typename T, typename... Args>
    T& AddComponent(Args&&... args) {
        return world_->registry_.emplace<T>(entity_, std::forward<Args>(args)...);
    }

    template<typename T>
    T& GetComponent() {
        return world_->registry_.get<T>(entity_);
    }

    template<typename T>
    bool HasComponent() const {
        return world_->registry_.all_of<T>(entity_);
    }

    entt::entity Handle() const { return entity_; }
    const std::string& Name() const { return name_; }

private:
    World* world_ = nullptr;
    entt::entity entity_ { entt::null };
    std::string name_;
};

// Inline implementations
inline GameObject World::CreateObject(const std::string& name)
{
    if (name_to_entity_.count(name) != 0)
        throw std::runtime_error("Object with name already exists: " + name);
    auto e = registry_.create();
    registry_.emplace<Name>(e, Name{name});
    name_to_entity_[name] = e;
    return GameObject{this, e, name};
}

inline GameObject World::GetObject(const std::string& name)
{
    auto it = name_to_entity_.find(name);
    if (it == name_to_entity_.end())
        throw std::runtime_error("Object not found: " + name);
    return GameObject{this, it->second, name};
}

}} // namespace Diligent::ECS
