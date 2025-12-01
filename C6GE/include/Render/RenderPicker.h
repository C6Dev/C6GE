#pragma once

#include "main.h"

class C6GE_API RenderPicker {
public:
    enum class RenderType {
        Vulkan,
        Metal
    };

    static RenderType GetRenderType();
};