
LibRoot.RPM = SrcDir() .. "r0.scaffold/rpmalloc"

-- ==============================================================================
ModuleRefInclude["rpmalloc"] = function()
    includedirs
    {
        LibRoot.RPM .. "/rpmalloc",
    }
end

-- ==============================================================================
project "r0-rpmalloc"
    kind "StaticLib"
    language "C"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("r0")

    ModuleRefInclude["rpmalloc"]()

    filter "configurations:Debug"
    defines 
    {
        "ENABLE_STATISTICS",
        "ENABLE_VALIDATE_ARGS",
        "ENABLE_ASSERTS",
    }
    filter {}

    files
    {
        LibRoot.RPM .. "/rpmalloc/rpmalloc.c",
        LibRoot.RPM .. "/rpmalloc/rpmalloc.h",
    }

ModuleRefLink["rpmalloc"] = function()
    links { "r0-rpmalloc" }
end
