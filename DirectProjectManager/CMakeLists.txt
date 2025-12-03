cmake_minimum_required(VERSION 3.15)
project(DirectProjectManager NONE)

# Function to check for dotnet and .NET 9
function(check_dotnet)
    execute_process(
        COMMAND dotnet --list-sdks
        OUTPUT_VARIABLE sdks
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT sdks)
        message(FATAL_ERROR "dotnet CLI not found. Please install .NET SDK from https://dotnet.microsoft.com/download")
    endif()

    string(REGEX MATCH "9\\.[0-9]+\\.[0-9]+" net9_version "${sdks}")
    if(NOT net9_version)
        message(FATAL_ERROR ".NET 9 SDK not found. Please install .NET 9 SDK from https://dotnet.microsoft.com/download/dotnet/9.0")
    endif()
endfunction()

check_dotnet()

# Choose a sensible .NET runtime identifier (RID) for the host platform
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    set(DOTNET_RID "win-x64")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(DOTNET_RID "linux-x64")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    set(DOTNET_RID "osx-x64")
else()
    set(DOTNET_RID "win-x64")
endif()

# Unique target name for DirectProjectManager
add_custom_target(dotnet_build_projectmanager ALL
    COMMAND dotnet publish -c Debug -r ${DOTNET_RID} --self-contained false
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Publishing Avalonia app with dotnet publish"
)
add_dependencies(dotnet_build_projectmanager DirectEngine)

# Copy DirectProjectManager.exe and DirectEngine.dll after build (if available)
set(MAIN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin")
add_custom_command(TARGET dotnet_build_projectmanager POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/bin/Debug/net9.0/${DOTNET_RID}/publish"
        "${MAIN_OUTPUT_DIR}/DirectProjectManager"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${MAIN_OUTPUT_DIR}/DirectEngine.dll"
        "${MAIN_OUTPUT_DIR}/DirectProjectManager/DirectEngine.dll"
 )