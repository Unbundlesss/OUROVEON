
function Root_PORTAUDIO()
    return SrcDir() .. "r0.hal/portaudio/"
end

function Root_Steinberg()
    return SrcDir() .. "r0.closed/steinberg/"
end

-- ==============================================================================
ModuleRefInclude["portaudio"] = function()

    includedirs
    {
        Root_PORTAUDIO() .. "include",
    }

    filter "system:Windows"
    defines 
    {
        "PA_USE_ASIO",
        "PA_USE_DS",
        "PA_USE_WASAPI",
        "PA_USE_WDMKS",
    }
    filter {}

    filter "system:linux"
    defines 
    {
        "PA_USE_ALSA",
        "PA_USE_JACK",
    }
    links
    {
        "jack",
        "asound",
    }
    filter {}

    filter "system:macosx"
    defines 
    {
        "PA_USE_COREAUDIO",
    }
    filter {}
end

-- ==============================================================================
project "r0-portaudio"
    kind "StaticLib"
    language "C"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("r0")
    
    ModuleRefInclude["portaudio"]()

    includedirs
    {
        Root_PORTAUDIO() .. "src/common",
    }
    files 
    { 
        Root_PORTAUDIO() .. "src/common/**.*",
    }

    filter "system:Windows"
    includedirs
    {
        Root_PORTAUDIO() .. "src/os/win",

        Root_Steinberg() .. "asio_2_3_3/common",
        Root_Steinberg() .. "asio_2_3_3/host",
        Root_Steinberg() .. "asio_2_3_3/host/pc",
    }
    files 
    { 
        Root_PORTAUDIO() .. "src/os/win/**.*",
        Root_PORTAUDIO() .. "src/hostapi/asio/*.cpp",
        Root_PORTAUDIO() .. "src/hostapi/asio/*.h",
        Root_PORTAUDIO() .. "src/hostapi/dsound/*.*",
        Root_PORTAUDIO() .. "src/hostapi/wasapi/*.c",
        Root_PORTAUDIO() .. "src/hostapi/wdmks/*.c",
        Root_PORTAUDIO() .. "src/hostapi/wmme/*.c",
        Root_PORTAUDIO() .. "**.h",

        Root_Steinberg() .. "asio_2_3_3/common/asio.cpp",
        Root_Steinberg() .. "asio_2_3_3/host/asiodrivers.cpp",
        Root_Steinberg() .. "asio_2_3_3/host/pc/asiolist.cpp",
    }
    filter {}

    filter "system:linux"
    includedirs
    {
        Root_PORTAUDIO() .. "src/os/unix",
    }
    files 
    { 
        Root_PORTAUDIO() .. "src/os/unix/**.*",
        Root_PORTAUDIO() .. "src/hostapi/jack/*.c",
        Root_PORTAUDIO() .. "src/hostapi/alsa/*.c",
        Root_PORTAUDIO() .. "**.h",
    }
    filter {}

    filter "system:macosx"
    includedirs
    {
        Root_PORTAUDIO() .. "src/os/unix",
    }
    files 
    { 
        Root_PORTAUDIO() .. "src/os/unix/**.*",
        Root_PORTAUDIO() .. "src/hostapi/coreaudio/pa_mac_core.c",
        Root_PORTAUDIO() .. "src/hostapi/coreaudio/pa_mac_core_blocking.c",
        Root_PORTAUDIO() .. "src/hostapi/coreaudio/pa_mac_core_utilities.c",
        Root_PORTAUDIO() .. "src/hostapi/coreaudio/**.h",
    }
    filter {}


ModuleRefLink["portaudio"] = function()
    links { "r0-portaudio" }
end