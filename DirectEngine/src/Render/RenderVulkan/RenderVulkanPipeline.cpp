#include "../../../include/Render/RenderVulkan/RenderVulkanPipeline.h"

#if !defined(__APPLE__) // Vulkan is not supported on macOS

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

// Validation layers
namespace { // anonymous namespace
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

// Enable validation layers only in debug builds
#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif
} // namespace

// Helper function to get required extensions
std::vector<const char*> RenderVulkan::GetRequiredExtensions() {
    std::vector<const char*> extensions;

    extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME); // Required for all platforms

#if defined(_WIN32)
    extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME); // Windows-specific extension
#elif defined(__linux__)
#ifdef VK_KHR_XCB_SURFACE_EXTENSION_NAME
    extensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME); // Linux-specific extension
#else
    extensions.push_back("VK_KHR_xcb_surface"); // Fallback if not defined
#endif
#endif

    // Add debug utils extension if validation layers are enabled
    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

// The CreateInstance function implements the Vulkan instance creation process
DirectEngine_API void RenderVulkan::CreateInstance() {
    // Check for validation layer support
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("validation layers requested, but not available!");
    }

    // Application info (optional but useful)
    VkApplicationInfo appInfo{}; // Information about the application
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; // Structure type identifier
    appInfo.pApplicationName = "DirectEngine"; // Name of the application
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0); // Application version
    appInfo.pEngineName = "DirectEngine"; // Name of the engine
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0); // Engine version
    appInfo.apiVersion = VK_API_VERSION_1_0; // Vulkan API version

    // Get required extensions
    const auto extensions = GetRequiredExtensions();

    // Create instance info
    VkInstanceCreateInfo createInfo{}; // Structure specifying parameters for instance creation
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO; // Structure type identifier
    createInfo.pApplicationInfo = &appInfo; // Pointer to application info
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size()); // Number of enabled extensions
    createInfo.ppEnabledExtensionNames = extensions.data(); // Pointer to array of enabled extension names

    // Set up validation layers if enabled
    if (enableValidationLayers) {
        createInfo.enabledLayerCount   = static_cast<uint32_t>(validationLayers.size()); // Number of enabled layers
        createInfo.ppEnabledLayerNames = validationLayers.data(); // Pointer to array of enabled layer names
    } else {
        createInfo.enabledLayerCount   = 0; // No layers enabled
        createInfo.ppEnabledLayerNames = nullptr; // No layer names
    }

    // Set up debug messenger create info if validation layers are enabled
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{}; // Structure for debug messenger creation
    // Populate debug messenger create info
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size()); // Number of enabled layers
        createInfo.ppEnabledLayerNames = validationLayers.data(); // Pointer to array of enabled layer names

        populateDebugMessengerCreateInfo(debugCreateInfo); // Populate debug messenger create info
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo; // Pointer to next structure
    } else {
        createInfo.enabledLayerCount = 0; // No layers enabled

        createInfo.pNext = nullptr; // No next structure
    }

    // Create the Vulkan instance
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance!"); // Throw an error if instance creation fails
    }

    // Optional: List available extensions for debugging
    uint32_t extensionCount = 0; // Variable to hold the number of extensions
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr); // Get the count of available extensions

    // Get the available extensions
    std::vector<VkExtensionProperties> availableExtensions(extensionCount); // Vector to hold extension properties
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data()); // Retrieve extension properties

    // Print available extensions for debugging purposes
    std::cout << "available extensions:\n";
    for (const auto& extension : availableExtensions) {
        std::cout << '\t' << extension.extensionName << '\n';
    }

}

// Function to check if requested validation layers are supported
bool RenderVulkan::checkValidationLayerSupport() {
    uint32_t layerCount = 0; // Variable to hold the number of available layers
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr); // Get the count of available layers

    std::vector<VkLayerProperties> availableLayers(layerCount); // Vector to hold layer properties
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data()); // Retrieve layer properties

    // Check if all requested validation layers are available
    for (const char* layerName : validationLayers) {
        bool layerFound = false; // Flag to indicate if the layer is found

        // Search for the layer in the available layers
        for (const auto& layerProperties : availableLayers) {
            if (std::strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        // Return false if any requested layer is not found
        if (!layerFound) {
            return false;
        }
    }

    return true;
}

// Debug callback function for validation layers
VKAPI_ATTR VkBool32 VKAPI_CALL RenderVulkan::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, // Severity of the message
    VkDebugUtilsMessageTypeFlagsEXT messageType, // Type of the message
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, // Callback data containing message details
    void* pUserData) { // User data (not used here)

    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl; // Print the validation message to standard error

    // Return false to indicate that the Vulkan call should not be aborted
    return VK_FALSE;
}

// Function to set up the debug messenger
DirectEngine_API void RenderVulkan::setupDebugMessenger() {
    if (!enableValidationLayers) return; // No need to set up if validation layers are not enabled

    VkDebugUtilsMessengerCreateInfoEXT createInfo{}; // Structure for debug messenger creation
    populateDebugMessengerCreateInfo(createInfo); // Populate debug messenger create info
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT; // Structure type identifier
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT; // Message severity flags
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT; // Message type flags
    createInfo.pfnUserCallback = debugCallback; // Pointer to the debug callback function
    createInfo.pUserData = nullptr; // Optional

    // Create the debug messenger
    if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
        throw std::runtime_error("failed to set up debug messenger!");
    }
}

// Function to create the debug utils messenger
DirectEngine_API VkResult RenderVulkan::CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, // Pointer to the debug messenger create info
    const VkAllocationCallbacks* pAllocator, // Optional allocation callbacks
    VkDebugUtilsMessengerEXT* pDebugMessenger) {  // Pointer to the debug messenger handle

    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"); // Get the function pointer for vkCreateDebugUtilsMessengerEXT
    // Call the function if it is available
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }

    // Return an error if the function is not available
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

// Function to populate the debug messenger create info structure
DirectEngine_API void RenderVulkan::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {}; // Initialize the structure to zero
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT; // Structure type identifier
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT; // Message severity flags
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT; // Message type flags
    createInfo.pfnUserCallback = debugCallback; // Pointer to the debug callback function
}

// Function to destroy the debug utils messenger
DirectEngine_API void RenderVulkan::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"); // Get the function pointer for vkDestroyDebugUtilsMessengerEXT
    // Call the function if it is available
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

// Function to clean up the Vulkan renderer
DirectEngine_API void RenderVulkan::CleanupVulkanRenderer() {
    // Destroy debug messenger if it exists
    if (instance != VK_NULL_HANDLE) {
        if (debugMessenger != VK_NULL_HANDLE) {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
            debugMessenger = VK_NULL_HANDLE;
        }
        // Destroy Vulkan instance
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
    }
}

#endif