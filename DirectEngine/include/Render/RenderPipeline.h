#pragma once

#include "RenderPicker.h"
#include <iostream>
#include <stdexcept>
#include <vector>
#include "RenderVulkan/RenderVulkanPipeline.h"

#include "Logger.h"

class DirectEngine_API RenderPipeline {
    public:
        void CreateRender(RenderPicker::RenderType Type);
        void CleanupRenderer(RenderPicker::RenderType Type);
};