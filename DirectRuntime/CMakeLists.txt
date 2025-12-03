cmake_minimum_required(VERSION 3.10)
project(DirectRuntime)

# Set source and include directories
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

# Collect all source files in src/
file(GLOB_RECURSE SRC_FILES "${SRC_DIR}/*.cpp" "${SRC_DIR}/*.c")

# Collect all header files in include/ (optional, for IDEs)
file(GLOB_RECURSE HEADER_FILES "${INCLUDE_DIR}/*.h" "${INCLUDE_DIR}/*.hpp")

# Add executable
add_executable(DirectRuntime ${SRC_FILES} ${HEADER_FILES})

# Include directories
target_include_directories(DirectRuntime PRIVATE ${INCLUDE_DIR})
target_include_directories(DirectRuntime PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../DirectEngine/include")

if (WIN32)
	# Add Vulkan SDK include directory on Windows (assumes default VULKAN_SDK env var)
	if (DEFINED ENV{VULKAN_SDK})
		target_include_directories(DirectRuntime PRIVATE "$ENV{VULKAN_SDK}/Include")
	endif()
endif()

# Link against the DirectEngine engine library
target_link_libraries(DirectRuntime PRIVATE DirectEngine)

# Copy DirectRuntime.exe (DirectEngine.dll is already copied by the DirectEngine target)
set(MAIN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin")
add_custom_command(TARGET DirectRuntime POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"$<TARGET_FILE:DirectRuntime>"
		"${MAIN_OUTPUT_DIR}/$<TARGET_FILE_NAME:DirectRuntime>"
)

# GLFW setup
set(GLFW_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/GLFW")
add_subdirectory(${GLFW_DIR})

# Link GLFW to DirectRuntime
target_link_libraries(DirectRuntime PRIVATE glfw)

# Include GLFW headers
target_include_directories(DirectRuntime PRIVATE "${GLFW_DIR}/include")