@echo off

echo Building C6GE...
echo Warning: The provided GLFW library only works in debug mode. Release builds will not work unless you replace the library with a release version.

rem Create build directory
if not exist build mkdir build
cd build

rem Generate CMake files
cmake .. -DCMAKE_BUILD_TYPE=Debug

rem Build the project
cmake --build .

echo Build completed. Executable is in the build directory.