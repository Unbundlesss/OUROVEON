
LibRoot.PFold = SrcDir() .. "r0.platform/pfold"

-- ==============================================================================
ModuleRefInclude["pfold"] = function()
    externalincludedirs
    {
        LibRoot.PFold,
    }
end

-- ==============================================================================
project "r0-pfold"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("r0")

    ModuleRefInclude["pfold"]()

    files
    {
        LibRoot.PFold .. "/*.cpp",
        LibRoot.PFold .. "/*.h",
    }

ModuleRefLink["pfold"] = function()
    links { "r0-pfold" }
end
