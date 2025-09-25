cmake_minimum_required(VERSION 3.19)
project(C6GE)

# Stop macOS builds
if(APPLE)
    message(FATAL_ERROR "C6GE: Vulkan backend on macOS is currently broken. Build aborted.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable DiligentTools and Samples
set(DILIGENT_TOOLS ON CACHE BOOL "Enable DiligentTools" FORCE)
set(DILIGENT_BUILD_SAMPLES ON CACHE BOOL "Enable Diligent Samples" FORCE)
set(DILIGENT_BUILD_NATIVE_APP ON CACHE BOOL "Enable Diligent NativeApp" FORCE)

# Add DiligentEngine
add_subdirectory(external/DiligentEngine)

# Detect platforms
if(WIN32)
    set(PLATFORM_WIN32 TRUE)
elseif(UNIX AND NOT APPLE)
    set(PLATFORM_LINUX TRUE)
endif()

# Executable
add_executable(C6GE
    src/main.cpp
)

# Include directories
target_include_directories(C6GE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine/DiligentTools/NativeApp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine/DiligentSamples/ThirdParty/glfw/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine/DiligentSamples/SampleBase/include
)

# Link libraries
set(DILIGENT_LIBS
    Diligent-BuildSettings
    Diligent-Primitives
    Diligent-BasicPlatform
    Diligent-Win32Platform
    Diligent-GraphicsAccessories
    Diligent-GraphicsEngine
    Diligent-GraphicsEngineD3DBase
    Diligent-GraphicsEngineD3D12-static
    Diligent-GraphicsTools
    Diligent-HLSL2GLSLConverterLib
    Diligent-ShaderTools
    Diligent-Imgui          # Use Diligent's built-in ImGui
    Diligent-SampleBase
    glfw
)

target_link_libraries(C6GE PRIVATE ${DILIGENT_LIBS})

# Add library directories if needed
target_link_directories(C6GE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine/build/DiligentTools/NativeApp/lib/Debug
    ${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine/build/DiligentSamples/SampleBase/lib/Debug
)

# Ensure GLFW native headers are available
target_compile_definitions(C6GE PRIVATE
    GLFW_EXPOSE_NATIVE_WIN32
)
