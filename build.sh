#!/bin/bash

# Build script for C6GE
# Note: This builds the engine using CMake. GLFW is built from source, supporting both debug and release modes.

echo "Building C6GE..."

# Create build directory
mkdir -p build
cd build

# Generate CMake files
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5

# Build the project
make

echo "Build completed. Executable is in the build directory."