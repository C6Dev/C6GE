#include <entt/entt.hpp>
#include "../../Logging/Log.h"

namespace C6GE {
    extern entt::registry registry;
    extern std::unordered_map<std::string, entt::entity> nameToEntity;
    void CreateObject(const std::string& name);
    void RegisterObject(std::string name);
    entt::entity GetObject(const std::string& name);
    void LogObjectInfo(entt::entity entity);
    template<typename T, typename... Args>
    T& AddComponent(const std::string& name, Args&&... args) {
        auto it = nameToEntity.find(name);
        if (it != nameToEntity.end()) {
            return registry.emplace<T>(it->second, std::forward<Args>(args)...);
        } else {
            throw std::runtime_error("Entity with name '" + name + "' not found.");
        }
    }
}