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
	# Add Vulkan SDK include directory on Windows (assumes default VULKAN_SDK env var)
	if (DEFINED ENV{VULKAN_SDK})
		target_include_directories(C6GE PRIVATE "$ENV{VULKAN_SDK}/Include")
        target_link_directories(C6GE PRIVATE "$ENV{VULKAN_SDK}/Lib")
        target_link_libraries(C6GE PRIVATE vulkan-1)
	endif()
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