add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "."})

add_rpathdirs("/usr/local/lib", ".")

target("learn_vulkan", function()
    set_kind("binary")
    set_languages("c++26")
    add_cxxflags("-freflection", {force = true})
    add_syslinks("stdc++exp")
    
    add_includedirs("include", "/usr/local/include", {public = true})
    add_files("src/*.cpp")
    add_files("/usr/local/include/alib6/**.cppm")
    
    add_defines("GLFW_INCLUDE_VULKAN")
    add_links("aaaa0ggmcLib6", "glfw", "vulkan")
    set_rundir("$(projectdir)")
end)
