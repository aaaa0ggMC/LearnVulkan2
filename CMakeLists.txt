cmake_minimum_required(VERSION 3.28)

project(learnvulkan2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(include /usr/local/include)
link_directories(/usr/local/lib)

file(GLOB_RECURSE SRC "src/*.cpp")
file(GLOB_RECURSE ALIB6_MODULES "/usr/local/include/alib6/*.cppm")

add_executable(learn_vulkan ${SRC} ${ALIB6_MODULES})
target_compile_options(learn_vulkan PRIVATE -std=c++26 -freflection)
target_compile_definitions(learn_vulkan PRIVATE GLFW_INCLUDE_VULKAN)
target_link_libraries(learn_vulkan PRIVATE aaaa0ggmcLib6 glfw vulkan stdc++exp)
target_link_options(learn_vulkan PRIVATE -Wl,--rpath,/usr/local/lib)
