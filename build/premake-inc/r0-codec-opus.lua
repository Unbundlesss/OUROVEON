
LibRoot.Opus = SrcDir() .. "r0.codec/opus"

-- ==============================================================================
ModuleRefInclude["opus"] = function()

    filter "system:Windows"
    includedirs
    {
        LibRoot.Opus .. "/include",
    }
    defines 
    { 
        "OPUS_EXPORT="
    }
    filter {}

    filter "system:linux"
    externalincludedirs
    {
        "/usr/include/opus/",
    }
    filter {}

    filter "system:macosx"
    externalincludedirs
    {
        GetMacOSPackgesDir() .. "opus/include/opus",
    }
    filter {}    

end

if ( os.host() == "windows" ) then

project "r0-opus"
    kind "StaticLib"
    language "C"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("r0")
    
    ModuleRefInclude["opus"]()

    includedirs
    {
        LibRoot.Opus,
        LibRoot.Opus .. "/win32",
        LibRoot.Opus .. "/celt",
        LibRoot.Opus .. "/silk",
        LibRoot.Opus .. "/silk/float",
    }
    defines 
    { 
        "USE_ALLOCA=1",
        "OPUS_BUILD=1",
        "OPUS_X86_MAY_HAVE_SSE",
        "OPUS_X86_MAY_HAVE_SSE2",
        "OPUS_X86_MAY_HAVE_SSE4_1",
        "OPUS_X86_MAY_HAVE_AVX",
        "OPUS_HAVE_RTCD"
    }

    files
    {
        LibRoot.Opus .. "/celt/*.c",
        LibRoot.Opus .. "/celt/*.h",
        LibRoot.Opus .. "/silk/*.c",
        LibRoot.Opus .. "/silk/*.h",
        LibRoot.Opus .. "/src/*.c",
        LibRoot.Opus .. "/src/*.h",
        LibRoot.Opus .. "/include/**.h",
    }
    files
    {
        LibRoot.Opus .. "/win32/*.h",
        LibRoot.Opus .. "/celt/x86/*.c",
        LibRoot.Opus .. "/celt/x86/*.h",
        LibRoot.Opus .. "/silk/float/*.c",
        LibRoot.Opus .. "/silk/x86/*.c",
        LibRoot.Opus .. "/silk/x86/*.h",
    }
    excludes 
    { 
        LibRoot.Opus .. "/celt/opus_custom_demo.c",
        LibRoot.Opus .. "/src/opus_compare.c",
        LibRoot.Opus .. "/src/opus_demo.c",
        LibRoot.Opus .. "/src/repacketizer_demo.c",
        LibRoot.Opus .. "/src/mlp_train.*",
    }

end

-- ==============================================================================
function _Opus_LinkPrebuilt()

    filter "system:linux"
    links
    {
        "opus",
    }
    filter {}

    filter "system:macosx"
    libdirs
    {
        GetPrebuiltLibs_MacUniversal()
    }
    links
    {
        "opus",
    }
    filter {}
end

function _Opus_LinkProject()
    links { "r0-opus" }
end

ModuleRefLinkWin["opus"]        = _Opus_LinkProject
ModuleRefLinkLinux["opus"]      = _Opus_LinkPrebuilt
ModuleRefLinkOSX["opus"]        = _Opus_LinkPrebuilt

