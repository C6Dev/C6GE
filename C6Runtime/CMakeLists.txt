cmake_minimum_required(VERSION 3.10)
project(C6Runtime)

# Set source and include directories
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

# Collect all source files in src/
file(GLOB_RECURSE SRC_FILES "${SRC_DIR}/*.cpp" "${SRC_DIR}/*.c")

# Collect all header files in include/ (optional, for IDEs)
file(GLOB_RECURSE HEADER_FILES "${INCLUDE_DIR}/*.h" "${INCLUDE_DIR}/*.hpp")

# Add executable
add_executable(C6Runtime ${SRC_FILES} ${HEADER_FILES})

# Include directories
target_include_directories(C6Runtime PRIVATE ${INCLUDE_DIR})
target_include_directories(C6Runtime PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../C6GE/include")

if (WIN32)
	# Add Vulkan SDK include directory on Windows (assumes default VULKAN_SDK env var)
	if (DEFINED ENV{VULKAN_SDK})
		target_include_directories(C6Runtime PRIVATE "$ENV{VULKAN_SDK}/Include")
	endif()
endif()

# Link against the C6GE engine library
target_link_libraries(C6Runtime PRIVATE C6GE)

# Copy C6Runtime.exe (C6GE.dll is already copied by the C6GE target)
set(MAIN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin")
add_custom_command(TARGET C6Runtime POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"$<TARGET_FILE:C6Runtime>"
		"${MAIN_OUTPUT_DIR}/$<TARGET_FILE_NAME:C6Runtime>"
)

# GLFW setup
set(GLFW_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/GLFW")
add_subdirectory(${GLFW_DIR})

# Link GLFW to C6Runtime
target_link_libraries(C6Runtime PRIVATE glfw)

# Include GLFW headers
target_include_directories(C6Runtime PRIVATE "${GLFW_DIR}/include")