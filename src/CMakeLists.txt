
# Add source to this project's executable.
add_executable(DirectEngine
    main.cpp
    d_engine.cpp
    d_engine.h
    vk_types.h
    vk_initializers.h
)


set_property(TARGET DirectEngine PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:DirectEngine>")

target_include_directories(DirectEngine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(DirectEngine vk-bootstrap VulkanMemoryAllocator GLM fastgltf imgui stb_image fmt::fmt)

target_link_libraries(DirectEngine Vulkan::Vulkan SDL3::SDL3)

add_dependencies(DirectEngine Shaders)