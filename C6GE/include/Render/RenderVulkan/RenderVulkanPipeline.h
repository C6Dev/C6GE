#pragma once

#include "main.h"

#include <vector>
#include <string>

#if defined(__APPLE__)
#include <stdexcept>
#include <cstdint>

// Provide minimal Vulkan type declarations so the macOS stub compiles without the SDK.
struct VkInstance_T;
struct VkDebugUtilsMessengerEXT_T;
struct VkDebugUtilsMessengerCreateInfoEXT;
struct VkAllocationCallbacks;
using VkInstance = VkInstance_T*;
using VkDebugUtilsMessengerEXT = VkDebugUtilsMessengerEXT_T*;
using VkResult = int32_t;

class C6GE_API RenderVulkan {
    std::string reason = "Vulkan renderer is not supported on macOS";

public:
    void CreateInstance() {
        throw std::runtime_error(reason);
    }

    bool checkValidationLayerSupport() {
        throw std::runtime_error(reason);
    }

    void setupDebugMessenger() {
        throw std::runtime_error(reason);
    }

    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
        throw std::runtime_error(reason);
    }

    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
        throw std::runtime_error(reason);
    }

    void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
        throw std::runtime_error(reason);
    }

    void CleanupVulkanRenderer() {
        throw std::runtime_error(reason);
    }
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

class C6GE_API RenderVulkan {
public:
    void CreateInstance();
    bool checkValidationLayerSupport();
    void setupDebugMessenger();
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
    void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
    VkResult CreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback);
    void DestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator);
    void CleanupVulkanRenderer();

private:
    std::vector<const char*> GetRequiredExtensions(bool includeDebugUtils);
    std::vector<VkExtensionProperties> QueryInstanceExtensions();
    bool HasExtension(const std::vector<VkExtensionProperties>& extensions, const char* name);

    enum class DebugMessengerBackend {
        None,
        DebugUtils,
        DebugReport
    };

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData);

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallback(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT objectType,
    uint64_t object,
    size_t location,
    int32_t messageCode,
    const char* pLayerPrefix,
    const char* pMessage,
    void* pUserData);

    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    VkDebugReportCallbackEXT debugReportCallbackHandle = VK_NULL_HANDLE;
    bool validationLayersActive = false;
    DebugMessengerBackend debugMessengerBackend = DebugMessengerBackend::None;
};
#endif