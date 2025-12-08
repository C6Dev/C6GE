#pragma once

#include "main.h"

#include "Logger.h"

class DirectEngine_API RenderPicker {
public:
    enum class RenderType {
        Vulkan,
        Metal
    };

    static RenderType GetRenderType();
};