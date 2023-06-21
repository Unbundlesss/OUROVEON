
LibRoot.Imgui = SrcDir() .. "r1.render/imgui"

-- ==============================================================================
ModuleRefInclude["imgui"] = function()

    includedirs
    {
        LibRoot.Imgui .. "/",
        LibRoot.Imgui .. "/backends/",
        LibRoot.Imgui .. "/misc/freetype/",
        LibRoot.Imgui .. "/misc/cpp/",

        LibRoot.Imgui .. "-term/include/",
        LibRoot.Imgui .. "-ex/",
        LibRoot.Imgui .. "-filedlg/",
        LibRoot.Imgui .. "-implot/",
        LibRoot.Imgui .. "-nodes/",
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
        "IMGUI_DEFINE_MATH_OPERATORS",
    }
    files
    {
        LibRoot.Imgui .. "/*.cpp",
        LibRoot.Imgui .. "/*.h",
        LibRoot.Imgui .. "/misc/freetype/*.cpp",
        LibRoot.Imgui .. "/misc/freetype/*.h",
        LibRoot.Imgui .. "/backends/imgui_impl_glfw.*",
        LibRoot.Imgui .. "/backends/imgui_impl_opengl3.*",

        LibRoot.Imgui .. "/misc/cpp/*.*",

        LibRoot.Imgui .. "-term/include/*.cpp",
        LibRoot.Imgui .. "-term/include/*.hpp",

        LibRoot.Imgui .. "-ex/*.cpp",
        LibRoot.Imgui .. "-ex/*.h",

        LibRoot.Imgui .. "-filedlg/*.cpp",
        LibRoot.Imgui .. "-filedlg/*.h",

        LibRoot.Imgui .. "-implot/*.cpp",
        LibRoot.Imgui .. "-implot/*.h",

        LibRoot.Imgui .. "-nodes/*.cpp",
        LibRoot.Imgui .. "-nodes/*.h",
    }


ModuleRefLink["imgui"] = function()
    links { "r1-imgui" }
end