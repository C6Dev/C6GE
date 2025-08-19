cmake_minimum_required(VERSION 3.10...3.28)
project(C6GE)
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
    ${CMAKE_SOURCE_DIR}/C6GE/src/Window/Window.cpp
    ${CMAKE_SOURCE_DIR}/C6GE/src/ECS/Object/Object.cpp
    ${CMAKE_SOURCE_DIR}/C6GE/src/Render/Shader/Shader.cpp
    ${CMAKE_SOURCE_DIR}/C6GE/src/Components/MeshComponent.cpp
    ${CMAKE_SOURCE_DIR}/C6GE/src/Components/CameraComponent.cpp
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

# Fetch GLAD
FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v0.1.36
)
FetchContent_MakeAvailable(glad)

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

# Fetch bx (bgfx dependency)
FetchContent_Declare(
    bx
    GIT_REPOSITORY https://github.com/bkaradzic/bx.git
    GIT_TAG master
)
FetchContent_MakeAvailable(bx)

# Fetch bimg (bgfx dependency)
FetchContent_Declare(
    bimg
    GIT_REPOSITORY https://github.com/bkaradzic/bimg.git
    GIT_TAG master
)
FetchContent_MakeAvailable(bimg)

# Fetch bgfx itself
FetchContent_Declare(
    bgfx
    GIT_REPOSITORY https://github.com/bkaradzic/bgfx.cmake.git
    GIT_TAG master
)
set(BGFX_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
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

FetchContent_MakeAvailable(bgfx)

# Find OpenGL
find_package(OpenGL REQUIRED)

# Executable
add_executable(C6GE ${SOURCES} ${HEADERS})

# Ensure bgfx is built before linking
add_dependencies(C6GE bgfx bx bimg)

target_link_libraries(C6GE PRIVATE 
    glfw 
    glad 
    EnTT::EnTT 
    OpenGL::GL 
    assimp::assimp 
    bgfx 
    bx 
    bimg
)

# Include directories for all dependencies
target_include_directories(C6GE PRIVATE 
    ${glad_BINARY_DIR}/include 
    ${glm_SOURCE_DIR} 
    ${stb_SOURCE_DIR} 
    ${glfw_SOURCE_DIR}/include 
    ${assimp_SOURCE_DIR}/include 
    ${assimp_BINARY_DIR}
    ${bgfx_SOURCE_DIR}/include
    ${bx_SOURCE_DIR}/include
    ${bimg_SOURCE_DIR}/include
    ${bgfx_SOURCE_DIR}/3rdparty
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