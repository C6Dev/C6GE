#pragma once

#include "main.h"
#if !defined(__APPLE__)

#include <vector>

#include <vulkan/vulkan.h>

class C6GE_API RenderVulkan {
    public:
        void CreateInstance(const std::vector<const char*>& extensions);
        void CleanupVulkanRenderer();
    
    private:
        VkInstance instance;
};
#endif