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

# Copy C6Runtime.exe and C6GE.dll after build
set(MAIN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin")
add_custom_command(TARGET C6Runtime POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"$<TARGET_FILE:C6Runtime>"
		"${MAIN_OUTPUT_DIR}/C6Runtime.exe"
	)

	# GLFW setup
	set(GLFW_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw")
	add_subdirectory(${GLFW_DIR})

	# Link GLFW to C6Runtime
	target_link_libraries(C6Runtime PRIVATE glfw)

	# Include GLFW headers
	target_include_directories(C6Runtime PRIVATE "${GLFW_DIR}/include")