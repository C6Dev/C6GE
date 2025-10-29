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
    void DestroyEntity(entt::entity e) {
        // Remove from name maps if present
        auto it = entity_to_name_.find(e);
        if (it != entity_to_name_.end()) {
            const std::string& name = it->second;
            auto itN = name_to_entity_.find(name);
            if (itN != name_to_entity_.end() && itN->second == e)
                name_to_entity_.erase(itN);
            entity_to_name_.erase(it);
        }
        registry_.destroy(e);
    }

    // High-level object API
    GameObject CreateObject(const std::string& name);
    GameObject GetObject(const std::string& name);
    bool HasObject(const std::string& name) const { return name_to_entity_.count(name) > 0; }
    bool DestroyObject(const std::string& name);
    // Attempt to rename an entity; returns false if name is taken or invalid
    bool RenameEntity(entt::entity e, const std::string& newName);

private:
    friend class GameObject;
    entt::registry registry_;
    std::unordered_map<std::string, entt::entity> name_to_entity_;
    std::unordered_map<entt::entity, std::string> entity_to_name_;
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
    entity_to_name_[e] = name;
    return GameObject{this, e, name};
}

inline GameObject World::GetObject(const std::string& name)
{
    auto it = name_to_entity_.find(name);
    if (it == name_to_entity_.end())
        throw std::runtime_error("Object not found: " + name);
    return GameObject{this, it->second, name};
}

inline bool World::DestroyObject(const std::string& name)
{
    auto it = name_to_entity_.find(name);
    if (it == name_to_entity_.end())
        return false;
    entt::entity e = it->second;
    name_to_entity_.erase(it);
    entity_to_name_.erase(e);
    registry_.destroy(e);
    return true;
}

inline bool World::RenameEntity(entt::entity e, const std::string& newName)
{
    if (e == entt::null || !registry_.valid(e))
        return false;
    if (newName.empty())
        return false;
    // Check if name taken by another entity
    auto itTaken = name_to_entity_.find(newName);
    if (itTaken != name_to_entity_.end() && itTaken->second != e)
        return false;
    // Find old name (if any)
    std::string oldName;
    auto itOld = entity_to_name_.find(e);
    if (itOld != entity_to_name_.end())
        oldName = itOld->second;
    // Update maps
    if (!oldName.empty())
        name_to_entity_.erase(oldName);
    name_to_entity_[newName] = e;
    entity_to_name_[e] = newName;
    // Update Name component if present
    if (registry_.any_of<Name>(e))
        registry_.get<Name>(e).value = newName;
    return true;
}

}} // namespace Diligent::ECS
