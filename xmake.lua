add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "."})

add_rpathdirs(".")

target("aaaa0ggmcLib6", function()
    set_kind("shared")
    set_languages("c++26")
    add_cxxflags("-freflection", {force = true, public = true})
    add_rules("utils.symbols.export_all")
    
    add_includedirs("/home/aaaa0ggmc/Projs/aaaa0ggmcLib/include", {public = true})
    add_files("/home/aaaa0ggmc/Projs/aaaa0ggmcLib/include/alib6/**.cppm", {public = true})
    add_files("/home/aaaa0ggmc/Projs/aaaa0ggmcLib/modules/alib6/**.cpp")
    
    add_headerfiles("/home/aaaa0ggmc/Projs/aaaa0ggmcLib/include/(alib6/**.cppm)")
    add_headerfiles("/home/aaaa0ggmc/Projs/aaaa0ggmcLib/include/(alib6/**.h)")
    add_syslinks("stdc++exp", {public = true})
end)

target("learn_vulkan", function()
    set_kind("binary")
    set_languages("c++26")
    add_cxxflags("-freflection", {force = true})
    add_syslinks("stdc++exp")
    
    add_includedirs("include", {public = true})
    add_files("src/*.cpp")
    
    add_defines("GLFW_INCLUDE_VULKAN")
    add_links("glfw", "vulkan")
    add_deps("aaaa0ggmcLib6")
    set_rundir("$(projectdir)")
end)
