--   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
--  |       |   |   |   __ \       |   |   |    ___|       |    |  |
--  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
--  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|

require('xcode')
local xcode = premake.modules.xcode 
xcode.cppLanguageStandards["C++20"] = "c++20"

require("vstudio")
premake.override(premake.vstudio.vc2010, "languageStandard", function(base, cfg)
    premake.vstudio.vc2010.element("LanguageStandard", nil, 'stdcpp20')
end)

-- ==============================================================================

-- stash the starting directory upfront to use as a reference root 
local initialDir = os.getcwd()
print( "Premake launch directory: " .. initialDir )

local rootBuildGenerationDir = "_gen"
local rootSourceDir = "src"
local osxHomebrew = "/opt/homebrew/opt/"

function GetSourceDir()
    return rootSourceDir
end
function SrcDir()
    return initialDir .. "/../" .. rootSourceDir .. "/"
end

function GetMacOSPackgesDir()
    return osxHomebrew
end
function GetPrebuiltLibs_MacUniversal()
    return initialDir .. "/../libs/macos/universal-fat/"
end

function GetPrebuiltLibs_Win64()
    return initialDir .. "/../libs/windows/win64/"
end
function GetPrebuiltLibs_Win64_VSMacro()
    return "$(SolutionDir)..\\..\\libs\\windows\\win64\\"
end


function GetBuildRootToken()
    if ( os.host() == "windows" ) then
        return "$(SolutionDir)"
    else
        return initialDir .. "/" .. rootBuildGenerationDir .. "/"
    end
end
print ( "Build root token : " .. GetBuildRootToken() )


ModuleRefInclude = {}
ModuleRefLink = {}

ModuleRefLinkWin    = {}
ModuleRefLinkLinux  = {}
ModuleRefLinkOSX    = {}


include "ispc-premake/premake.lua"

include "premake-inc/common.lua"
include "premake-inc/sys-freetype.lua"
include "premake-inc/sys-openssl.lua"
include "premake-inc/sys-superluminal.lua"


-- ==============================================================================
workspace ("ouroveon_" .. _ACTION)

    configurations  { "Debug", "Release" }

    if os.is64bit() then
        print( os.host() .. " 64-bit")
    else
        print( os.host() .. " 32-bit")
    end

    location (rootBuildGenerationDir)

    filter "system:Windows"

        platforms       { "x86_64" }
        architecture      "x64"

        useISPC()

        defines 
        {
            "WIN32",
            "_WINDOWS",

            "OURO_PLATFORM_WIN=1",
            "OURO_PLATFORM_OSX=0",
            "OURO_PLATFORM_NIX=0",

            "OURO_FEATURE_VST24=1",
            
            "OURO_CXX20_SEMA=1",

            -- for DPP; needs adjusting at the highest level before winsock is included
            "FD_SETSIZE=1024"
        }
        ispcVars { 
            OS              = "windows",
            Architecture    = "x64",
        }

    filter {}
    
    filter "system:linux"

        platforms       { "x86_64" }
        architecture      "x64"

        systemversion   "latest"
        pic             "On"
        staticruntime   "On"

        defines
        {
            "OURO_PLATFORM_WIN=0",
            "OURO_PLATFORM_OSX=0",
            "OURO_PLATFORM_NIX=1",

            "OURO_FEATURE_VST24=0",
        }
        buildoptions
        {
            "-ffast-math"
        }
    filter {}

    filter "system:macosx"

        platforms       { "universal" }
        architecture      "universal"
        systemversion     "11.0"
        
        defines
        {
            "OURO_PLATFORM_WIN=0",
            "OURO_PLATFORM_OSX=1",
            "OURO_PLATFORM_NIX=0",

            "OURO_FEATURE_VST24=0",

            "OURO_CXX20_SEMA=0",
        }
        buildoptions
        {
            "-Wall"
        }
    filter {}

group "external-hal"

include "premake-inc/hal-glfw.lua"
include "premake-inc/hal-portaudio.lua"
include "premake-inc/hal-rtmidi.lua"

group ""
group "external"

include "premake-inc/ext-imgui.lua"


-- ------------------------------------------------------------------------------
function AddInclude_BROTLI()
    sysincludedirs
    {
        SrcDir() .. "r0.external/brotli/include",
    }
end

project "ext-brotli"
    kind "StaticLib"
    language "C"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")
    
    AddInclude_BROTLI()
    files 
    { 
        SrcDir() .. "r0.external/brotli/**.c",
        SrcDir() .. "r0.external/brotli/**.h",
    }

-- ------------------------------------------------------------------------------
function AddInclude_ZLIB()
    sysincludedirs
    {
        SrcDir() .. "r0.external/zlib",
    }
end

project "ext-zlib"
    kind "StaticLib"
    language "C"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")
    
    AddInclude_ZLIB()
    files 
    { 
        SrcDir() .. "r0.external/zlib/**.c",
        SrcDir() .. "r0.external/zlib/**.h",
    }


-- ------------------------------------------------------------------------------
function AddInclude_FLAC()

    defines
    {
        "FLAC__NO_DLL",
    }

    filter "system:Windows"
    sysincludedirs
    {
        SrcDir() .. "r0.external/xiph/include",
    }
    filter {}

    filter "system:macosx"
    sysincludedirs ( GetMacOSPackgesDir() .. "flac/include" )
    filter {}    
end

if ( os.host() == "windows" ) then

project "ext-flac"
    kind "StaticLib"
    language "C"

    disablewarnings { "4267", "4996", "4244", "4334" }

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")

    AddInclude_FLAC()

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
        SrcDir() .. "r0.external/xiph/src/libFLAC/include",
    }
    files 
    {
        SrcDir() .. "r0.external/xiph/include/libFLAC/**.h",
        SrcDir() .. "r0.external/xiph/src/libFLAC/**.h",
        SrcDir() .. "r0.external/xiph/src/libFLAC/**.c",
        SrcDir() .. "r0.external/xiph/src/ogg/*.c",
    }

project "ext-flac-cpp"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"

    disablewarnings { "4267", "4996", "4244", "4334" }

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")

    AddInclude_FLAC()
    
    files 
    { 
        SrcDir() .. "r0.external/xiph/include/FLAC++/*.h",
        SrcDir() .. "r0.external/xiph/src/libFLAC++/*.cpp"
    }

end

-- ==============================================================================
function _FLAC_LinkPrebuilt()

    links       { "flac", "flac++", "ogg" }

    filter "system:macosx"
    libdirs     ( GetPrebuiltLibs_MacUniversal() )
    filter {}
end

function _FLAC_LinkProject()
    links       { "ext-flac", "ext-flac-cpp" }
end

ModuleRefLinkWin["flac"]      = _FLAC_LinkProject
ModuleRefLinkLinux["flac"]    = _FLAC_LinkPrebuilt
ModuleRefLinkOSX["flac"]      = _FLAC_LinkPrebuilt



-- ------------------------------------------------------------------------------
function AddInclude_SQLITE3()
    includedirs
    {
        SrcDir() .. "r0.external/sqlite3",
    }
    defines   { "SQLITE_ENABLE_FTS5", "SQLITE_ENABLE_JSON1" }
end

project "ext-sqlite3"
    kind "StaticLib"
    language "C"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")

    AddInclude_SQLITE3()
    files 
    { 
        SrcDir() .. "r0.external/sqlite3/**.c",
        SrcDir() .. "r0.external/sqlite3/**.h",
    }

-- ------------------------------------------------------------------------------
function AddInclude_SODIUM()

    filter "system:Windows"
    includedirs
    {
        SrcDir() .. "r0.external/sodium/src/libsodium/include/",
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

project "ext-sodium"

    kind "StaticLib"
    language "C"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")

    disablewarnings { 
        "4244", -- conversion from 'int64_t' to 'unsigned char'
        "4197", -- top-level volatile in cast is ignored
    }
    AddInclude_SODIUM()
    files 
    { 
        SrcDir() .. "r0.external/sodium/src/**.c",
        SrcDir() .. "r0.external/sodium/src/**.h",
    }
    
    includedirs
    {
        SrcDir() .. "r0.external/sodium/src/libsodium/include/sodium",
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
    links { "ext-sodium" }
end

ModuleRefLinkWin["sodium"]      = _Sodium_LinkProject
ModuleRefLinkLinux["sodium"]    = _Sodium_LinkPrebuilt
ModuleRefLinkOSX["sodium"]      = _Sodium_LinkPrebuilt


-- ------------------------------------------------------------------------------
function AddInclude_OPUS()
    filter "system:Windows"
    includedirs
    {
        SrcDir() .. "r0.external/opus/include",
    }
    defines 
    { 
        "OPUS_EXPORT="
    }
    filter {}

    filter "system:linux"
    sysincludedirs
    {
        "/usr/include/opus/",
    }
    filter {}

    filter "system:macosx"
    sysincludedirs
    {
        GetMacOSPackgesDir() .. "opus/include/opus",
    }
    filter {}    

end

if ( os.host() == "windows" ) then

project "ext-opus"
    kind "StaticLib"
    language "C"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")
    
    AddInclude_OPUS()

    includedirs
    {
        SrcDir() .. "r0.external/opus/",
        SrcDir() .. "r0.external/opus/win32",
        SrcDir() .. "r0.external/opus/celt",
        SrcDir() .. "r0.external/opus/silk",
        SrcDir() .. "r0.external/opus/silk/float",
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
        SrcDir() .. "r0.external/opus/celt/*.c",
        SrcDir() .. "r0.external/opus/celt/*.h",
        SrcDir() .. "r0.external/opus/silk/*.c",
        SrcDir() .. "r0.external/opus/silk/*.h",
        SrcDir() .. "r0.external/opus/src/*.c",
        SrcDir() .. "r0.external/opus/src/*.h",
        SrcDir() .. "r0.external/opus/include/**.h",
    }
    files
    {
        SrcDir() .. "r0.external/opus/win32/*.h",
        SrcDir() .. "r0.external/opus/celt/x86/*.c",
        SrcDir() .. "r0.external/opus/celt/x86/*.h",
        SrcDir() .. "r0.external/opus/silk/float/*.c",
        SrcDir() .. "r0.external/opus/silk/x86/*.c",
        SrcDir() .. "r0.external/opus/silk/x86/*.h",
    }
    excludes 
    { 
        SrcDir() .. "r0.external/opus/celt/opus_custom_demo.c",
        SrcDir() .. "r0.external/opus/src/opus_compare.c",
        SrcDir() .. "r0.external/opus/src/opus_demo.c",
        SrcDir() .. "r0.external/opus/src/repacketizer_demo.c",
        SrcDir() .. "r0.external/opus/src/mlp_train.*",
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
    links { "ext-opus" }
end

ModuleRefLinkWin["opus"]        = _Opus_LinkProject
ModuleRefLinkLinux["opus"]      = _Opus_LinkPrebuilt
ModuleRefLinkOSX["opus"]        = _Opus_LinkPrebuilt




-- ------------------------------------------------------------------------------
function AddInclude_DRAGONBOX()
    includedirs
    {
        SrcDir() .. "r0.external/dragonbox/include",
    }
end

project "ext-dragonbox"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")
    
    AddInclude_DRAGONBOX()
    files 
    { 
        SrcDir() .. "r0.external/dragonbox/**.cpp",
        SrcDir() .. "r0.external/dragonbox/**.h",
    }

-- ------------------------------------------------------------------------------
function AddInclude_URIPARSER()
    sysincludedirs
    {
        SrcDir() .. "r0.external/uriparser/include",
    }
    defines 
    {
        "URI_NO_UNICODE",
        "URI_STATIC_BUILD"
    }
end

project "ext-uriparser"
    kind "StaticLib"
    language "C"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")
    
    AddInclude_URIPARSER()
    files 
    { 
        SrcDir() .. "r0.external/uriparser/**.c",
        SrcDir() .. "r0.external/uriparser/**.h",
    }

-- ------------------------------------------------------------------------------
function AddInclude_DATE_TZ()
    includedirs
    {
        SrcDir() .. "r0.external/date/include",
    }
    defines 
    {
        "HAS_REMOTE_API=0"
    }
end

project "ext-date-tz"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")

    AddInclude_DATE_TZ()
    files
    {
        SrcDir() .. "r0.external/date/include/**.h",
        SrcDir() .. "r0.external/date/src/tz.cpp"
    }

-- ------------------------------------------------------------------------------
function AddInclude_AIFF()
    includedirs
    {
        SrcDir() .. "r0.external/libaiff",
    }
end

project "ext-libaiff"
    kind "StaticLib"
    language "C"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")

    AddInclude_AIFF()
    files 
    { 
        SrcDir() .. "r0.external/libaiff/**.c",
        SrcDir() .. "r0.external/libaiff/**.h",
    }

-- ------------------------------------------------------------------------------
function AddInclude_R8BRAIN()
    includedirs
    {
        SrcDir() .. "r0.external/r8brain",
    }
    defines
    {
        "R8B_PFFFT=1"
    }
end

project "ext-r8brain"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")
    
    AddInclude_R8BRAIN()
    files 
    { 
        SrcDir() .. "r0.external/r8brain/**.cpp",
        SrcDir() .. "r0.external/r8brain/**.h",
    }

-- ------------------------------------------------------------------------------
function AddInclude_PFOLD()
    includedirs
    {
        SrcDir() .. "r0.external/pfold",
    }
end

project "ext-pfold"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")
    
    AddInclude_PFOLD()
    files 
    { 
        SrcDir() .. "r0.external/pfold/*.cpp",
        SrcDir() .. "r0.external/pfold/*.h",
    }

-- ------------------------------------------------------------------------------
function AddInclude_FMT()
    includedirs
    {
        SrcDir() .. "r0.external/fmt/include",
    }
end

project "ext-fmt"
    kind "StaticLib"
    language "C"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")
    
    AddInclude_FMT()
    files 
    { 
        SrcDir() .. "r0.external/fmt/**.cc",
        SrcDir() .. "r0.external/fmt/**.h",
    }
    excludes
    {
        SrcDir() .. "r0.external/fmt/src/fmt.cc"
    }

-- ------------------------------------------------------------------------------
function AddInclude_FFT()
    defines
    {
    	-- "KISS_FFT_USE_SIMD",
    	"KISS_FFT_USE_ALLOCA",
    }	
    includedirs
    {
        SrcDir() .. "r0.external/kissfft",
    }
end

project "ext-kissfft"
    kind "StaticLib"
    language "C"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")

    AddInclude_FMT()
    files 
    { 
        SrcDir() .. "r0.external/kissfft/*.c",
        SrcDir() .. "r0.external/kissfft/*.h",
    }

-- ------------------------------------------------------------------------------
function AddInclude_CITY()
    sysincludedirs
    {
        SrcDir() .. "r0.external/cityhash",
    }
end

project "ext-cityhash"
    kind "StaticLib"
    language "C"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")

    AddInclude_CITY()
    files 
    { 
        SrcDir() .. "r0.external/cityhash/*.cc",
        SrcDir() .. "r0.external/cityhash/*.h",
    }

-- ------------------------------------------------------------------------------
function AddInclude_STB()
    includedirs
    {
        SrcDir() .. "r0.external/stb",
    }
    defines
    {
        "STB_VORBIS_NO_STDIO"
    }
end

project "ext-stb"
    kind "StaticLib"
    language "C"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")

    AddInclude_STB()
    files 
    { 
        SrcDir() .. "r0.external/stb/*.cpp",
        SrcDir() .. "r0.external/stb/*.c",
        SrcDir() .. "r0.external/stb/*.h",
    }

-- ------------------------------------------------------------------------------
function AddInclude_HTTPLIB()
    includedirs
    {
        SrcDir() .. "r0.external/httplib",
    }
    defines
    {
        "CPPHTTPLIB_OPENSSL_SUPPORT",
        "CPPHTTPLIB_BROTLI_SUPPORT",
        "CPPHTTPLIB_ZLIB_SUPPORT"
    }
    ModuleRefInclude["openssl"]()
end


-- ------------------------------------------------------------------------------
function AddInclude_DPP()
    sysincludedirs
    {
        SrcDir() .. "r0.external/dpp/include",
    }
    defines { "DPP_ENABLE_VOICE" }
end

project "ext-dpp"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")

    AddInclude_DPP()
    AddInclude_ZLIB()
    AddInclude_BROTLI()
    AddInclude_OPUS()
    AddInclude_SODIUM()
    AddInclude_FMT()
    AddInclude_HTTPLIB()
    includedirs
    {
        SrcDir() .. "r0.external/json",
    }

    files 
    { 
        SrcDir() .. "r0.external/dpp/src/**.cpp",
        SrcDir() .. "r0.external/dpp/include/**.h",
    }

    AddPCH(
        "../src/r0.external/dpp/src/dpp_pch.cpp",
        SrcDir() .. "r0.external/dpp/include/",
        "dpp_pch.h" )


-- ==============================================================================


group ""

-- ------------------------------------------------------------------------------
function SetupOuroveonLayer( isFinalRing, layerName )

    language "C++"
    cppdialect "C++20"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories(layerName)

    if isFinalRing == true then
       targetdir ( GetBuildRootToken() .. "../../bin/" .. layerName .. "/%{cfg.system}_%{cfg.shortname}" )
    else
       targetdir ( GetBuildRootToken() .. "_artefact/ouro/_" .. layerName .. "/%{cfg.system}_%{cfg.shortname}" )
    end


    defines 
    {
        "VST_2_4_EXTENSIONS",
        "VST_64BIT_PLATFORM=1",
    }

    for libName, libFn in pairs(ModuleRefInclude) do
        libFn()
    end

    AddInclude_BROTLI()
    AddInclude_ZLIB()
    AddInclude_FLAC()
    AddInclude_SQLITE3()
    AddInclude_DRAGONBOX()
    AddInclude_OPUS()
    AddInclude_SODIUM()
    AddInclude_DPP()
    AddInclude_URIPARSER()
    AddInclude_DATE_TZ()
    AddInclude_PFOLD()
    AddInclude_R8BRAIN()
    AddInclude_FMT()
    AddInclude_FFT()
    AddInclude_CITY()
    AddInclude_STB()
    AddInclude_HTTPLIB()

    includedirs
    {
        SrcDir() .. "r0.closed/steinberg",

        SrcDir() .. "r0.external/concurrent",
        SrcDir() .. "r0.external/utf8",
        SrcDir() .. "r0.external/cereal/include",
        SrcDir() .. "r0.external/cereal/include/cereal_optional",
        SrcDir() .. "r0.external/taskflow",
        SrcDir() .. "r0.external/robinhood",
        SrcDir() .. "r0.external/cppcodec",
        SrcDir() .. "r0.external/json",

        SrcDir() .. "r0.core",
        SrcDir() .. "r0.platform",
        SrcDir() .. "r1.endlesss",
        SrcDir() .. "r2.action",
    }

    -- sdk layer compiles all code
    if isFinalRing == false then
    files 
    { 
        SrcDir() .. "r0.core/**.cpp",
        SrcDir() .. "r0.core/**.ispc",
        SrcDir() .. "r0.core/**.isph",

        SrcDir() .. "r0.platform/**.cpp",

        SrcDir() .. "r1.endlesss/**.cpp",

        SrcDir() .. "r2.action/**.cpp",
    }
    end

    -- headers
    files 
    { 
        SrcDir() .. "r0.closed/steinberg/**.h",

        SrcDir() .. "r0.external/concurrent/*.h",
        SrcDir() .. "r0.external/utf8/*.h",
        SrcDir() .. "r0.external/cereal/include/**.hpp",
        SrcDir() .. "r0.external/taskflow/**.hpp",
        SrcDir() .. "r0.external/robinhood/**.h",
        SrcDir() .. "r0.external/cppcodec/**.hpp",
        SrcDir() .. "r0.external/httplib/**.h",
        SrcDir() .. "r0.external/json/**.h",
        SrcDir() .. "r0.external/date/include/**.h",

        SrcDir() .. "r0.core/**.h",
        SrcDir() .. "r0.core/**.inl",

        SrcDir() .. "r0.platform/**.h",

        SrcDir() .. "r1.endlesss/**.h",

        SrcDir() .. "r2.action/**.h",
    }

end

-- ------------------------------------------------------------------------------
function CommonAppLink()

    links
    {
        "sdk",

        "ext-sqlite3",
        "ext-dragonbox",
        "ext-dpp",
        "ext-uriparser",
        "ext-date-tz",
        "ext-pfold",
        "ext-r8brain",
        "ext-fmt",
        "ext-kissfft",
        "ext-cityhash",
        "ext-stb",

        "ext-brotli",
        "ext-zlib",
    }
    
    for libName, libFn in pairs(ModuleRefLink) do
        libFn()
    end
    if ( os.host() == "windows" ) then
        for libName, libFn in pairs(ModuleRefLinkWin) do
            libFn()
        end
    end
    if ( os.host() == "linux" ) then
        for libName, libFn in pairs(ModuleRefLinkLinux) do
            libFn()
        end
    end
    if ( os.host() == "macosx" ) then
        for libName, libFn in pairs(ModuleRefLinkOSX) do
            libFn()
        end
    end    

    filter "system:Windows"
    links
    {
        "opengl32",
        "winmm",
        "dxguid",

        "ws2_32",
        "shlwapi",
        "version",
        "setupapi",
        "imm32",
    }
    filter {}

    filter "system:linux"
    links 
    {
        "m",
        "pthread",
        "dl",

        "X11",
        "Xrandr",
        "Xinerama",
        "Xxf86vm",
        "Xcursor",
        "GL",
    }
    filter {}

    filter "system:macosx"
    links 
    {
        "pthread",

        "Cocoa.framework",
        "IOKit.framework",
        "OpenGL.framework",
        "CoreVideo.framework",
        "CoreAudio.framework",
        "CoreMIDI.framework",
        "AudioToolBox.framework",
        "AudioUnit.framework",
    }
    filter {}

end


-- ==============================================================================


group "ouroveon"

project "sdk"
    kind "StaticLib"
    SetupOuroveonLayer( false, "sdk" )

    AddPCH( 
        "../src/r0.core/pch.cpp",
        SrcDir() .. "r0.core/",
        "pch.h" )

group ""


-- ==============================================================================


group "apps"

-- ------------------------------------------------------------------------------
project "BEAM"

    kind "ConsoleApp"
    SetupOuroveonLayer( true, "beam" )
    CommonAppLink()

    files
    {
        SrcDir() .. "r3.beam/pch.cpp",

        SrcDir() .. "r3.beam/**.cpp",
        SrcDir() .. "r3.beam/**.h",
        SrcDir() .. "r3.beam/**.inl",
    }
    filter "system:windows"
    files
    {
        SrcDir() .. "r3.beam/*.rc",
        SrcDir() .. "r3.beam/*.ico",
    }
    filter {}

    AddPCH( 
        "../src/r3.beam/pch.cpp",
        SrcDir() .. "r0.core/",
        "pch.h" )

-- ------------------------------------------------------------------------------
project "LORE"

    kind "ConsoleApp"
    SetupOuroveonLayer( true, "lore" )
    CommonAppLink()

    files 
    {
        SrcDir() .. "r3.lore/pch.cpp",

        SrcDir() .. "r3.lore/**.cpp",
        SrcDir() .. "r3.lore/**.h",
        SrcDir() .. "r3.lore/**.inl",
    }
    filter "system:windows"
    files
    {
        SrcDir() .. "r3.lore/*.rc",
        SrcDir() .. "r3.lore/*.ico",
    }
    filter {}

    AddPCH( 
        "../src/r3.lore/pch.cpp",
        SrcDir() .. "r0.core/",
        "pch.h" )


-- ------------------------------------------------------------------------------
project "PONY"

    kind "ConsoleApp"
    SetupOuroveonLayer( true, "pony" )
    CommonAppLink()

    files
    {
        SrcDir() .. "r3.pony/pch.cpp",

        SrcDir() .. "r3.pony/**.cpp",
        SrcDir() .. "r3.pony/**.h",
        SrcDir() .. "r3.pony/**.inl",
    }

    filter "system:windows"
    files
    {
        SrcDir() .. "r3.pony/*.rc",
        SrcDir() .. "r3.pony/*.ico",
    }
    filter {}

    AddPCH( 
        "../src/r3.pony/pch.cpp",
        SrcDir() .. "r0.core/",
        "pch.h" )


group ""
