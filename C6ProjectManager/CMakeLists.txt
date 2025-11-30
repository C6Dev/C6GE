cmake_minimum_required(VERSION 3.15)
project(C6ProjectManager NONE)

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

# Unique target name for C6ProjectManager
add_custom_target(dotnet_build_projectmanager ALL
    COMMAND dotnet publish -c Debug -r win-x64 --self-contained false
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Publishing Avalonia app with dotnet publish"
)
add_dependencies(dotnet_build_projectmanager C6GE)

# Copy C6ProjectManager.exe and C6GE.dll after build (if available)
set(MAIN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin")
add_custom_command(TARGET dotnet_build_projectmanager POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/bin/Debug/net9.0/win-x64/publish"
        "${MAIN_OUTPUT_DIR}/C6ProjectManager"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${MAIN_OUTPUT_DIR}/C6GE.dll"
        "${MAIN_OUTPUT_DIR}/C6ProjectManager/C6GE.dll"
 )