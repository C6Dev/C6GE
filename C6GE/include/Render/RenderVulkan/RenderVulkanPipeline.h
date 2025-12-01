#pragma once

#include "main.h"

#include <vector>

#if defined(__APPLE__)
#include <stdexcept>

class C6GE_API RenderVulkan {
    public:
        void CreateInstance(const std::vector<const char*>& extensions) {
            throw std::runtime_error("Vulkan renderer is not supported on macOS");
        }

        void CleanupVulkanRenderer() {}
};
#else

#include <vulkan/vulkan.h>

class C6GE_API RenderVulkan {
    public:
        void CreateInstance(const std::vector<const char*>& extensions);
        void CleanupVulkanRenderer();
    
    private:
        VkInstance instance;
};
#endif