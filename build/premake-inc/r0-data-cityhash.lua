
LibRoot.Cityhash = SrcDir() .. "r0.data/cityhash"

-- ==============================================================================
ModuleRefInclude["cityhash"] = function()
    includedirs
    {
        LibRoot.Cityhash,
    }
end

-- ==============================================================================
project "r0-cityhash"
    kind "StaticLib"
    language "C"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("r0")

    ModuleRefInclude["cityhash"]()

    files
    {
        LibRoot.Cityhash .. "/**.cc",
        LibRoot.Cityhash .. "/**.h",
    }

ModuleRefLink["cityhash"] = function()
    links { "r0-cityhash" }
end
