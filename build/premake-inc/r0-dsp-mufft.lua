
LibRoot.muFFT = SrcDir() .. "r0.dsp/muFFT"

-- ==============================================================================
ModuleRefInclude["mufft"] = function()
    sysincludedirs
    {
        LibRoot.muFFT,
    }
    defines 
    {

    }
end

-- ==============================================================================
project "r0-mufft"
    kind "StaticLib"
    language "C"
    floatingpoint "Strict"
    
    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("r0")

    ModuleRefInclude["mufft"]()

    files
    {
        LibRoot.muFFT .. "/**.c",
        LibRoot.muFFT .. "/**.h",
    }

ModuleRefLink["mufft"] = function()
    links { "r0-mufft" }
end
