
LibRoot.Sodium = SrcDir() .. "r0.data/sodium"

-- ==============================================================================
ModuleRefInclude["sodium"] = function()

    filter "system:Windows"
    sysincludedirs
    {
        LibRoot.Sodium .. "/src/libsodium/include/",
    }
    defines
    { 
        "SODIUM_STATIC",
        "NATIVE_LITTLE_ENDIAN"
    }
    filter {}

    filter "system:linux"
    sysincludedirs
    {
        "/usr/include/sodium/",
    }
    filter {}

    filter "system:macosx"
    sysincludedirs
    {
        GetMacOSPackgesDir() .. "libsodium/include",
    }
    filter {}

end

if ( os.host() == "windows" ) then

project "r0-sodium"
    kind "StaticLib"
    language "C"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("r0")

    disablewarnings { 
        "4244", -- conversion from 'int64_t' to 'unsigned char'
        "4197", -- top-level volatile in cast is ignored
    }
    ModuleRefInclude["sodium"]()
    files 
    { 
        LibRoot.Sodium .. "/src/**.c",
        LibRoot.Sodium .. "/src/**.h",
    }
    
    includedirs
    {
        LibRoot.Sodium .. "/src/libsodium/include/sodium",
    }

end

-- ==============================================================================
function _Sodium_LinkPrebuilt()

    filter "system:linux"
    links
    {
        "sodium",
    }
    filter {}

    filter "system:macosx"
    libdirs
    {
        GetPrebuiltLibs_MacUniversal()
    }
    links
    {
        "sodium",
    }
    filter {}
end

function _Sodium_LinkProject()
    links { "r0-sodium" }
end

ModuleRefLinkWin["sodium"]      = _Sodium_LinkProject
ModuleRefLinkLinux["sodium"]    = _Sodium_LinkPrebuilt
ModuleRefLinkOSX["sodium"]      = _Sodium_LinkPrebuilt
