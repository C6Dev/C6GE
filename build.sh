#!/bin/bash

# Build script for C6GE
# Note: This builds the engine using CMake. GLFW is built from source, supporting both debug and release modes.

echo "Building C6GE..."
echo "Warning: The provided GLFW library only works in debug mode. Release builds will not work unless you replace the library with a release version."

# Create build directory
mkdir -p build
cd build

# Generate CMake files
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Build the project
make

echo "Build completed. Executable is in the build directory."