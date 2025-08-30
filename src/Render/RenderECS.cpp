#include "RenderECS.h"
#include "../MeshLoader/MeshLoader.h"
#include "../TextureLoader/TextureLoader.h"
#include <iostream>

namespace C6GE {

void RenderECS::createTransformMatrix(const Transform& transform, float* mtx) {
    // Create scale matrix
    float scaleMtx[16];
    bx::mtxScale(scaleMtx, transform.scale.x, transform.scale.y, transform.scale.z);
    
    // Create rotation matrix
    float rotMtx[16];
    bx::mtxRotateXYZ(rotMtx, transform.rotation.x, transform.rotation.y, transform.rotation.z);
    
    // Create translation matrix
    float posMtx[16];
    bx::mtxTranslate(posMtx, transform.position.x, transform.position.y, transform.position.z);
    
    // Combine transformations: position * rotation * scale
    float tempMtx[16];
    bx::mtxMul(tempMtx, posMtx, rotMtx);
    bx::mtxMul(mtx, tempMtx, scaleMtx);
}

void RenderECS::renderSingleObject(entt::entity entity, const Transform& transform, 
                                  const Model& model, const Material& material, 
                                  const Texture& texture) {
    if (!model.mesh) {
        std::cout << "Warning: Model has no mesh for entity " << (uint32_t)entity << std::endl;
        return;
    }
    
    // Create transformation matrix
    float mtx[16];
    createTransformMatrix(transform, mtx);
    
    // Set transform
    bgfx::setTransform(mtx);
    
    // Set texture if available
    if (bgfx::isValid(texture.handle)) {
        bgfx::setTexture(0, material.textureUniform, texture.handle);
    }
    
    // Set vertex and index buffers
    bgfx::setVertexBuffer(0, model.mesh->m_groups[0].m_vbh);
    bgfx::setIndexBuffer(model.mesh->m_groups[0].m_ibh);
    
    // Submit for rendering
    bgfx::submit(0, material.shader, material.renderState);
}

void RenderECS::renderInstancedObjects(entt::entity entity, const Transform& transform,
                                      const Model& model, const Material& material,
                                      const Texture& texture, const Instanced& instanced) {
    if (!model.mesh) {
        std::cout << "Warning: Model has no mesh for instanced entity " << (uint32_t)entity << std::endl;
        return;
    }
    

    
    // Set texture if available
    if (bgfx::isValid(texture.handle)) {
        bgfx::setTexture(0, material.textureUniform, texture.handle);
    }
    
    // Set vertex and index buffers
    bgfx::setVertexBuffer(0, model.mesh->m_groups[0].m_vbh);
    bgfx::setIndexBuffer(model.mesh->m_groups[0].m_ibh);
    
    // Render each instance
    for (uint32_t i = 0; i < instanced.instanceCount; ++i) {
        // Create transformation matrix for this instance
        float mtx[16];
        
        if (i < instanced.instances.size()) {
            // Use instance-specific transform
            createTransformMatrix(instanced.instances[i], mtx);
        } else {
            // Use base transform
            createTransformMatrix(transform, mtx);
        }
        
        // Set transform
        bgfx::setTransform(mtx);
        
        // Submit for rendering
        bgfx::submit(0, material.shader, material.renderState);
    }
}

void RenderECS::RenderObject(const std::string& objectName) {
    // Find entity by name (this is a simplified approach - in a real system you'd have a name component)
    auto view = registry.view<Transform, Model, Material, Texture>();
    
    for (auto entity : view) {
        auto& transform = view.get<Transform>(entity);
        auto& model = view.get<Model>(entity);
        auto& material = view.get<Material>(entity);
        auto& texture = view.get<Texture>(entity);
        
        // Check if this entity has instancing
        if (auto instanced = registry.try_get<Instanced>(entity)) {
            if (instanced->enabled) {
                renderInstancedObjects(entity, transform, model, material, texture, *instanced);
            } else {
                renderSingleObject(entity, transform, model, material, texture);
            }
        } else {
            renderSingleObject(entity, transform, model, material, texture);
        }
    }
}

void RenderECS::RenderAllObjects() {
    // Render all entities that have the required components
    auto view = registry.view<Transform, Model, Material, Texture>();
    
    for (auto entity : view) {
        auto& transform = view.get<Transform>(entity);
        auto& model = view.get<Model>(entity);
        auto& material = view.get<Material>(entity);
        auto& texture = view.get<Texture>(entity);
        
        // Check if this entity has instancing
        if (auto instanced = registry.try_get<Instanced>(entity)) {
            if (instanced->enabled) {
                renderInstancedObjects(entity, transform, model, material, texture, *instanced);
            } else {
                renderSingleObject(entity, transform, model, material, texture);
            }
        } else {
            renderSingleObject(entity, transform, model, material, texture);
        }
    }
}

void RenderECS::RenderObjectsWithComponents() {
    // This method can be used to render objects with specific component combinations
    // For now, it just calls RenderAllObjects
    RenderAllObjects();
}

void RenderECS::SetViewProjection(const float* view, const float* proj) {
    bgfx::setViewTransform(0, view, proj);
}

} // namespace C6GE
