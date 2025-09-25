cmake_minimum_required(VERSION 3.19)
project(C6GE)

# Stop macOS builds
if(APPLE)
    message(FATAL_ERROR "C6GE: Vulkan backend on macOS is currently broken. Build aborted.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable DiligentTools
set(DILIGENT_TOOLS ON CACHE BOOL "Enable DiligentTools" FORCE)

# Add DiligentEngine
add_subdirectory(external/DiligentEngine)

# Detect platforms
if(WIN32)
    set(PLATFORM_WIN32 TRUE)
elseif(UNIX)
    set(PLATFORM_LINUX TRUE)
endif()

# Executable
add_executable(C6GE
    src/main.cpp
)

# Include directories
target_include_directories(C6GE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/ImGui
    ${CMAKE_CURRENT_SOURCE_DIR}/external/ImGui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine/DiligentTools/NativeApp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine/DiligentSamples/SampleBase/include
)

# SampleBase sources
target_sources(C6GE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine/DiligentSamples/SampleBase/src/SampleBase.cpp
)

# Link libraries
set(DILIGENT_LIBS
    Diligent-BuildSettings
    Diligent-Primitives
    Diligent-BasicPlatform
    Diligent-GraphicsAccessories
    Diligent-GraphicsEngine
    Diligent-GraphicsTools
    Diligent-HLSL2GLSLConverterLib
    Diligent-ShaderTools
    Diligent-Imgui
    glfw
)

if(PLATFORM_WIN32)
    list(APPEND DILIGENT_LIBS Diligent-GraphicsEngineD3D12)
elseif(PLATFORM_LINUX)
    list(APPEND DILIGENT_LIBS Diligent-GraphicsEngineVk-static)
endif()

target_link_libraries(C6GE PRIVATE ${DILIGENT_LIBS})
