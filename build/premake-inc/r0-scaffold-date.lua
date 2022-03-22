
LibRoot.Date = SrcDir() .. "r0.scaffold/date"

-- ==============================================================================
ModuleRefInclude["date"] = function()
    includedirs
    {
        LibRoot.Date .. "/include",
    }
    defines 
    {
        "HAS_REMOTE_API=0"
    }
end

-- ==============================================================================
project "r0-date"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("r0")

    ModuleRefInclude["date"]()

    files
    {
        LibRoot.Date .. "/src/tz.cpp",
        LibRoot.Date .. "/include/**.h",
    }

ModuleRefLink["date"] = function()
    links { "r0-date" }
end
