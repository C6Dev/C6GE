#pragma once

#include "main.h"

#include <vector>
#include <string>

#include "Logger.h"

#if defined(__APPLE__)
#include <stdexcept>
#include <cstdint>

class DirectEngine_API RenderVulkan {
    std::string reason = "Vulkan renderer is not supported on macOS";

public:

};
#else

#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#include <windows.h>
#endif
#include <vulkan/vulkan.h>
#if defined(_WIN32)
#include <vulkan/vulkan_win32.h>
#endif

class DirectEngine_API RenderVulkan {
public:


private:

};
#endif