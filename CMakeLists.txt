cmake_minimum_required(VERSION 3.16...3.28)
project(C6GE)

# Set CMake policies
cmake_policy(SET CMP0079 NEW)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/C6GE/src)

# Source files
set(SOURCES
    ${CMAKE_SOURCE_DIR}/C6GE/src/Main.cpp
    ${CMAKE_SOURCE_DIR}/C6GE/src/Engine/Engine.cpp
    ${CMAKE_SOURCE_DIR}/C6GE/src/Logging/Log.cpp
    ${CMAKE_SOURCE_DIR}/C6GE/src/Render/Render.cpp
    ${CMAKE_SOURCE_DIR}/C6GE/src/Render/VertexLayouts.cpp
    ${CMAKE_SOURCE_DIR}/C6GE/src/Window/Window.cpp
    ${CMAKE_SOURCE_DIR}/C6GE/src/ECS/Object/Object.cpp
    ${CMAKE_SOURCE_DIR}/C6GE/src/Render/Shader/Shader.cpp
    ${CMAKE_SOURCE_DIR}/C6GE/src/Components/MeshComponent.cpp
    ${CMAKE_SOURCE_DIR}/C6GE/src/Components/ModelComponent.cpp
    ${CMAKE_SOURCE_DIR}/C6GE/src/Components/TransformComponent.cpp
    ${CMAKE_SOURCE_DIR}/C6GE/src/Components/CameraComponent.cpp
    ${CMAKE_SOURCE_DIR}/C6GE/src/Components/TextureComponent.cpp
    ${CMAKE_SOURCE_DIR}/C6GE/src/Render/Texture/Texture.cpp
    ${CMAKE_SOURCE_DIR}/C6GE/src/Input/Input.cpp
)

# Add macOS-specific source files
if(APPLE)
    list(APPEND SOURCES ${CMAKE_SOURCE_DIR}/C6GE/src/Window/Window_macOS.mm)
    list(APPEND SOURCES ${CMAKE_SOURCE_DIR}/C6GE/src/Render/Render_macOS.mm)
endif()

# Headers (optional, for IDEs)
set(HEADERS
    ${CMAKE_SOURCE_DIR}/C6GE/src/Engine/Engine.h
    ${CMAKE_SOURCE_DIR}/C6GE/src/Logging/Log.h
    ${CMAKE_SOURCE_DIR}/C6GE/src/Render/Render.h
    ${CMAKE_SOURCE_DIR}/C6GE/src/Window/Window.h
    ${CMAKE_SOURCE_DIR}/C6GE/src/ECS/Object/Object.h
    ${CMAKE_SOURCE_DIR}/C6GE/src/Render/Shader/Shader.h
    ${CMAKE_SOURCE_DIR}/C6GE/src/Render/Texture/Texture.h
    ${CMAKE_SOURCE_DIR}/C6GE/src/Components/MeshComponent.h
    ${CMAKE_SOURCE_DIR}/C6GE/src/Components/ShaderComponent.h
    ${CMAKE_SOURCE_DIR}/C6GE/src/Components/TextureComponent.h
    ${CMAKE_SOURCE_DIR}/C6GE/src/Components/CameraComponent.h
    ${CMAKE_SOURCE_DIR}/C6GE/src/Components/LightComponent.h
    ${CMAKE_SOURCE_DIR}/C6GE/src/Components/ModelComponent.h
    ${CMAKE_SOURCE_DIR}/C6GE/src/Time/DeltaTime.h
)

# Fetch dependencies
include(FetchContent)

# Fetch GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# Fetch EnTT
FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG v3.13.2
)
FetchContent_MakeAvailable(entt)

# Fetch GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)

# Fetch Assimp
FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG v5.4.3
)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(assimp)

# Fetch stb_image
FetchContent_Declare(
    stb
    URL https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
    DOWNLOAD_NO_EXTRACT TRUE
)
FetchContent_MakeAvailable(stb)

# Configure bgfx.cmake options before fetching
set(BGFX_BUILD_TOOLS ON CACHE BOOL "" FORCE)
set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BGFX_INSTALL OFF CACHE BOOL "" FORCE)
set(BGFX_AMALGAMATED OFF CACHE BOOL "" FORCE)
set(BGFX_LIBRARY_TYPE STATIC CACHE STRING "" FORCE)

# BGFX configuration for Apple Silicon
if(APPLE)
    set(BGFX_CONFIG_RENDERER_METAL ON CACHE BOOL "" FORCE)
    set(BGFX_CONFIG_RENDERER_OPENGL ON CACHE BOOL "" FORCE)
    set(BGFX_CONFIG_RENDERER_VULKAN OFF CACHE BOOL "" FORCE)
    set(BGFX_CONFIG_RENDERER_DIRECT3D9 OFF CACHE BOOL "" FORCE)
    set(BGFX_CONFIG_RENDERER_DIRECT3D11 OFF CACHE BOOL "" FORCE)
    set(BGFX_CONFIG_RENDERER_DIRECT3D12 OFF CACHE BOOL "" FORCE)
endif()

# Fetch and configure bgfx.cmake
FetchContent_Declare(
    bgfx_cmake
    GIT_REPOSITORY https://github.com/bkaradzic/bgfx.cmake.git
    GIT_TAG master
)

FetchContent_MakeAvailable(bgfx_cmake)

# Find OpenGL
find_package(OpenGL REQUIRED)

# Executable
add_executable(C6GE ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(C6GE PRIVATE 
    glfw 
    EnTT::EnTT 
    OpenGL::GL 
    assimp::assimp 
    bgfx
)

# If bgfx tools are built, add a custom command to compile shaders
find_program(BGFX_SHADERC_EXECUTABLE NAMES shaderc HINTS
    ${bgfx_cmake_SOURCE_DIR}/.build/win64_vs2022/bin
    ${bgfx_cmake_SOURCE_DIR}/.build/win64_vs2019/bin
    ${bgfx_cmake_BINARY_DIR}
    ${bgfx_cmake_BINARY_DIR}/bin
)

if(BGFX_SHADERC_EXECUTABLE)
    message(STATUS "Found bgfx shaderc: ${BGFX_SHADERC_EXECUTABLE}")
    add_custom_command(TARGET C6GE POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:C6GE>/Assets/shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/C6GE/src/Assets/shaders"
            "$<TARGET_FILE_DIR:C6GE>/Assets/shaders"
        # Compile vertex shader
        COMMAND ${BGFX_SHADERC_EXECUTABLE}
            --platform windows
            --type vertex
            --varyingdef "$<TARGET_FILE_DIR:C6GE>/Assets/shaders/varying.def.sc"
            --profile vs_5_0
            -f "$<TARGET_FILE_DIR:C6GE>/Assets/shaders/test.sc"
            -o "$<TARGET_FILE_DIR:C6GE>/Assets/shaders/test.vs.bin"
        # Compile fragment shader
        COMMAND ${BGFX_SHADERC_EXECUTABLE}
            --platform windows
            --type fragment
            --varyingdef "$<TARGET_FILE_DIR:C6GE>/Assets/shaders/varying.def.sc"
            --profile ps_5_0
            -f "$<TARGET_FILE_DIR:C6GE>/Assets/shaders/test_fs.sc"
            -o "$<TARGET_FILE_DIR:C6GE>/Assets/shaders/test.fs.bin"
        VERBATIM
    )
else()
    message(WARNING "bgfx shaderc not found; shaders will not be compiled.")
endif()

# Include directories for all dependencies
target_include_directories(C6GE PRIVATE 
    ${glm_SOURCE_DIR} 
    ${stb_SOURCE_DIR} 
    ${glfw_SOURCE_DIR}/include 
    ${assimp_SOURCE_DIR}/include 
    ${assimp_BINARY_DIR}
)

# For macOS, link necessary frameworks and add bgfx-specific settings
if(APPLE)
    # Find SQLite3 for macOS (required for BGFX on Apple Silicon)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SQLITE3 REQUIRED sqlite3)
    
    target_link_libraries(C6GE PRIVATE 
        "-framework Cocoa" 
        "-framework IOKit" 
        "-framework CoreFoundation" 
        "-framework Metal" 
        "-framework QuartzCore"
        ${SQLITE3_LIBRARIES}
    )
    target_compile_definitions(C6GE PRIVATE 
        GL_SILENCE_DEPRECATION
        BGFX_CONFIG_RENDERER_METAL=1
    )
    target_include_directories(C6GE PRIVATE ${SQLITE3_INCLUDE_DIRS})
endif()

# Copy Assets files to build directory after build
add_custom_command(TARGET C6GE POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:C6GE>/Assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/C6GE/src/Assets"
        "$<TARGET_FILE_DIR:C6GE>/Assets"
)