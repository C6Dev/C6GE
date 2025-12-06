cmake_minimum_required(VERSION 3.15)
project(DirectEngine_MainBuild)

# Ensure a shared output directory for runtime artifacts
set(MAIN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin")
file(MAKE_DIRECTORY "${MAIN_OUTPUT_DIR}")

# Core engine
add_subdirectory(DirectEngine)

# Native editor frontend
add_subdirectory(DirectEditor)

# Runtime executable
add_subdirectory(DirectRuntime)

