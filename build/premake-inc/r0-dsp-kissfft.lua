
LibRoot.KissFFT = SrcDir() .. "r0.dsp/kissfft"

-- ==============================================================================
ModuleRefInclude["kissfft"] = function()
    sysincludedirs
    {
        LibRoot.KissFFT,
    }
    defines 
    {
        -- "KISS_FFT_USE_SIMD",
        "KISS_FFT_USE_ALLOCA",
    }
end

-- ==============================================================================
project "r0-kissfft"
    kind "StaticLib"
    language "C"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("r0")

    ModuleRefInclude["kissfft"]()

    files
    {
        LibRoot.KissFFT .. "/*.c",
        LibRoot.KissFFT .. "/*.h",
    }

ModuleRefLink["kissfft"] = function()
    links { "r0-kissfft" }
end
