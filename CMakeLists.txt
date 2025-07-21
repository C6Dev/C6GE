cmake_minimum_required(VERSION 3.10)

project(C6GE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/C6GE/Libraries/Include)
include_directories(${CMAKE_SOURCE_DIR}/C6GE/src)

# Source files
set(SOURCES
    ${CMAKE_SOURCE_DIR}/C6GE/glad.c
    ${CMAKE_SOURCE_DIR}/C6GE/src/Main.cpp
    ${CMAKE_SOURCE_DIR}/C6GE/src/Engine/Engine.cpp
    ${CMAKE_SOURCE_DIR}/C6GE/src/Logging/Log.cpp
    ${CMAKE_SOURCE_DIR}/C6GE/src/Render/Render.cpp
    ${CMAKE_SOURCE_DIR}/C6GE/src/Window/Window.cpp
)

# Headers (optional, for IDEs)
set(HEADERS
    ${CMAKE_SOURCE_DIR}/C6GE/src/Engine/Engine.h
    ${CMAKE_SOURCE_DIR}/C6GE/src/Logging/Log.h
    ${CMAKE_SOURCE_DIR}/C6GE/src/Render/Render.h
    ${CMAKE_SOURCE_DIR}/C6GE/src/Window/Window.h
)


# Find OpenGL
find_package(OpenGL REQUIRED)

# GLFW configuration
if(WIN32)
  set(GLFW_LIBRARY ${CMAKE_SOURCE_DIR}/C6GE/Libraries/Lib/glfw3.lib)
else()
  find_library(GLFW_LIBRARY glfw)
  if(NOT GLFW_LIBRARY)
    message(WARNING "GLFW library not found. Please install GLFW.")
  endif()
endif()

# Executable
add_executable(C6GE ${SOURCES} ${HEADERS})

target_link_libraries(C6GE PRIVATE ${GLFW_LIBRARY} OpenGL::GL)

# For macOS, link necessary frameworks
if(APPLE)
    target_link_libraries(C6GE PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreFoundation")
endif()