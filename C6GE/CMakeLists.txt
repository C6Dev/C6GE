cmake_minimum_required(VERSION 3.15)
project(C6GE LANGUAGES CXX)

# Set output to DLL
add_library(C6GE SHARED)

# Add source files from src/
file(GLOB_RECURSE SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

# Add header files from include/ (for IDEs and installation, not for compilation)
file(GLOB_RECURSE HEADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp")

target_sources(C6GE PRIVATE ${SRC_FILES} ${HEADER_FILES})

# Include directories
target_include_directories(C6GE PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

if (NOT APPLE)
    find_package(Vulkan REQUIRED)
    target_link_libraries(C6GE PRIVATE Vulkan::Vulkan)
endif()

# Set output directory for DLL
set_target_properties(C6GE PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Copy DLL to main output directory after build
set(MAIN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin")
add_custom_command(TARGET C6GE POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:C6GE>"
        "${MAIN_OUTPUT_DIR}/C6GE.dll"
)