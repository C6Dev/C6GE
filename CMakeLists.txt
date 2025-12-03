# Custom target to copy everything from DirectEditor and DirectProjectManager to main build/bin

# Copy all .exe and .dll files from DirectEditor and DirectProjectManager publish folders to main build/bin after build
add_custom_target(copy_dotnet_outputs
	COMMAND ${CMAKE_COMMAND} -E copy_directory "${DirectEditor_PUBLISH_DIR}" "${MAIN_OUTPUT_DIR}"
	COMMAND ${CMAKE_COMMAND} -E copy_directory "${DirectProjectManager_PUBLISH_DIR}" "${MAIN_OUTPUT_DIR}"
	COMMENT "Copying .exe and .dll files from dotnet publish folders to main build/bin"
)
add_dependencies(copy_dotnet_outputs dotnet_build_editor dotnet_build_projectmanager)
cmake_minimum_required(VERSION 3.15)
project(DirectEngine_MainBuild)

# Set output directories
set(MAIN_OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin)
file(MAKE_DIRECTORY ${MAIN_OUTPUT_DIR})

# Add DirectEngine (Engine)
add_subdirectory(DirectEngine)
set(DirectEngine_DLL "${CMAKE_BINARY_DIR}/DirectEngine/bin/DirectEngine.dll")

# Add DirectEditor (C#)
add_subdirectory(DirectEditor)
# Choose .NET runtime identifier (RID) for host platform to find publish outputs
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
	set(DOTNET_RID "win-x64")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
	set(DOTNET_RID "linux-x64")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
	set(DOTNET_RID "osx-x64")
else()
	set(DOTNET_RID "win-x64")
endif()

set(DirectEditor_PUBLISH_DIR "${CMAKE_SOURCE_DIR}/DirectEditor/bin/Debug/net9.0/${DOTNET_RID}/publish")
set(DirectEditor_OUTPUT_DIR "${MAIN_OUTPUT_DIR}/DirectEditor")

# Add DirectRuntime (C++)
add_subdirectory(DirectRuntime)
# Use the target file of DirectRuntime so CMake picks the correct extension on each OS
set(DirectRuntime_EXE "$<TARGET_FILE:DirectRuntime>")

# Add DirectProjectManager (C#)
add_subdirectory(DirectProjectManager)
set(DirectProjectManager_PUBLISH_DIR "${CMAKE_SOURCE_DIR}/DirectProjectManager/bin/Debug/net9.0/${DOTNET_RID}/publish")
set(DirectProjectManager_OUTPUT_DIR "${MAIN_OUTPUT_DIR}/DirectProjectManager")

