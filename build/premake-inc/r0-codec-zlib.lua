
LibRoot.Zlib = SrcDir() .. "r0.codec/zlib"

-- ==============================================================================
ModuleRefInclude["zlib"] = function()
    includedirs
    {
        LibRoot.Zlib,
    }
end

-- ==============================================================================
project "r0-zlib"
    kind "StaticLib"
    language "C"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("r0")

    ModuleRefInclude["zlib"]()

    files
    {
        LibRoot.Zlib .. "/**.c",
        LibRoot.Zlib .. "/**.h",
    }

ModuleRefLink["zlib"] = function()
    links { "r0-zlib" }
end

