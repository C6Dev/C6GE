#include "../include/Render/RenderPicker.h"

RenderPicker::RenderType RenderPicker::GetRenderType() {
    #ifdef __APPLE__
        return RenderType::Metal;
    #else
        return RenderType::Vulkan;
    #endif
}