#include <entt/entt.hpp>
#include "../../Logging/Log.h"
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <vector>

namespace C6GE {

    // Global registry for all entities
    extern entt::registry registry;

    // Maps object names to their corresponding entities
    extern std::unordered_map<std::string, entt::entity> nameToEntity;

    // Creates a new object with the given name
    void CreateObject(const std::string& name);

    // Registers an object by name
    void RegisterObject(std::string name);

    // Retrieves the entity associated with the given name
    #ifdef _WIN32
#undef GetObject
#endif
entt::entity GetObject(const std::string& name);

    // Logs information about the specified entity
    void LogObjectInfo(entt::entity entity);

    // Adds a component of type T to the entity with the given name
    template<typename T, typename... Args>
    T& AddComponent(const std::string& name, Args&&... args) {
        auto it = nameToEntity.find(name);
        if (it != nameToEntity.end()) {
            return registry.emplace<T>(it->second, std::forward<Args>(args)...);
        } else {
            throw std::runtime_error("Entity with name '" + name + "' not found.");
        }
    }

    // Gets a pointer to the component of type T from the entity with the given name
    template<typename T>
    T* GetComponent(const std::string& name) {
        auto it = nameToEntity.find(name);
        if (it != nameToEntity.end()) {
            return registry.try_get<T>(it->second);
        } else {
            throw std::runtime_error("Entity with name '" + name + "' not found.");
        }
    }

    // Gets all object names with the specified component type T
    template<typename T>
    std::vector<std::string> GetAllObjectsWithComponent() {
        std::vector<std::string> names;
        for (const auto& pair : nameToEntity) {
            if (registry.all_of<T>(pair.second)) {
                names.push_back(pair.first);
            }
        }
        return names;
    }

} // namespace C6GE
