cmake_minimum_required(VERSION 3.16)

find_package(Vulkan REQUIRED)

# Build bundled SDL3 so consumers can link to SDL3::SDL3 without relying on a system install.
add_subdirectory(SDL)

# Use upstream CMake builds for bundled dependencies to stay aligned with their targets.
add_subdirectory(vk-bootstrap)
add_subdirectory(fastgltf)

# Build fmt without extras; provides fmt::fmt for consumers
set(FMT_DOC OFF CACHE BOOL "" FORCE)
set(FMT_TEST OFF CACHE BOOL "" FORCE)
set(FMT_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(fmt)

add_library(GLM INTERFACE)
target_include_directories(GLM INTERFACE GLM)

add_library(VulkanMemoryAllocator INTERFACE)
target_include_directories(VulkanMemoryAllocator INTERFACE VulkanMemoryAllocator/include)

add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE stb)

add_library(imgui STATIC)
target_sources(imgui PRIVATE
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_vulkan.cpp
    imgui/backends/imgui_impl_sdl3.cpp   # <-- SDL3 backend!
)
target_include_directories(imgui PUBLIC
    imgui
    imgui/backends
)
target_link_libraries(imgui PUBLIC Vulkan::Vulkan SDL3::SDL3)