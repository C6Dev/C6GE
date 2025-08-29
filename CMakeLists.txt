
cmake_minimum_required(VERSION 3.21)
project(C6GE)

# Set CMake policies to suppress warnings
cmake_policy(SET CMP0175 NEW)

# Set bgfx configuration options before adding subdirectory
set(BX_CONFIG_DEBUG ON CACHE BOOL "Enable debug logging" FORCE)
set(BGFX_CONFIG_DEBUG_ANNOTATION ON CACHE BOOL "Enable debug annotations" FORCE)

add_subdirectory(external/bgfx.cmake ${CMAKE_BINARY_DIR}/bgfx.cmake-build)
include(${CMAKE_CURRENT_SOURCE_DIR}/external/bgfx.cmake/cmake/bgfxToolUtils.cmake)

# Add meshoptimizer
add_subdirectory(external/meshoptimizer)

# Add bgfx shader build integration
set(SHADER_DIR ${CMAKE_SOURCE_DIR}/src)
set(SHADER_OUT_DIR ${CMAKE_BINARY_DIR}/generated/shaders)
set(BGFX_SHADER_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/external/bgfx.cmake/bgfx/examples/common
    ${CMAKE_SOURCE_DIR}/external/bgfx.cmake/bgfx/src
)

bgfx_compile_shaders(
  TYPE VERTEX
  SHADERS ${SHADER_DIR}/vs_mesh.sc
  VARYING_DEF ${SHADER_DIR}/varying.def.sc
  OUTPUT_DIR ${SHADER_OUT_DIR}
  AS_HEADERS
  DEPENDS shaderc
  LANG glsl
  INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_DIRS}
)

bgfx_compile_shaders(
  TYPE VERTEX
  SHADERS ${SHADER_DIR}/vs_mesh.sc
  VARYING_DEF ${SHADER_DIR}/varying.def.sc
  OUTPUT_DIR ${SHADER_OUT_DIR}
  AS_HEADERS
  DEPENDS shaderc
  LANG essl
  INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_DIRS}
)

bgfx_compile_shaders(
  TYPE VERTEX
  SHADERS ${SHADER_DIR}/vs_mesh.sc
  VARYING_DEF ${SHADER_DIR}/varying.def.sc
  OUTPUT_DIR ${SHADER_OUT_DIR}
  AS_HEADERS
  DEPENDS shaderc
  LANG spirv
  INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_DIRS}
)

bgfx_compile_shaders(
  TYPE FRAGMENT
  SHADERS ${SHADER_DIR}/fs_mesh.sc
  VARYING_DEF ${SHADER_DIR}/varying.def.sc
  OUTPUT_DIR ${SHADER_OUT_DIR}
  AS_HEADERS
  DEPENDS shaderc
  LANG glsl
  INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_DIRS}
)

bgfx_compile_shaders(
  TYPE FRAGMENT
  SHADERS ${SHADER_DIR}/fs_mesh.sc
  VARYING_DEF ${SHADER_DIR}/varying.def.sc
  OUTPUT_DIR ${SHADER_OUT_DIR}
  AS_HEADERS
  DEPENDS shaderc
  LANG essl
  INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_DIRS}
)

bgfx_compile_shaders(
  TYPE FRAGMENT
  SHADERS ${SHADER_DIR}/fs_mesh.sc
  VARYING_DEF ${SHADER_DIR}/varying.def.sc
  OUTPUT_DIR ${SHADER_OUT_DIR}
  AS_HEADERS
  DEPENDS shaderc
  LANG spirv
  INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_DIRS}
)

# Metal shaders for macOS
if(APPLE)
  bgfx_compile_shaders(
    TYPE VERTEX
    SHADERS ${SHADER_DIR}/vs_mesh.sc
    VARYING_DEF ${SHADER_DIR}/varying.def.sc
    OUTPUT_DIR ${SHADER_OUT_DIR}
    AS_HEADERS
    DEPENDS shaderc
    LANG metal
    INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_DIRS}
  )

  bgfx_compile_shaders(
    TYPE FRAGMENT
    SHADERS ${SHADER_DIR}/fs_mesh.sc
    VARYING_DEF ${SHADER_DIR}/varying.def.sc
    OUTPUT_DIR ${SHADER_OUT_DIR}
    AS_HEADERS
    DEPENDS shaderc
    LANG metal
    INCLUDE_DIRS ${BGFX_SHADER_INCLUDE_DIRS}
  )

endif()

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

target_link_libraries(imgui PUBLIC glfw)

target_compile_definitions(imgui PUBLIC
    BX_CONFIG_DEBUG=1
)

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
    src/Render/Render.cpp
    src/Render/Render.h
    src/Window/Window.cpp
    src/Window/Window.h
    src/vs_mesh.sc
    src/fs_mesh.sc
    src/varying.def.sc
    external/bgfx.cmake/bgfx/examples/common/common.sh
    external/bgfx.cmake/bgfx/src/bgfx_shader.sh
    external/bgfx.cmake/bgfx/examples/common/bgfx_utils.cpp
    src/entry_stubs.cpp
    src/MeshLoader/MeshLoader.cpp
)

target_compile_definitions(C6GE PRIVATE
    BX_CONFIG_DEBUG=1
)

target_link_libraries(C6GE PRIVATE
    bgfx
    bimg
    bimg_decode
    bx
    glfw
    imgui
    meshoptimizer
)

# Platform-specific linking
if(APPLE)
    target_link_libraries(C6GE PRIVATE
        "-framework Cocoa"
        "-framework Metal"
        "-framework QuartzCore"
        "-framework IOKit"
        "-framework CoreFoundation"
        "-framework Foundation"
    )
elseif(WIN32)
    target_link_libraries(C6GE PRIVATE
        "-lkernel32"
        "-luser32"
        "-lgdi32"
        "-lwinspool"
        "-lshell32"
        "-lole32"
        "-loleaut32"
        "-luuid"
        "-lcomdlg32"
        "-ladvapi32"
    )
endif()

target_include_directories(C6GE PRIVATE 
    ${IMGUI_GEN_DIR} 
    ${SHADER_OUT_DIR}
    ${CMAKE_SOURCE_DIR}/external/bgfx.cmake/bgfx/examples/common
    ${CMAKE_SOURCE_DIR}/external/bgfx.cmake/bx/include
    ${CMAKE_SOURCE_DIR}/external/bgfx.cmake/bgfx/examples/common/entry
    ${CMAKE_SOURCE_DIR}/external
)

# Copy assets to the same directory as the executable
add_custom_command(TARGET C6GE POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/src/assets $<TARGET_FILE_DIR:C6GE>/assets
)