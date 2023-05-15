
LibRoot.muFFT = SrcDir() .. "r0.dsp/pffft"

-- ==============================================================================
ModuleRefInclude["pffft"] = function()
    externalincludedirs
    {
        LibRoot.muFFT,
    }
    defines 
    {

    }
end

-- ==============================================================================
project "r0-pffft"
    kind "StaticLib"
    language "C"
    
    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("r0")

    ModuleRefInclude["pffft"]()

    files
    {
        LibRoot.muFFT .. "/fftpack.c",
        LibRoot.muFFT .. "/fftpack.h",
        LibRoot.muFFT .. "/pffft.c",
        LibRoot.muFFT .. "/pffft.h",
    }

    filter "system:Windows"
    disablewarnings { "4305", "4244" }
    defines 
    {
        "_USE_MATH_DEFINES"
    }
    filter {}

ModuleRefLink["pffft"] = function()
    links { "r0-pffft" }
end
