#include "../include/Render/RenderPicker.h"

// The RenderPicker class is responsible for selecting the appropriate rendering backend
RenderPicker::RenderType RenderPicker::GetRenderType() {
    #ifdef __APPLE__
        return RenderType::Metal;
    #else
        return RenderType::Vulkan;
    #endif
}