
LibRoot.Fmt = SrcDir() .. "r0.scaffold/fmt"

-- ==============================================================================
ModuleRefInclude["fmt"] = function()
    includedirs
    {
        LibRoot.Fmt .. "/include",
    }
end

-- ==============================================================================
project "r0-fmt"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("r0")

    ModuleRefInclude["fmt"]()

    files 
    { 
        LibRoot.Fmt .. "/**.cc",
        LibRoot.Fmt .. "/**.h",
    }
    excludes
    {
        LibRoot.Fmt .. "/src/fmt.cc"
    }

ModuleRefLink["fmt"] = function()
    links { "r0-fmt" }
end
