#pragma once

#include "RenderPicker.h"
#include <iostream>
#include <stdexcept>
#include <vector>
#include "RenderVulkan/RenderVulkanPipeline.h"

class C6GE_API RenderPipeline {
    public:
        void CreateRender(RenderPicker::RenderType Type);
        void CleanupRenderer(RenderPicker::RenderType Type);
};