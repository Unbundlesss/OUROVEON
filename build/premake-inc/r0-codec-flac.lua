
LibRoot.Xiph = SrcDir() .. "r0.codec/xiph"

-- ==============================================================================
ModuleRefInclude["flac"] = function()

    defines
    {
        "FLAC__NO_DLL",
    }

    filter "system:Windows"
    externalincludedirs ( LibRoot.Xiph .. "/include" )
    filter {}

    filter "system:macosx"
    externalincludedirs ( GetPrebuiltLibs_MacUniversal_Headers() .. "flac/include" )
    filter {}
end

if ( os.host() == "windows" ) then

project "r0-flac"
    kind "StaticLib"
    language "C"

    disablewarnings { "4267", "4996", "4244", "4334" }

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("r0")

    ModuleRefInclude["flac"]()

    -- some precomputed config.h for windows source build of FLAC
    defines
    {
        "ENABLE_64_BIT_WORDS",
        "FLAC__HAS_OGG=1",
        "FLAC__CPU_X86_64",
        "FLAC__HAS_X86INTRIN=1",
        "FLAC__ALIGN_MALLOC_DATA",
        "FLAC__OVERFLOW_DETECT",
        "HAVE_INTTYPES_H=1",
        "HAVE_STDINT_H=1",
        "PACKAGE_VERSION=\"1.3.3\"",
    }

    includedirs
    {
        LibRoot.Xiph .. "/src/libFLAC/include",
    }
    files 
    {
        LibRoot.Xiph .. "/include/libFLAC/**.h",
        LibRoot.Xiph .. "/src/libFLAC/**.h",
        LibRoot.Xiph .. "/src/libFLAC/**.c",
        LibRoot.Xiph .. "/src/ogg/*.c",
    }

project "r0-flac-cpp"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"

    disablewarnings { "4267", "4996", "4244", "4334" }

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("r0")

    ModuleRefInclude["flac"]()

    files 
    { 
        LibRoot.Xiph .. "/include/FLAC++/*.h",
        LibRoot.Xiph .. "/src/libFLAC++/*.cpp"
    }

end

-- ==============================================================================
function _FLAC_LinkPrebuilt()

    links       { "FLAC", "FLAC++", "ogg" }

    filter "system:macosx"
    libdirs     ( GetPrebuiltLibs_MacUniversal() )
    filter {}
end

function _FLAC_LinkProject()
    links       { "r0-flac", "r0-flac-cpp" }
end

ModuleRefLinkWin["flac"]      = _FLAC_LinkProject
ModuleRefLinkLinux["flac"]    = _FLAC_LinkPrebuilt
ModuleRefLinkOSX["flac"]      = _FLAC_LinkPrebuilt
