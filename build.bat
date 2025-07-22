@echo off

echo Building C6GE...

rem Create build directory
if not exist build mkdir build
cd build

rem Generate CMake files
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5

rem Build the project
cmake --build .

echo Build completed. Executable is in the build directory.