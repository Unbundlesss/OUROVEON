
LibRoot.Imgui = SrcDir() .. "r1.render/imgui"

-- ==============================================================================
ModuleRefInclude["imgui"] = function()

    includedirs
    {
        LibRoot.Imgui .. "/",
        LibRoot.Imgui .. "/backends/",
        LibRoot.Imgui .. "/freetype/",
        LibRoot.Imgui .. "/misc/cpp/",

        LibRoot.Imgui .. "-ex/",
        LibRoot.Imgui .. "-nodes/",
        LibRoot.Imgui .. "-implot/",
        LibRoot.Imgui .. "-filedlg/",
    }
    ModuleRefInclude["freetype"]()
end

-- ==============================================================================
project "r1-imgui"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("r1")

    ModuleRefInclude["glfw"]()
    ModuleRefInclude["imgui"]()

    defines 
    {
        "IMGUI_IMPL_OPENGL_LOADER_GLAD2",
    }
    files
    {
        LibRoot.Imgui .. "/*.cpp",
        LibRoot.Imgui .. "/*.h",
        LibRoot.Imgui .. "/freetype/*.cpp",
        LibRoot.Imgui .. "/freetype/*.h",
        LibRoot.Imgui .. "/backends/imgui_impl_glfw.*",
        LibRoot.Imgui .. "/backends/imgui_impl_opengl3.*",

        LibRoot.Imgui .. "/misc/cpp/*.*",

        LibRoot.Imgui .. "-nodes/*.cpp",
        LibRoot.Imgui .. "-nodes/*.h",

        LibRoot.Imgui .. "-implot/*.cpp",
        LibRoot.Imgui .. "-implot/*.h",

        LibRoot.Imgui .. "-filedlg/*.cpp",
        LibRoot.Imgui .. "-filedlg/*.h",
    }


ModuleRefLink["imgui"] = function()
    links { "r1-imgui" }
end