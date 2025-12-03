#include "../../include/Render/RenderPipeline.h"

DirectEngine_API void RenderPipeline::CreateRender(RenderPicker::RenderType Type) {
    switch (Type) {
        case RenderPicker::RenderType::Vulkan: {
            std::cout << "Creating Vulkan Render Instance" << std::endl;
            RenderVulkan renderVulkan;
            renderVulkan.CreateInstance();
            renderVulkan.setupDebugMessenger();
            break;
        }
        case RenderPicker::RenderType::Metal: {
            std::cout << "Creating Metal Render Instance" << std::endl;
            break;
        }
        default: {
            throw std::runtime_error("Unsupported Render Type");
        }
    }
}

DirectEngine_API void RenderPipeline::CleanupRenderer(RenderPicker::RenderType Type) {
    switch (Type) {
        case RenderPicker::RenderType::Vulkan: {
            std::cout << "Cleaning up Vulkan Renderer" << std::endl;
            RenderVulkan renderVulkan;
            renderVulkan.CleanupVulkanRenderer();
            break;
        }
        case RenderPicker::RenderType::Metal: {
            std::cout << "Cleaning up Metal Renderer" << std::endl;
            break;
        }
        default: {
            throw std::runtime_error("Unsupported Render Type");
        }
    }
}