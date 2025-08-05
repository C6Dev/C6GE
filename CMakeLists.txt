cmake_minimum_required(VERSION 3.10)

project(C6GE)

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

# Fetch stb_image
FetchContent_Declare(
    stb
    URL https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
    DOWNLOAD_NO_EXTRACT TRUE
)
FetchContent_MakeAvailable(stb)

# Find OpenGL
find_package(OpenGL REQUIRED)

# Executable
add_executable(C6GE ${SOURCES} ${HEADERS})

target_link_libraries(C6GE PRIVATE glfw glad EnTT::EnTT OpenGL::GL)
target_include_directories(C6GE PRIVATE ${glad_SOURCE_DIR}/include ${glm_SOURCE_DIR} ${stb_SOURCE_DIR})

# For macOS, link necessary frameworks
if(APPLE)
    target_link_libraries(C6GE PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreFoundation")
endif()

# Copy shader files to build directory after build
add_custom_command(TARGET C6GE POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:C6GE>/shader"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/C6GE/src/Render/ShaderFiles"
        "$<TARGET_FILE_DIR:C6GE>/shader"
)

add_custom_command(TARGET C6GE POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:C6GE>/texture"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/C6GE/src/Render/TextureFiles"
        "$<TARGET_FILE_DIR:C6GE>/texture"
)