

function Root_IMGUI()
    return SrcDir() .. "r0.external/imgui"
end

-- ------------------------------------------------------------------------------
function _ImGui_Include()
    includedirs
    {
        Root_IMGUI() .. "/",
        Root_IMGUI() .. "/backends/",
        Root_IMGUI() .. "/freetype/",
        Root_IMGUI() .. "/misc/cpp/",

        Root_IMGUI() .. "-ex/",
        Root_IMGUI() .. "-nodes/",
        Root_IMGUI() .. "-implot/",
        Root_IMGUI() .. "-filedlg/",
    }
    ModuleRefInclude["freetype"]()
end

ModuleRefInclude["imgui"] = _ImGui_Include

-- ==============================================================================
project "ext-imgui"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")

    ModuleRefInclude["glfw"]()
    ModuleRefInclude["imgui"]()

    defines 
    {
        "IMGUI_IMPL_OPENGL_LOADER_GLAD2",
    }
    files
    {
        Root_IMGUI() .. "/*.cpp",
        Root_IMGUI() .. "/*.h",
        Root_IMGUI() .. "/freetype/*.cpp",
        Root_IMGUI() .. "/freetype/*.h",
        Root_IMGUI() .. "/backends/imgui_impl_glfw.*",
        Root_IMGUI() .. "/backends/imgui_impl_opengl3.*",

        Root_IMGUI() .. "/misc/cpp/*.*",

        Root_IMGUI() .. "-nodes/*.cpp",
        Root_IMGUI() .. "-nodes/*.h",

        Root_IMGUI() .. "-implot/*.cpp",
        Root_IMGUI() .. "-implot/*.h",

        Root_IMGUI() .. "-filedlg/*.cpp",
        Root_IMGUI() .. "-filedlg/*.h",
    }

function _ImGui_LinkProject()
    links { "ext-imgui" }
end
ModuleRefLink["imgui"] = _ImGui_LinkProject