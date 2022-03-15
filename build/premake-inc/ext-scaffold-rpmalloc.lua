
LibRoot.RPM = SrcDir() .. "r0.ext-scaffold/rpmalloc"

-- ------------------------------------------------------------------------------
ModuleRefInclude["rpmalloc"] = function()
    includedirs
    {
        LibRoot.RPM .. "/rpmalloc",
    }
end

-- ==============================================================================
project "ext-rpmalloc"
    kind "StaticLib"
    language "C"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")

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
    links { "ext-rpmalloc" }
end
