cmake_minimum_required(VERSION 3.19)
project(C6GE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

## Render backend selection
# Introduce AUTO mode that enables a set of backends per-OS; runtime picks the best available.
set(RENDER_BACKEND "auto" CACHE STRING "Render backend: auto|metal|vulkan|opengl|d3d11|d3d12")
set_property(CACHE RENDER_BACKEND PROPERTY STRINGS auto metal vulkan opengl d3d11 d3d12)

# Normalize user input for comparisons but do not overwrite cache entry
set(_RENDER_BACKEND "${RENDER_BACKEND}")
string(TOLOWER "${_RENDER_BACKEND}" _RENDER_BACKEND)

# If an incompatible backend was cached from a previous configure, fall back to auto.
if((WIN32 OR UNIX) AND NOT APPLE)
    if(_RENDER_BACKEND STREQUAL "metal")
        message(WARNING "Requested backend 'metal' is not available on this platform; falling back to auto selection.")
        set(_RENDER_BACKEND "auto")
    endif()
endif()

if(_RENDER_BACKEND STREQUAL "auto")
    if(WIN32)
        message(STATUS "Auto-select: enabling D3D12, D3D11, Vulkan, OpenGL on Windows")
        set(DILIGENT_NO_DIRECT3D12 OFF CACHE BOOL "Disable Direct3D12 backend" FORCE)
        set(DILIGENT_NO_DIRECT3D11 OFF CACHE BOOL "Disable Direct3D11 backend" FORCE)
        set(DILIGENT_NO_VULKAN      OFF CACHE BOOL "Disable Vulkan backend"      FORCE)
        set(DILIGENT_NO_OPENGL      OFF CACHE BOOL "Disable OpenGL/GLES backend"  FORCE)
        set(DILIGENT_NO_METAL        ON CACHE BOOL "Disable Metal backend"        FORCE)
    elseif(APPLE)
        message(STATUS "Auto-select: enabling Metal (and allowing Vulkan/OpenGL if supported) on macOS")
        set(DILIGENT_NO_METAL       OFF CACHE BOOL "Disable Metal backend"        FORCE)
        set(DILIGENT_NO_VULKAN      OFF CACHE BOOL "Disable Vulkan backend"       FORCE)
        set(DILIGENT_NO_OPENGL      OFF CACHE BOOL "Disable OpenGL/GLES backend"  FORCE)
        set(DILIGENT_NO_DIRECT3D11   ON CACHE BOOL "Disable Direct3D11 backend"   FORCE)
        set(DILIGENT_NO_DIRECT3D12   ON CACHE BOOL "Disable Direct3D12 backend"   FORCE)
    elseif(UNIX)
        message(STATUS "Auto-select: enabling Vulkan and OpenGL on Linux")
        set(DILIGENT_NO_VULKAN      OFF CACHE BOOL "Disable Vulkan backend"       FORCE)
        set(DILIGENT_NO_OPENGL      OFF CACHE BOOL "Disable OpenGL/GLES backend"  FORCE)
        set(DILIGENT_NO_DIRECT3D11   ON CACHE BOOL "Disable Direct3D11 backend"   FORCE)
        set(DILIGENT_NO_DIRECT3D12   ON CACHE BOOL "Disable Direct3D12 backend"   FORCE)
        set(DILIGENT_NO_METAL        ON CACHE BOOL "Disable Metal backend"        FORCE)
    endif()
elseif(_RENDER_BACKEND STREQUAL "metal")
    message(STATUS "Configuring Diligent Engine: Metal only")
    set(DILIGENT_NO_METAL       OFF CACHE BOOL "Disable Metal backend"        FORCE)
    set(DILIGENT_NO_VULKAN       ON CACHE BOOL "Disable Vulkan backend"       FORCE)
    set(DILIGENT_NO_OPENGL       ON CACHE BOOL "Disable OpenGL/GLES backend"  FORCE)
    set(DILIGENT_NO_DIRECT3D11   ON CACHE BOOL "Disable Direct3D11 backend"   FORCE)
    set(DILIGENT_NO_DIRECT3D12   ON CACHE BOOL "Disable Direct3D12 backend"   FORCE)
elseif(_RENDER_BACKEND STREQUAL "vulkan")
    message(STATUS "Configuring Diligent Engine: Vulkan only")
    set(DILIGENT_NO_VULKAN      OFF CACHE BOOL "Disable Vulkan backend"       FORCE)
    set(DILIGENT_NO_METAL        ON CACHE BOOL "Disable Metal backend"        FORCE)
    set(DILIGENT_NO_OPENGL       ON CACHE BOOL "Disable OpenGL/GLES backend"  FORCE)
    set(DILIGENT_NO_DIRECT3D11   ON CACHE BOOL "Disable Direct3D11 backend"   FORCE)
    set(DILIGENT_NO_DIRECT3D12   ON CACHE BOOL "Disable Direct3D12 backend"   FORCE)
elseif(_RENDER_BACKEND STREQUAL "opengl")
    message(STATUS "Configuring Diligent Engine: OpenGL only")
    set(DILIGENT_NO_OPENGL      OFF CACHE BOOL "Disable OpenGL/GLES backend"  FORCE)
    set(DILIGENT_NO_METAL        ON CACHE BOOL "Disable Metal backend"        FORCE)
    set(DILIGENT_NO_VULKAN       ON CACHE BOOL "Disable Vulkan backend"       FORCE)
    set(DILIGENT_NO_DIRECT3D11   ON CACHE BOOL "Disable Direct3D11 backend"   FORCE)
    set(DILIGENT_NO_DIRECT3D12   ON CACHE BOOL "Disable Direct3D12 backend"   FORCE)
elseif(_RENDER_BACKEND STREQUAL "d3d11")
    message(STATUS "Configuring Diligent Engine: Direct3D11 only")
    set(DILIGENT_NO_DIRECT3D11  OFF CACHE BOOL "Disable Direct3D11 backend"   FORCE)
    set(DILIGENT_NO_DIRECT3D12   ON CACHE BOOL "Disable Direct3D12 backend"   FORCE)
    set(DILIGENT_NO_METAL        ON CACHE BOOL "Disable Metal backend"        FORCE)
    set(DILIGENT_NO_VULKAN       ON CACHE BOOL "Disable Vulkan backend"       FORCE)
    set(DILIGENT_NO_OPENGL       ON CACHE BOOL "Disable OpenGL/GLES backend"  FORCE)
elseif(_RENDER_BACKEND STREQUAL "d3d12")
    message(STATUS "Configuring Diligent Engine: Direct3D12 only")
    set(DILIGENT_NO_DIRECT3D12  OFF CACHE BOOL "Disable Direct3D12 backend"   FORCE)
    set(DILIGENT_NO_DIRECT3D11   ON CACHE BOOL "Disable Direct3D11 backend"   FORCE)
    set(DILIGENT_NO_METAL        ON CACHE BOOL "Disable Metal backend"        FORCE)
    set(DILIGENT_NO_VULKAN       ON CACHE BOOL "Disable Vulkan backend"       FORCE)
    set(DILIGENT_NO_OPENGL       ON CACHE BOOL "Disable OpenGL/GLES backend"  FORCE)
else()
    message(FATAL_ERROR "Unknown backend '${RENDER_BACKEND}'. Valid options are: auto, metal, vulkan, opengl, d3d11, d3d12")
endif()


# Enable DiligentTools and Samples
set(DILIGENT_TOOLS ON CACHE BOOL "Enable DiligentTools" FORCE)
set(DILIGENT_BUILD_SAMPLES ON CACHE BOOL "Enable Diligent Samples" FORCE)
set(DILIGENT_BUILD_NATIVE_APP ON CACHE BOOL "Enable Diligent NativeApp" FORCE)
if(APPLE)
    set(DILIGENT_MACOS_PLATFORM ON CACHE BOOL "Enable macOS platform" FORCE)
endif()



# No extra Apple-specific defaults: AUTO mode above already configures sensible choices per-OS.

# DiligentEngine will now use our docking ImGui via symlink

# Disable Wayland support to avoid missing header issues
if (UNIX AND NOT APPLE)
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "Build support for Wayland" FORCE)
set(GLFW_BUILD_X11 ON CACHE BOOL "Build support for X11" FORCE)
add_definitions(-D_GLFW_X11)
endif()


# Only build SampleBase from DiligentSamples, skip Samples and Tutorials
set(DILIGENT_BUILD_SAMPLE_BASE_ONLY ON CACHE BOOL "Build only SampleBase project" FORCE)
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
    src/Render/Render.Core.cpp
    src/Render/Render.Pipeline.cpp
    src/Render/Render.Framebuffer.cpp
    src/Render/Render.RayTracing.cpp
    src/Render/Render.cpp
    src/Render/Render.UI.cpp
    src/Render/Systems/RenderSystem.cpp
    src/Project/Project.cpp
    src/Platform/FileDialog.cpp
    external/DiligentEngine/DiligentSamples/Tutorials/Common/src/TexturedCube.cpp
    src/EditorTheme/EditorTheme.cpp
    # ImGui platform backend for GLFW (required for multi-viewport platform windows)
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends/imgui_impl_glfw.cpp
    # ImGuizmo manipulator (source file)
    ${CMAKE_CURRENT_SOURCE_DIR}/external/ImGuizmo/ImGuizmo.cpp
)

# Include directories
target_include_directories(C6GE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/entt/single_include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine/DiligentTools/NativeApp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine/DiligentSamples/ThirdParty/glfw/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine/DiligentSamples/SampleBase/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine/DiligentSamples/Tutorials/Common/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine/DiligentTools/Imgui/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine/DiligentTools/ThirdParty/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/external/ImGuizmo
    
)

add_executable(Runtime
    src/Runtime/Runtime.cpp
    src/Render/Render.Core.cpp
    src/Render/Render.Pipeline.cpp
    src/Render/Render.Framebuffer.cpp
    src/Render/Render.RayTracing.cpp
    src/Render/Render.cpp
    src/Render/Render.UI.cpp
    src/Render/Systems/RenderSystem.cpp
    src/Project/Project.cpp
    src/Platform/FileDialog.cpp
    external/DiligentEngine/DiligentSamples/Tutorials/Common/src/TexturedCube.cpp
    src/EditorTheme/EditorTheme.cpp
    # ImGui platform backend for GLFW (required for multi-viewport platform windows)
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends/imgui_impl_glfw.cpp
    # ImGuizmo manipulator (source file)
    ${CMAKE_CURRENT_SOURCE_DIR}/external/ImGuizmo/ImGuizmo.cpp
)

# Include directories
target_include_directories(Runtime PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/entt/single_include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine/DiligentTools/NativeApp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine/DiligentSamples/ThirdParty/glfw/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine/DiligentSamples/SampleBase/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine/DiligentSamples/Tutorials/Common/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine/DiligentTools/Imgui/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine/DiligentTools/ThirdParty/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/external/ImGuizmo
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

# set src/assets/C6GELogo.ico as application icon on Windows for C6GE
if(PLATFORM_WIN32)
    set(C6GE_ICON_RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/app_icon.rc")
    target_sources(C6GE PRIVATE ${C6GE_ICON_RESOURCE})
endif()

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

# Copy runtime asset folders next to executables
set(ASSETS_DIR           "${CMAKE_SOURCE_DIR}/src/Assets")
set(SHADER_ASSETS_DIR    "${CMAKE_SOURCE_DIR}/assets")

add_custom_command(
    TARGET C6GE POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${ASSETS_DIR}"
        "$<TARGET_FILE_DIR:C6GE>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${SHADER_ASSETS_DIR}"
        "$<TARGET_FILE_DIR:C6GE>/assets"
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

# Also stage assets for Runtime target
add_custom_command(
    TARGET Runtime POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${ASSETS_DIR}"
        "$<TARGET_FILE_DIR:Runtime>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${SHADER_ASSETS_DIR}"
        "$<TARGET_FILE_DIR:Runtime>/assets"
)

if(PLATFORM_WIN32)
copy_required_dlls(C6GE DXC_REQUIRED YES)
endif()

if(PLATFORM_WIN32)
copy_required_dlls(Runtime DXC_REQUIRED YES)
endif()