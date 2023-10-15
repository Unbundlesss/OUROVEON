
LibRoot.Ryml = SrcDir() .. "r0.data/rapidyaml"

-- ==============================================================================
ModuleRefInclude["rapidyaml"] = function()
    includedirs
    {
        path.join( LibRoot.Ryml, "include" ),
    }
    defines
    {
        "C4_NO_DEBUG_BREAK",
        "RYML_EXPORTS"
    }
end

-- ==============================================================================
project "r0-rapidyaml"
    kind "StaticLib"
    language "C++"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("r0")

    ModuleRefInclude["rapidyaml"]()

    files
    {
        LibRoot.Ryml .. "/**.cpp",
        LibRoot.Ryml .. "/**.hpp",
    }

ModuleRefLink["rapidyaml"] = function()
    links { "r0-rapidyaml" }
end
