cmake_minimum_required(VERSION 3.19)
project(C6GE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable DiligentTools and Samples
set(DILIGENT_TOOLS ON CACHE BOOL "Enable DiligentTools" FORCE)
set(DILIGENT_BUILD_SAMPLES ON CACHE BOOL "Enable Diligent Samples" FORCE)
set(DILIGENT_BUILD_NATIVE_APP ON CACHE BOOL "Enable Diligent NativeApp" FORCE)
set(DILIGENT_MACOS_PLATFORM ON CACHE BOOL "Enable macOS platform" FORCE)
set(DILIGENT_METAL_SUPPORTED ON CACHE BOOL "Enable Metal backend" FORCE)
set(DILIGENT_VULKAN_SUPPORTED ON CACHE BOOL "Enable Vulkan backend" FORCE)
set(DILIGENT_OPENGL_SUPPORTED OFF CACHE BOOL "Disable OpenGL backend" FORCE)
set(DILIGENT_D3D11_SUPPORTED OFF CACHE BOOL "Disable Direct3D11 backend" FORCE)
set(DILIGENT_D3D12_SUPPORTED OFF CACHE BOOL "Disable Direct3D12 backend" FORCE)

# DiligentEngine will now use our docking ImGui via symlink

# Disable Wayland support to avoid missing header issues
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "Build support for Wayland" FORCE)
set(GLFW_BUILD_X11 ON CACHE BOOL "Build support for X11" FORCE)
add_definitions(-D_GLFW_X11)

# Add DiligentEngine
add_subdirectory(external/DiligentEngine)

# Detect platforms
if(WIN32)
    set(PLATFORM_WIN32 TRUE)
elseif(UNIX AND NOT APPLE)
    set(PLATFORM_LINUX TRUE)
elseif(UNIX AND APPLE)
    set(PLATFORM_MACOS TRUE)
endif()

# Platform-specific definitions
if(PLATFORM_MACOS)
    add_definitions(-DPLATFORM_MACOS -DGLFW_EXPOSE_NATIVE_COCOA)
    add_definitions(-U_WIN32 -UGLFW_EXPOSE_NATIVE_WIN32)
    find_library(COCOA_FRAMEWORK Cocoa)
    find_library(IOKIT_FRAMEWORK IOKit)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
    set(PLATFORM_LIBRARIES ${COCOA_FRAMEWORK} ${IOKIT_FRAMEWORK} ${COREFOUNDATION_FRAMEWORK})
elseif(PLATFORM_WIN32)
    add_definitions(-DPLATFORM_WIN32 -DGLFW_EXPOSE_NATIVE_WIN32)
elseif(PLATFORM_LINUX)
    add_definitions(-DPLATFORM_LINUX -DGLFW_EXPOSE_NATIVE_X11)
endif()

# Add Objective-C++ flags for macOS
if(PLATFORM_MACOS)
    set_source_files_properties(src/main.cpp PROPERTIES COMPILE_FLAGS "-x objective-c++")
endif()

if(PLATFORM_MACOS)
    set_source_files_properties(src/Runtime/Runtime.cpp PROPERTIES COMPILE_FLAGS "-x objective-c++")
endif()

# Executable
add_executable(C6GE
    src/main.cpp
    src/Render/Render.cpp
    external/DiligentEngine/DiligentSamples/Tutorials/Common/src/TexturedCube.cpp
)

# Include directories
target_include_directories(C6GE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine/DiligentTools/NativeApp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine/DiligentSamples/ThirdParty/glfw/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine/DiligentSamples/SampleBase/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine/DiligentSamples/Tutorials/Common/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine/DiligentTools/Imgui/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external
)

add_executable(Runtime
    src/Runtime/Runtime.cpp
    src/Render/Render.cpp
    external/DiligentEngine/DiligentSamples/Tutorials/Common/src/TexturedCube.cpp
)

# Include directories
target_include_directories(Runtime PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine/DiligentTools/NativeApp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine/DiligentSamples/ThirdParty/glfw/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine/DiligentSamples/SampleBase/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine/DiligentSamples/Tutorials/Common/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine/DiligentTools/Imgui/src
)

# Link libraries
set(DILIGENT_LIBS
    Diligent-GraphicsEngine
    DiligentFX
    Diligent-Imgui
    Diligent-SampleBase
    Diligent-RenderStateNotation
    glfw
)

if(PLATFORM_MACOS)
    list(APPEND DILIGENT_LIBS Diligent-ApplePlatform)
elseif(PLATFORM_WIN32)
    list(APPEND DILIGENT_LIBS Diligent-Win32Platform)
elseif(PLATFORM_LINUX)
    list(APPEND DILIGENT_LIBS Diligent-LinuxPlatform)
endif()

target_link_libraries(C6GE PRIVATE ${DILIGENT_LIBS} ${PLATFORM_LIBRARIES})

target_link_libraries(Runtime PRIVATE ${DILIGENT_LIBS} ${PLATFORM_LIBRARIES})

# Add library directories
target_link_directories(C6GE PRIVATE
    ${CMAKE_BINARY_DIR}/external/DiligentEngine/DiligentTools/NativeApp/lib
    ${CMAKE_BINARY_DIR}/external/DiligentEngine/DiligentSamples/SampleBase/lib
    ${CMAKE_BINARY_DIR}/external/DiligentEngine/DiligentCore/Platforms/Apple
)

target_link_directories(Runtime PRIVATE
    ${CMAKE_BINARY_DIR}/external/DiligentEngine/DiligentTools/NativeApp/lib
    ${CMAKE_BINARY_DIR}/external/DiligentEngine/DiligentSamples/SampleBase/lib
    ${CMAKE_BINARY_DIR}/external/DiligentEngine/DiligentCore/Platforms/Apple
)

# Copy assets folder contents into exe dir
set(ASSETS_DIR "${CMAKE_SOURCE_DIR}/src/Assets")
add_custom_command(
    TARGET C6GE POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${ASSETS_DIR}"
            "$<TARGET_FILE_DIR:C6GE>"
)

# Copy Diligent backend DLLs into exe dir
set(DILIGENT_DLL_TARGETS
    Diligent-GraphicsEngineVk-shared
    Diligent-GraphicsEngineGL-shared
    Diligent-GraphicsEngineGLES-shared
)

if(PLATFORM_MACOS)
    list(APPEND DILIGENT_DLL_TARGETS Diligent-GraphicsEngineMtl-shared)
elseif(PLATFORM_WIN32)
    list(APPEND DILIGENT_DLL_TARGETS
        Diligent-GraphicsEngineD3D11-shared
        Diligent-GraphicsEngineD3D12-shared
    )
endif()

foreach(DLL_TARGET IN LISTS DILIGENT_DLL_TARGETS)
    if(TARGET ${DLL_TARGET})
        add_custom_command(
            TARGET C6GE POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:${DLL_TARGET}>"
                    "$<TARGET_FILE_DIR:C6GE>"
        )
    endif()
endforeach()

foreach(DLL_TARGET IN LISTS DILIGENT_DLL_TARGETS)
    if(TARGET ${DLL_TARGET})
        add_custom_command(
            TARGET Runtime POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:${DLL_TARGET}>"
                    "$<TARGET_FILE_DIR:Runtime>"
        )
    endif()
endforeach()

if(PLATFORM_WIN32)
copy_required_dlls(C6GE DXC_REQUIRED YES)
endif()

if(PLATFORM_WIN32)
copy_required_dlls(Runtime DXC_REQUIRED YES)
endif()