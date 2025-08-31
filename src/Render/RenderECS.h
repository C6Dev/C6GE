#pragma once

#include <entt/entt.hpp>
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include "../Object/Object.h"

namespace C6GE {

class RenderECS {
private:
    entt::registry& registry;
    
    // Helper function to create transformation matrix
    void createTransformMatrix(const Transform& transform, float* mtx);
    
    // Render single object
    void renderSingleObject(entt::entity entity, const Transform& transform, 
                           const Model& model, const Material& material, 
                           const Texture& texture);
    
    // Render instanced objects
    void renderInstancedObjects(entt::entity entity, const Transform& transform,
                               const Model& model, const Material& material,
                               const Texture& texture, const Instanced& instanced);

public:
    RenderECS(entt::registry& reg) : registry(reg) {}
    
    // Render a specific object by name
    void RenderObject(const std::string& objectName);
    
    // Render all objects in the registry
    void RenderAllObjects();
    
    // Render objects with specific components
    void RenderObjectsWithComponents();
    
    // Set view and projection matrices
    void SetViewProjection(const float* view, const float* proj);
    
    // Set HDR parameters
    void SetHDRParams(float exposure, float gamma, float whitePoint, float threshold);
};

} // namespace C6GE
