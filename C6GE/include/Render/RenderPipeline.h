#pragma once

#include "RenderPicker.h"
#include <iostream>
#include <stdexcept>
#include <vector>
#include "RenderVulkan/RenderVulkanPipeline.h"

class C6GE_API RenderPipeline {
    public:
        void CreateInstance(RenderPicker::RenderType Type, const std::vector<const char*>& extensions);
        void CleanupRenderer(RenderPicker::RenderType Type);
};