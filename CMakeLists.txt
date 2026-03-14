cmake_minimum_required(VERSION 3.10)

include_directories(./include)

aux_source_directory("./src" SRC)
add_executable(learn_vulkan ${SRC})
target_compile_options(learn_vulkan PRIVATE -std=c++26 -Wl,)
target_link_libraries(learn_vulkan PRIVATE glfw aaaa0ggmcLib vulkan)
target_compile_definitions(learn_vulkan PRIVATE GLFW_INCLUDE_VULKAN ALIB5_USE_VULKAN_H)
target_link_options(learn_vulkan PRIVATE 
  -Wl,--rpath,/usr/local/lib
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
