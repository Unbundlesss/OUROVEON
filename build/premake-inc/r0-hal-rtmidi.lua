
function Root_RTMIDI()
    return SrcDir() .. "r0.hal/rtmidi/"
end

-- ==============================================================================
ModuleRefInclude["rtmidi"] = function()
    includedirs
    {
        Root_RTMIDI(),
    }
end

-- ==============================================================================
project "r0-rtmidi"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("r0")
    
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

    
ModuleRefLink["rtmidi"] = function()
    links { "r0-rtmidi" }
end