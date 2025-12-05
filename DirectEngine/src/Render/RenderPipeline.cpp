#include "../../include/Render/RenderPipeline.h"

// The CreateRender method initializes the rendering pipeline based on the selected RenderType.
DirectEngine_API void RenderPipeline::CreateRender(RenderPicker::RenderType Type) {
    // Depending on the RenderType, create the appropriate rendering instance.
    switch (Type) { // Type is of enum class RenderPicker::RenderType
        case RenderPicker::RenderType::Vulkan: { // If the RenderType is Vulkan
            std::cout << "Creating Vulkan Render Instance" << std::endl;
            RenderVulkan renderVulkan; // Create an instance of RenderVulkan
            renderVulkan.CreateInstance(); // Call CreateInstance method
            renderVulkan.setupDebugMessenger(); // Set up debug messenger
            break;
        }
        case RenderPicker::RenderType::Metal: { // If the RenderType is Metal
            std::cout << "Creating Metal Render Instance" << std::endl;
            break;
        }
        default: { // If the RenderType is unsupported
            throw std::runtime_error("Unsupported Render Type");
        }
    }
}

// The CleanupRenderer method cleans up the rendering pipeline based on the selected RenderType.
DirectEngine_API void RenderPipeline::CleanupRenderer(RenderPicker::RenderType Type) {
    switch (Type) { // Type is of enum class RenderPicker::RenderType
        case RenderPicker::RenderType::Vulkan: { // If the RenderType is Vulkan
            std::cout << "Cleaning up Vulkan Renderer" << std::endl;
            RenderVulkan renderVulkan; // Create an instance of RenderVulkan
            renderVulkan.CleanupVulkanRenderer(); // Call CleanupVulkanRenderer method
            break;
        }
        case RenderPicker::RenderType::Metal: { // If the RenderType is Metal
            std::cout << "Cleaning up Metal Renderer" << std::endl;
            break;
        }
        default: { // If the RenderType is unsupported
            throw std::runtime_error("Unsupported Render Type");
        }
    }
}