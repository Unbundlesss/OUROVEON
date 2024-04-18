
LibRoot.Zstd = SrcDir() .. "r0.codec/zstd"

-- ==============================================================================
ModuleRefInclude["zstd"] = function()
    includedirs
    {
        LibRoot.Zstd .. "/lib",
    }
    defines 
    { 
        "ZSTD_MULTITHREAD=0",
        "ZSTD_DLL_EXPORT=0",
        "ZSTD_LEGACY_SUPPORT=0"
    }
end

-- ==============================================================================
project "r0-zstd"
    kind "StaticLib"
    language "C"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("r0")

    ModuleRefInclude["zstd"]()

    files
    {
        LibRoot.Zstd .. "/**.c",
        LibRoot.Zstd .. "/**.h",
    }
    filter "system:linux"
        files
        {
            LibRoot.Zstd .. "/**.S",
        }
    filter {}

ModuleRefLink["zstd"] = function()
    links { "r0-zstd" }
end

