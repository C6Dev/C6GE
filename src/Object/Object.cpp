#include "Object.h"
#include "../MeshLoader/MeshLoader.h"
#include "../TextureLoader/TextureLoader.h"
#include <iostream>

namespace C6GE {

Object ObjectManager::CreateObject(const std::string& name) {
    // Check if object already exists
    if (ObjectExists(name)) {
        std::cout << "Warning: Object '" << name << "' already exists. Returning existing object." << std::endl;
        return GetObject(name);
    }
    
    // Create new entity
    entt::entity entity = registry.create();
    
    // Add default transform component
    registry.emplace<Transform>(entity);
    
    // Store name mappings
    nameToEntity[name] = entity;
    entityToName[entity] = name;
    
    return Object(entity, registry, name);
}

Object ObjectManager::GetObject(const std::string& name) {
    auto it = nameToEntity.find(name);
    if (it != nameToEntity.end()) {
        return Object(it->second, registry, name);
    }
    
    // Return invalid object
    return Object(entt::null, registry, "");
}

bool ObjectManager::ObjectExists(const std::string& name) const {
    return nameToEntity.find(name) != nameToEntity.end();
}

void ObjectManager::DestroyObject(const std::string& name) {
    auto it = nameToEntity.find(name);
    if (it != nameToEntity.end()) {
        entt::entity entity = it->second;
        
        // Remove from name mappings
        nameToEntity.erase(it);
        entityToName.erase(entity);
        
        // Destroy entity
        registry.destroy(entity);
    }
}

void ObjectManager::Update() {
    updateTransforms();
    updateInstancing();
}

void ObjectManager::updateTransforms() {
    // This system can be used to update transforms based on physics, animations, etc.
    // For now, it's empty but can be extended later
}

void ObjectManager::updateInstancing() {
    // This system can be used to update instancing data
    // For now, it's empty but can be extended later
}

void ObjectManager::Clear() {
    registry.clear();
    nameToEntity.clear();
    entityToName.clear();
}

} // namespace C6GE
