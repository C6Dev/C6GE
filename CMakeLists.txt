
cmake_minimum_required(VERSION 3.21)
project(C6GE)


add_subdirectory(external/bgfx.cmake ${CMAKE_BINARY_DIR}/bgfx.cmake-build)
include(${CMAKE_CURRENT_SOURCE_DIR}/external/bgfx.cmake/cmake/bgfxToolUtils.cmake)

# Add bgfx shader build integration
set(SHADER_DIR ${CMAKE_SOURCE_DIR}/src)
set(SHADER_OUT_DIR ${CMAKE_BINARY_DIR}/generated/shaders)
set(BGFX_SHADER_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/external/bgfx.cmake/bgfx/examples/common
    ${CMAKE_SOURCE_DIR}/external/bgfx.cmake/bgfx/src
)

bgfx_compile_shaders(
  TYPE VERTEX
  SHADERS ${SHADER_DIR}/vs_cubes.sc
  VARYING_DEF ${SHADER_DIR}/varying.def.sc
  OUTPUT_DIR ${SHADER_OUT_DIR}
  AS_HEADERS
  DEPENDS shaderc
  LANG spirv
  INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_DIRS}
)
bgfx_compile_shaders(
  TYPE FRAGMENT
  SHADERS ${SHADER_DIR}/fs_cubes.sc
  VARYING_DEF ${SHADER_DIR}/varying.def.sc
  OUTPUT_DIR ${SHADER_OUT_DIR}
  AS_HEADERS
  DEPENDS shaderc
  LANG spirv
  INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_DIRS}
)

set(CMAKE_CXX_STANDARD 20)


include(FetchContent)

# Pull GLFW automatically
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4  # or latest release tag
)


FetchContent_MakeAvailable(glfw)

add_library(imgui
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/imgui_demo.cpp
    external/imgui/backends/imgui_impl_glfw.cpp
    src/imgui/imgui_impl_bgfx.cpp
)

target_link_libraries(imgui PUBLIC glfw bgfx bx bimg)

target_include_directories(imgui PUBLIC
    external/imgui
    external/imgui/backends
    external/bgfx.cmake/bgfx/include
    external/bgfx.cmake/bx/include
    external/bgfx.cmake/bimg/include
    external/bgfx.cmake/bgfx/examples/common
    ${IMGUI_GEN_DIR}
    ${SHADER_OUT_DIR}
)

# engine sources
add_executable(C6GE
    src/main.cpp
    src/Engine/Engine.cpp
    src/Engine/Engine.h
    src/Window/Window.cpp
    src/Window/Window.h
    src/vs_cubes.sc
    src/fs_cubes.sc
    src/varying.def.sc
    external/bgfx.cmake/bgfx/examples/common/common.sh
    external/bgfx.cmake/bgfx/src/bgfx_shader.sh
)

target_link_libraries(C6GE PRIVATE
    bgfx
    bimg
    bx
    glfw
    imgui
)

target_include_directories(C6GE PRIVATE ${IMGUI_GEN_DIR} ${SHADER_OUT_DIR})