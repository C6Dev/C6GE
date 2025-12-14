#pragma once

#include "vk_types.h"

struct FrameData {
    VkCommandPool _commandPool;
    VkCommandBuffer _mainCommandBuffer;

    VkSemaphore _swapchainSemaphore;
	VkFence _renderFence;
};

constexpr unsigned int FRAME_OVERLAP = 2;

class DirectEngine {
    public:
    bool _isInitialized { false };
    int _frameNumber {0};
    bool stop_rendering { false };
    VkExtent2D _windowExtent { 1700 , 900 };

    struct SDL_Window* _window { nullptr };

    VkInstance _instance { VK_NULL_HANDLE };
    VkDebugUtilsMessengerEXT _debug_messenger { VK_NULL_HANDLE };
    VkPhysicalDevice _physicalDevice { VK_NULL_HANDLE };
    VkDevice _device { VK_NULL_HANDLE };
    VkSurfaceKHR _surface { VK_NULL_HANDLE };

    VkSwapchainKHR _swapchain;
    VkFormat _swapchainImageFormat;

    std::vector<VkImage> _swapchainImages;
    std::vector<VkImageView> _swapchainImageViews;
    VkExtent2D _swapchainExtent;

    std::vector<VkSemaphore> _swapchainRenderSemaphores;

    FrameData _frames[FRAME_OVERLAP];

    FrameData& GetCurrentFrame() { return _frames[_frameNumber % FRAME_OVERLAP]; };

    VkQueue _graphicsQueue;
    uint32_t _graphicsQueueFamily;

    // Returns the engine registered as current. Prefer passing explicit pointers where possible.
    static DirectEngine& Get();
    static DirectEngine* GetCurrent();
    void SetAsCurrent();
    void ClearIfCurrent();

    // Initialize the engine
    void Init();

    // Cleanup the engine
    void Cleanup();

    // Draw a frame
    void Draw();

    // Run the main loop
    void Run();

    private:
    static DirectEngine* s_current;

    void InitVulkan();
    void CreateSwapchain(uint32_t width, uint32_t height);
    void InitSwapchain();
    void DestroySwapchain();
    void InitCommands();
    void InitSyncStructures();
};