#include "d_engine.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <vk_initializers.h>
#include <vk_types.h>

#include "VkBootstrap.h"

#include <cassert>
#include <chrono>
#include <thread>
#include <cstdlib>

constexpr bool bUseValidationLayers = true;

DirectEngine* DirectEngine::s_current = nullptr;

DirectEngine& DirectEngine::Get() {
    assert(s_current && "DirectEngine::Get called with no current engine set");
    return *s_current;
}

DirectEngine* DirectEngine::GetCurrent() { return s_current; }

void DirectEngine::SetAsCurrent() { s_current = this; }

void DirectEngine::ClearIfCurrent() {
    if (s_current == this) {
        s_current = nullptr;
    }
}

static PFN_vkDestroyDebugUtilsMessengerEXT LoadDebugMessengerDestroyFn(VkInstance instance) {
    return reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
}

VkAllocationCallbacks* GetVulkanAllocator() {
    // Use default Vulkan allocation callbacks for now; custom allocator caused CRT heap issues in Debug.
    return nullptr;
}

void DirectEngine::Init() {
    // Allow multiple DirectEngine instances; this one becomes the active default.
    SetAsCurrent();

    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags window_flags = SDL_WINDOW_VULKAN;

    _window = SDL_CreateWindow(
        "DirectEngine",
        static_cast<int>(_windowExtent.width),
        static_cast<int>(_windowExtent.height),
        window_flags);

    InitVulkan();

    InitSwapchain();

    InitCommands();

    InitSyncStructures();

    _isInitialized = true;
}

void DirectEngine::Cleanup() {
    if (_isInitialized) {
        vkDeviceWaitIdle(_device);

        for (int i = 0; i < FRAME_OVERLAP; i++) {
            vkDestroyCommandPool(_device, _frames[i]._commandPool, GetVulkanAllocator());

		    //destroy sync objects
		    vkDestroyFence(_device, _frames[i]._renderFence, GetVulkanAllocator());
		    vkDestroySemaphore(_device ,_frames[i]._swapchainSemaphore, GetVulkanAllocator());
        }
        
        DestroySwapchain();

        vkDestroySurfaceKHR(_instance, _surface, GetVulkanAllocator());
        vkDestroyDevice(_device, GetVulkanAllocator());

        if (_debug_messenger != VK_NULL_HANDLE) {
            if (auto destroyDebugUtils = LoadDebugMessengerDestroyFn(_instance)) {
                destroyDebugUtils(_instance, _debug_messenger, GetVulkanAllocator());
            }
        }

        vkDestroyInstance(_instance, GetVulkanAllocator());
        SDL_DestroyWindow(_window);
        SDL_Quit();
    }

    ClearIfCurrent();
}

void DirectEngine::Draw() {
    VK_CHECK(vkWaitForFences(_device, 1, &GetCurrentFrame()._renderFence, true, 1000000000));
	VK_CHECK(vkResetFences(_device, 1, &GetCurrentFrame()._renderFence));

    uint32_t swapchainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain, 1000000000, GetCurrentFrame()._swapchainSemaphore, VK_NULL_HANDLE, &swapchainImageIndex));

	VkSemaphore renderFinishedSemaphore = _swapchainRenderSemaphores[swapchainImageIndex];

	VkCommandBuffer cmd = GetCurrentFrame()._mainCommandBuffer;

	VK_CHECK(vkResetCommandBuffer(cmd, 0));

	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	VkClearColorValue clearValue;
	float flash = std::abs(std::sin(_frameNumber / 120.f));
	clearValue = { { 0.0f, 0.0f, flash, 1.0f } };

	VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

	vkCmdClearColorImage(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

	vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex],VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	VK_CHECK(vkEndCommandBuffer(cmd));


	VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);	
	
	VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,GetCurrentFrame()._swapchainSemaphore);
    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, renderFinishedSemaphore);	
	
	VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo,&signalInfo,&waitInfo);	

	VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, GetCurrentFrame()._renderFence));

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.pSwapchains = &_swapchain;
	presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
	presentInfo.waitSemaphoreCount = 1;

	presentInfo.pImageIndices = &swapchainImageIndex;

	VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

	_frameNumber++;
}

void DirectEngine::Run() {
    SDL_Event e;
    bool bQuit = false;

    while (!bQuit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                bQuit = true;
            }

            if (e.type == SDL_EVENT_WINDOW_MINIMIZED) {
                stop_rendering = true;
            }

            if (e.type == SDL_EVENT_WINDOW_RESTORED) {
                stop_rendering = false;
            }
        }

        if (stop_rendering) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        Draw();
    }
}

void DirectEngine::InitVulkan() {
    vkb::InstanceBuilder builder;

    auto inst_ret = builder.set_app_name("DirectEngine")
                            .request_validation_layers(bUseValidationLayers)
                            .use_default_debug_messenger()
                            .require_api_version(1, 3, 0)
                            .build();

    if (!inst_ret) {
        fmt::print("Failed to create Vulkan instance: {}\n", inst_ret.error().message());
        std::abort();
    }

    vkb::Instance vkb_inst = inst_ret.value();

    _instance = vkb_inst.instance;
    _debug_messenger = vkb_inst.debug_messenger;

    SDL_Vulkan_CreateSurface(_window, _instance, GetVulkanAllocator(), &_surface);

    VkPhysicalDeviceVulkan13Features features{  .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    features.dynamicRendering = VK_TRUE;
    features.synchronization2 = VK_TRUE;

    VkPhysicalDeviceVulkan12Features features12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    features12.bufferDeviceAddress = VK_TRUE;
    features12.descriptorIndexing = VK_TRUE;

    vkb::PhysicalDeviceSelector selector{ vkb_inst };
    vkb::PhysicalDevice physicalDevice = selector
        .set_minimum_version(1, 3)
        .set_required_features_13(features)
        .set_required_features_12(features12)
        .set_surface(_surface)
        .select()
        .value();

    if (!physicalDevice) {
        fmt::print("Failed to find a suitable GPU!\n");
        std::abort();
    }

    vkb::DeviceBuilder deviceBuilder{ physicalDevice };

    vkb::Device vkb_device = deviceBuilder.build().value();

    _device = vkb_device.device;
    _physicalDevice = physicalDevice.physical_device;

    _graphicsQueue = vkb_device.get_queue(vkb::QueueType::graphics).value();
    _graphicsQueueFamily = vkb_device.get_queue_index(vkb::QueueType::graphics).value();
}

void DirectEngine::CreateSwapchain(uint32_t width, uint32_t height) {
    vkb::SwapchainBuilder swapchainBuilder{_physicalDevice, _device, _surface};

    // Prefer B8G8R8A8_UNORM but allow fallback if unsupported on the surface.
    swapchainBuilder
        .set_desired_format(VkSurfaceFormatKHR{ .format = VK_FORMAT_B8G8R8A8_UNORM, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
        .add_fallback_format(VkSurfaceFormatKHR{ .format = VK_FORMAT_B8G8R8A8_SRGB, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    auto swapchain_ret = swapchainBuilder.build();
    if (!swapchain_ret) {
        fmt::print("Failed to create swapchain: {}\n", swapchain_ret.error().message());
        std::abort();
    }

    vkb::Swapchain vkb_swapchain = swapchain_ret.value();

    _swapchainImageFormat = vkb_swapchain.image_format;
    _swapchainExtent = vkb_swapchain.extent;
    _swapchain = vkb_swapchain.swapchain;

    auto images_ret = vkb_swapchain.get_images();
    if (!images_ret) {
        fmt::print("Failed to get swapchain images: {}\n", images_ret.error().message());
        std::abort();
    }
    _swapchainImages = images_ret.value();

    auto image_views_ret = vkb_swapchain.get_image_views();
    if (!image_views_ret) {
        fmt::print("Failed to create swapchain image views: {}\n", image_views_ret.error().message());
        std::abort();
    }
    _swapchainImageViews = image_views_ret.value();
}

void DirectEngine::InitSwapchain() {
    CreateSwapchain(_windowExtent.width, _windowExtent.height);
}


void DirectEngine::DestroySwapchain() {
    for (VkSemaphore sem : _swapchainRenderSemaphores) {
        if (sem != VK_NULL_HANDLE) {
            vkDestroySemaphore(_device, sem, GetVulkanAllocator());
        }
    }
    _swapchainRenderSemaphores.clear();

    for (int i = 0; i < _swapchainImageViews.size(); i++) {
        vkDestroyImageView(_device, _swapchainImageViews[i], GetVulkanAllocator());
    }

    vkDestroySwapchainKHR(_device, _swapchain, GetVulkanAllocator());
}

void DirectEngine::InitCommands() {
	VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	for (int i = 0; i < FRAME_OVERLAP; i++) {

		VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i]._commandPool));

		// allocate the default command buffer that we will use for rendering
		VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);

		VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer));
	}
}

void DirectEngine::InitSyncStructures() {
    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info(0);

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, GetVulkanAllocator(), &_frames[i]._renderFence));

		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, GetVulkanAllocator(), &_frames[i]._swapchainSemaphore));
	}

    _swapchainRenderSemaphores.resize(_swapchainImages.size());
    for (size_t i = 0; i < _swapchainRenderSemaphores.size(); i++) {
        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, GetVulkanAllocator(), &_swapchainRenderSemaphores[i]));
    }
}