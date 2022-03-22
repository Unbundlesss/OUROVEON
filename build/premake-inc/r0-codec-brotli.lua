
LibRoot.Brotli = SrcDir() .. "r0.codec/brotli"

-- ==============================================================================
ModuleRefInclude["brotli"] = function()
    includedirs
    {
        LibRoot.Brotli .. "/include",
    }
end

-- ==============================================================================
project "r0-brotli"
    kind "StaticLib"
    language "C"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("r0")

    ModuleRefInclude["brotli"]()

    files
    {
        LibRoot.Brotli .. "/**.c",
        LibRoot.Brotli .. "/**.h",
    }

ModuleRefLink["brotli"] = function()
    links { "r0-brotli" }
end

