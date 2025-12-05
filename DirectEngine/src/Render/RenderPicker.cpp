#include "../include/Render/RenderPicker.h"

// The RenderPicker class is responsible for selecting the appropriate rendering backend
RenderPicker::RenderType RenderPicker::GetRenderType() {
    // Platform-specific logic to determine the rendering backend
    #ifdef __APPLE__ // Check for Apple platforms
        return RenderType::Metal; // Use Metal on Apple platforms
    #else // Non-Apple platforms
        return RenderType::Vulkan; // Use Vulkan on non-Apple platforms
    #endif
}