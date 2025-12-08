#include "../../include/Render/RenderPipeline.h"


using namespace DirectLogger;

// The CreateRender method initializes the rendering pipeline based on the selected RenderType.
DirectEngine_API void RenderPipeline::CreateRender(RenderPicker::RenderType Type) {
    switch (Type) {
        case RenderPicker::RenderType::Vulkan: {
            Log(LogLevel::info, "Creating Vulkan Render Instance", "Render");
            break;
        }
        case RenderPicker::RenderType::Metal: {
            Log(LogLevel::info, "Creating Metal Render Instance", "Render");
            break;
        }
        default: {
            Log(LogLevel::critical, "Unsupported Render Type", "Render");
            throw;
        }
    }
}

// The CleanupRenderer method cleans up the rendering pipeline based on the selected RenderType.
DirectEngine_API void RenderPipeline::CleanupRenderer(RenderPicker::RenderType Type) {
    switch (Type) {
        case RenderPicker::RenderType::Vulkan: {
            Log(LogLevel::info, "Cleaning up Vulkan Renderer", "Render");
            break;
        }
        case RenderPicker::RenderType::Metal: {
            Log(LogLevel::info, "Cleaning up Metal Renderer", "Render");
            break;
        }
        default: {
            Log(LogLevel::critical, "Unsupported Render Type for Cleanup", "Render");
            throw;
        }
    }
}