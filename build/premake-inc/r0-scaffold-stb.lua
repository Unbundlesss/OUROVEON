
LibRoot.Stb = SrcDir() .. "r0.scaffold/stb"

-- ==============================================================================
ModuleRefInclude["stb"] = function()
    includedirs
    {
        LibRoot.Stb,
    }
    defines
    {
        "STB_VORBIS_NO_STDIO"
    }
end

-- ==============================================================================
project "r0-stb"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("r0")

    ModuleRefInclude["stb"]()

    files
    {
        LibRoot.Stb .. "/*.cpp",
        LibRoot.Stb .. "/*.c",
        LibRoot.Stb .. "/*.h",
    }

ModuleRefLink["stb"] = function()
    links { "r0-stb" }
end
