#include "Object.h"
#include <unordered_map>

namespace C6GE {
    // The global entt registry that stores all entities and their components.
    entt::registry registry;
    // Maps object names (strings) to their corresponding entt::entity handles.
    std::unordered_map<std::string, entt::entity> nameToEntity;

    // Creates a new entity and associates it with the given name.
    void CreateObject(const std::string& name) {
        entt::entity entity = registry.create(); // Create a new entity in the registry
        nameToEntity[name] = entity; // Store the mapping from name to entity
    }

    // Retrieves the entity handle associated with the given name.
    // Returns entt::null if the name is not found.
    entt::entity GetObject(const std::string& name) {
        auto it = nameToEntity.find(name);
        if (it != nameToEntity.end()) {
            return it->second;
        }
        return entt::null;
    }

    // Logs the name and entity value for a given entity handle.
    // If the entity is not found in the nameToEntity map, logs a warning.
    void LogObjectInfo(entt::entity entity) {
        for (const auto& pair : nameToEntity) {
            if (pair.second == entity) {
                Log(LogLevel::info, "Entity name: " + pair.first + ", entt entity: " + std::to_string(static_cast<uint32_t>(entity)));
                return;
            }
        }
        Log(LogLevel::warning, "Entity not found in nameToEntity map. entt entity: " + std::to_string(static_cast<uint32_t>(entity)));
    }
}