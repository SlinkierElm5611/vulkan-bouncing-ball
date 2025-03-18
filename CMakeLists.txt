
cmake_minimum_required(VERSION 3.15)

project(bouncing_ball)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

add_custom_target(shaders
    COMMAND glslc -fshader-stage=vertex -o vert.spv vert.glsl
    COMMAND glslc -fshader-stage=fragment -o frag.spv frag.glsl
    COMMAND glslc -fshader-stage=compute -o comp.spv comp.glsl
    DEPENDS vert.glsl frag.glsl comp.glsl
    BYPRODUCTS vert.spv frag.spv comp.spv
)

find_package(glfw3 REQUIRED)

find_package(Vulkan REQUIRED)

add_executable(bouncing_ball main.cpp)

add_dependencies(bouncing_ball glfw Vulkan::Vulkan shaders)

target_link_libraries(bouncing_ball glfw Vulkan::Vulkan)
