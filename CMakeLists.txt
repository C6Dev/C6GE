# Custom target to copy everything from C6Editor and C6ProjectManager to main build/bin

# Copy all .exe and .dll files from C6Editor and C6ProjectManager publish folders to main build/bin after build
add_custom_target(copy_dotnet_outputs
	COMMAND ${CMAKE_COMMAND} -E copy_directory "${C6EDITOR_PUBLISH_DIR}" "${MAIN_OUTPUT_DIR}"
	COMMAND ${CMAKE_COMMAND} -E copy_directory "${C6PROJECTMANAGER_PUBLISH_DIR}" "${MAIN_OUTPUT_DIR}"
	COMMENT "Copying .exe and .dll files from dotnet publish folders to main build/bin"
)
add_dependencies(copy_dotnet_outputs dotnet_build_editor dotnet_build_projectmanager)
cmake_minimum_required(VERSION 3.15)
project(C6GE_MainBuild)

# Set output directories
set(MAIN_OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin)
file(MAKE_DIRECTORY ${MAIN_OUTPUT_DIR})

# Add C6GE (Engine)
add_subdirectory(C6GE)
set(C6GE_DLL "${CMAKE_BINARY_DIR}/C6GE/bin/C6GE.dll")

# Add C6Editor (C#)
add_subdirectory(C6Editor)
set(C6EDITOR_PUBLISH_DIR "${CMAKE_SOURCE_DIR}/C6Editor/bin/Debug/net9.0/win-x64/publish")
set(C6EDITOR_OUTPUT_DIR "${MAIN_OUTPUT_DIR}/C6Editor")

# Add C6ProjectManager (C#)
add_subdirectory(C6ProjectManager)
set(C6PROJECTMANAGER_PUBLISH_DIR "${CMAKE_SOURCE_DIR}/C6ProjectManager/bin/Debug/net9.0/win-x64/publish")
set(C6PROJECTMANAGER_OUTPUT_DIR "${MAIN_OUTPUT_DIR}/C6ProjectManager")

# Add C6Runtime (C++)
add_subdirectory(C6Runtime)
set(C6RUNTIME_EXE "${CMAKE_BINARY_DIR}/C6Runtime/bin/C6Runtime.exe")

