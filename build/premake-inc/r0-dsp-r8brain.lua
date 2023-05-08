
LibRoot.R8Brain = SrcDir() .. "r0.dsp/r8brain"

-- ==============================================================================
ModuleRefInclude["r8brain"] = function()
    externalincludedirs
    {
        LibRoot.R8Brain,
    }
    defines 
    {
        "R8B_PFFFT=1"
    }
end

-- ==============================================================================
project "r0-r8brain"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    floatingpoint "Strict"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("r0")

    ModuleRefInclude["r8brain"]()

    files
    {
        LibRoot.R8Brain .. "/**.cpp",
        LibRoot.R8Brain .. "/**.h",
    }

ModuleRefLink["r8brain"] = function()
    links { "r0-r8brain" }
end
