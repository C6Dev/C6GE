cmake_minimum_required(VERSION 3.15)
project(DirectEngine LANGUAGES CXX)

# Set output to DLL
add_library(DirectEngine SHARED)

set_target_properties(DirectEngine PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

# Add source files from src/
file(GLOB_RECURSE SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

# Add header files from include/ (for IDEs and installation, not for compilation)
file(GLOB_RECURSE HEADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp")

target_sources(DirectEngine PRIVATE ${SRC_FILES} ${HEADER_FILES})

# Include directories
target_include_directories(DirectEngine
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

if (NOT APPLE)
    find_package(Vulkan REQUIRED)
    target_link_libraries(DirectEngine PUBLIC Vulkan::Vulkan)
endif()

# Set output directory for DLL
set_target_properties(DirectEngine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Copy DLL to main output directory after build
set(MAIN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin")
add_custom_command(TARGET DirectEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:DirectEngine>"
        "${MAIN_OUTPUT_DIR}/DirectEngine.dll"
)

# GLFW setup
set(GLFW_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/GLFW")
add_subdirectory(${GLFW_DIR})

# Link GLFW to DirectEngine
target_link_libraries(DirectEngine PUBLIC glfw)

# Include GLFW headers for dependents that consume DirectEngine headers
target_include_directories(DirectEngine PUBLIC "${GLFW_DIR}/include")

#vk-bootstrap setup
set(VK_BOOTSTRAP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/vk-bootstrap")
add_subdirectory(${VK_BOOTSTRAP_DIR})

# Link vk-bootstrap to DirectEngine
target_link_libraries(DirectEngine PUBLIC vk-bootstrap)

# VMA setup
set(VMA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/VMA")
target_include_directories(DirectEngine PUBLIC "${VMA_DIR}")

# GLM setup
set(GLM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/GLM")
target_include_directories(DirectEngine PUBLIC "${GLM_DIR}")

# DirectLogger setup
set(DIRECT_LOGGER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/DirectLogger")
add_subdirectory(${DIRECT_LOGGER_DIR})

# Link DirectLogger to DirectEngine
target_link_libraries(DirectEngine PUBLIC DirectLoggerLib)
target_include_directories(DirectEngine PUBLIC "${DIRECT_LOGGER_DIR}/include")

# stb_image setup
set(STB_IMAGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb_image")
target_include_directories(DirectEngine PUBLIC "${STB_IMAGE_DIR}")

# fastgltf setup
set(FASTGLTF_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fastgltf/include")
target_include_directories(DirectEngine PUBLIC "${FASTGLTF_DIR}")