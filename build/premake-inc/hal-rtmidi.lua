
function Root_RTMIDI()
    return SrcRoot() .. "r0.hal/rtmidi/"
end

-- ------------------------------------------------------------------------------
function _RtMidi_Include()
    includedirs
    {
        Root_RTMIDI(),
    }
end
ModuleRefInclude["rtmidi"] = _RtMidi_Include

-- ==============================================================================
project "hal-rtmidi"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("hal")
    
    ModuleRefInclude["rtmidi"]()

    defines
    {
        "RTMIDI_DO_NOT_ENSURE_UNIQUE_PORTNAMES"
    }
    
    filter "system:Windows"
    defines
    {
        "__WINDOWS_MM__",
    }
    filter {}

    filter "system:linux"
    defines 
    {
        "__LINUX_ALSA__ "
    }
    filter {}

    filter "system:macosx"
    defines
    {
        "__MACOSX_CORE__"
    }
    filter {}

    files 
    { 
        Root_RTMIDI() .. "**.cpp",
        Root_RTMIDI() .. "**.h",
    }

function _RtMidi_LinkProject()
    links { "hal-rtmidi" }
end
ModuleRefLink["rtmidi"] = _RtMidi_LinkProject