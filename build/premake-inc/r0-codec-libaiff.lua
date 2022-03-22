
LibRoot.Aiff = SrcDir() .. "r0.codec/libaiff"

-- ==============================================================================
ModuleRefInclude["libaiff"] = function()
    includedirs
    {
        LibRoot.Aiff,
    }
end

-- ==============================================================================
project "r0-libaiff"
    kind "StaticLib"
    language "C"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("r0")

    ModuleRefInclude["libaiff"]()

    files
    {
        LibRoot.Aiff .. "/**.c",
        LibRoot.Aiff .. "/**.h",
    }

ModuleRefLink["libaiff"] = function()
    links { "r0-libaiff" }
end