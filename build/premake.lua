
-- ==============================================================================

local initialDir = os.getcwd()
local rootSourceDir = "src"
local osxHomebrew = "/opt/homebrew/opt/"

function GetInitialDir()
    return initialDir
end
function GetSourceDir()
    return rootSourceDir
end
function SrcRoot()
    return GetInitialDir() .. "/../" .. rootSourceDir .. "/"
end
function GetHomebrewDir()
    return osxHomebrew
end


ModuleRefInclude = {}
ModuleRefLink = {}


include "ispc-premake/premake.lua"

include "premake-inc/common.lua"
include "premake-inc/sys-freetype.lua"
include "premake-inc/sys-openssl.lua"


-- ==============================================================================
workspace ("ouroveon_" .. _ACTION)

    configurations  { "Debug", "Release", "Release-AVX2" }
    platforms       { "x86_64" }
    architecture      "x64"

    location "_gen"

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

        defines
        {
            "OURO_PLATFORM_WIN=0",
            "OURO_PLATFORM_OSX=0",
            "OURO_PLATFORM_NIX=1",

            "OURO_FEATURE_VST24=0",
        }
        buildoptions
        {
            "-fPIC -fms-extensions"
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
    includedirs
    {
        SrcRoot() .. "r0.external/brotli/include",
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
        SrcRoot() .. "r0.external/brotli/**.c",
        SrcRoot() .. "r0.external/brotli/**.h",
    }

-- ------------------------------------------------------------------------------
function AddInclude_ZLIB()
    includedirs
    {
        SrcRoot() .. "r0.external/zlib",
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
        SrcRoot() .. "r0.external/zlib/**.c",
        SrcRoot() .. "r0.external/zlib/**.h",
    }


-- ------------------------------------------------------------------------------
function AddInclude_FLAC()
    includedirs
    {
        SrcRoot() .. "r0.external/xiph/include",
    }
    defines
    {
        "ENABLE_64_BIT_WORDS",
        "FLAC__HAS_OGG=1",
        "FLAC__CPU_X86_64",
        "FLAC__HAS_X86INTRIN",
        "FLAC__ALIGN_MALLOC_DATA",
        "FLAC__NO_DLL",
        "FLAC__OVERFLOW_DETECT",
        "FLaC__INLINE=_inline",
        "PACKAGE_VERSION=\"1.3.3\"",
    }
end

project "ext-flac"
    kind "StaticLib"
    language "C"

    disablewarnings { "4267", "4996", "4244", "4334" }

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")

    AddInclude_FLAC()
    includedirs
    {
        SrcRoot() .. "r0.external/xiph/src/libFLAC/include",
    }
    files 
    {
        SrcRoot() .. "r0.external/xiph/include/libFLAC/**.h",
        SrcRoot() .. "r0.external/xiph/src/libFLAC/**.h",
        SrcRoot() .. "r0.external/xiph/src/libFLAC/**.c",
        SrcRoot() .. "r0.external/xiph/src/ogg/*.c",
    }

project "ext-flac-cpp"
    kind "StaticLib"
    language "C++"

    disablewarnings { "4267", "4996", "4244", "4334" }

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")

    AddInclude_FLAC()
    files 
    { 
        SrcRoot() .. "r0.external/xiph/include/FLAC++/*.h",
        SrcRoot() .. "r0.external/xiph/src/libFLAC++/*.cpp"
    }

-- ------------------------------------------------------------------------------
function AddInclude_SQLITE3()
    includedirs
    {
        SrcRoot() .. "r0.external/sqlite3",
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
        SrcRoot() .. "r0.external/sqlite3/**.c",
        SrcRoot() .. "r0.external/sqlite3/**.h",
    }

-- ------------------------------------------------------------------------------
function AddInclude_SODIUM()
    includedirs
    {
        SrcRoot() .. "r0.external/sodium/src/libsodium/include/",
    }
    defines
    { 
        "SODIUM_STATIC",
        "NATIVE_LITTLE_ENDIAN"
    }
end

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
        SrcRoot() .. "r0.external/sodium/src/**.c",
        SrcRoot() .. "r0.external/sodium/src/**.h",
    }
    
    includedirs
    {
        SrcRoot() .. "r0.external/sodium/src/libsodium/include/sodium",
    }

-- ------------------------------------------------------------------------------
function AddInclude_OPUS()
    includedirs
    {
        SrcRoot() .. "r0.external/opus/include",
    }
    defines 
    { 
        "OPUS_EXPORT="
    }
end

project "ext-opus"
    kind "StaticLib"
    language "C"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")
    
    AddInclude_OPUS()

    includedirs
    {
        SrcRoot() .. "r0.external/opus/",
        SrcRoot() .. "r0.external/opus/win32",
        SrcRoot() .. "r0.external/opus/celt",
        SrcRoot() .. "r0.external/opus/silk",
        SrcRoot() .. "r0.external/opus/silk/float",
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
        SrcRoot() .. "r0.external/opus/celt/*.c",
        SrcRoot() .. "r0.external/opus/celt/*.h",
        SrcRoot() .. "r0.external/opus/silk/*.c",
        SrcRoot() .. "r0.external/opus/silk/*.h",
        SrcRoot() .. "r0.external/opus/src/*.c",
        SrcRoot() .. "r0.external/opus/src/*.h",
        SrcRoot() .. "r0.external/opus/include/**.h",
    }
    files
    {
        SrcRoot() .. "r0.external/opus/win32/*.h",
        SrcRoot() .. "r0.external/opus/celt/x86/*.c",
        SrcRoot() .. "r0.external/opus/celt/x86/*.h",
        SrcRoot() .. "r0.external/opus/silk/float/*.c",
        SrcRoot() .. "r0.external/opus/silk/x86/*.c",
        SrcRoot() .. "r0.external/opus/silk/x86/*.h",
    }
    excludes 
    { 
        SrcRoot() .. "r0.external/opus/celt/opus_custom_demo.c",
        SrcRoot() .. "r0.external/opus/src/opus_compare.c",
        SrcRoot() .. "r0.external/opus/src/opus_demo.c",
        SrcRoot() .. "r0.external/opus/src/repacketizer_demo.c",
        SrcRoot() .. "r0.external/opus/src/mlp_train.*",
    }


-- ------------------------------------------------------------------------------
function AddInclude_DRAGONBOX()
    includedirs
    {
        SrcRoot() .. "r0.external/dragonbox/include",
    }
end

project "ext-dragonbox"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")
    
    AddInclude_DRAGONBOX()
    files 
    { 
        SrcRoot() .. "r0.external/dragonbox/**.cpp",
        SrcRoot() .. "r0.external/dragonbox/**.h",
    }

-- ------------------------------------------------------------------------------
function AddInclude_URIPARSER()
    includedirs
    {
        SrcRoot() .. "r0.external/uriparser/include",
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
        SrcRoot() .. "r0.external/uriparser/**.c",
        SrcRoot() .. "r0.external/uriparser/**.h",
    }

-- ------------------------------------------------------------------------------
function AddInclude_DATE_TZ()
    includedirs
    {
        SrcRoot() .. "r0.external/date/include",
    }
end

project "ext-date-tz"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")

    AddInclude_DATE_TZ()
    files
    {
        SrcRoot() .. "r0.external/date/include/**.h",
        SrcRoot() .. "r0.external/date/src/tz.cpp"
    }

-- ------------------------------------------------------------------------------
function AddInclude_AIFF()
    includedirs
    {
        SrcRoot() .. "r0.external/libaiff",
    }
end

project "ext-libaiff"
    kind "StaticLib"
    language "C"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")

    defines
    {
        "_CRT_SECURE_NO_WARNINGS"
    }
    
    AddInclude_AIFF()
    files 
    { 
        SrcRoot() .. "r0.external/libaiff/**.c",
        SrcRoot() .. "r0.external/libaiff/**.h",
    }

-- ------------------------------------------------------------------------------
function AddInclude_R8BRAIN()
    includedirs
    {
        SrcRoot() .. "r0.external/r8brain",
    }
    defines
    {
        "R8B_PFFFT=1"
    }
end

project "ext-r8brain"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")
    
    AddInclude_R8BRAIN()
    files 
    { 
        SrcRoot() .. "r0.external/r8brain/**.cpp",
        SrcRoot() .. "r0.external/r8brain/**.h",
    }

-- ------------------------------------------------------------------------------
function AddInclude_PFOLD()
    includedirs
    {
        SrcRoot() .. "r0.external/pfold",
    }
end

project "ext-pfold"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")
    
    AddInclude_PFOLD()
    files 
    { 
        SrcRoot() .. "r0.external/pfold/*.cpp",
        SrcRoot() .. "r0.external/pfold/*.h",
    }

-- ------------------------------------------------------------------------------
function AddInclude_FASTNOISE()
    includedirs
    {
        SrcRoot() .. "r0.external/fastnoise2/include",
    }
end

project "ext-fastnoise2"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")
    
    AddInclude_FASTNOISE()
    files 
    { 
        SrcRoot() .. "r0.external/fastnoise2/**.cpp",
        SrcRoot() .. "r0.external/fastnoise2/**.inl",
        SrcRoot() .. "r0.external/fastnoise2/**.h",
    }

-- ------------------------------------------------------------------------------
function AddInclude_FMT()
    includedirs
    {
        SrcRoot() .. "r0.external/fmt/include",
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
        SrcRoot() .. "r0.external/fmt/**.cc",
        SrcRoot() .. "r0.external/fmt/**.h",
    }
    excludes
    {
        SrcRoot() .. "r0.external/fmt/src/fmt.cc"
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
        SrcRoot() .. "r0.external/kissfft",
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
        SrcRoot() .. "r0.external/kissfft/*.c",
        SrcRoot() .. "r0.external/kissfft/*.h",
    }

-- ------------------------------------------------------------------------------
function AddInclude_CITY()
    includedirs
    {
        SrcRoot() .. "r0.external/cityhash",
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
        SrcRoot() .. "r0.external/cityhash/*.cc",
        SrcRoot() .. "r0.external/cityhash/*.h",
    }

-- ------------------------------------------------------------------------------
function AddInclude_STB()
    includedirs
    {
        SrcRoot() .. "r0.external/stb",
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
        SrcRoot() .. "r0.external/stb/*.cpp",
        SrcRoot() .. "r0.external/stb/*.c",
        SrcRoot() .. "r0.external/stb/*.h",
    }

-- ------------------------------------------------------------------------------
function AddInclude_HTTPLIB()
    includedirs
    {
        SrcRoot() .. "r0.external/httplib",
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
    includedirs
    {
        SrcRoot() .. "r0.external/dpp/include",
    }
    defines { "DPP_ENABLE_VOICE" }
end

project "ext-dpp"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"

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
        SrcRoot() .. "r0.external/json",
    }

    defines 
    {
        "_CRT_SECURE_NO_WARNINGS"
    }

    files 
    { 
        SrcRoot() .. "r0.external/dpp/src/**.cpp",
        SrcRoot() .. "r0.external/dpp/include/**.h",
    }

    pchsource ( "../src/r0.external/dpp/src/dpp_pch.cpp" )
    pchheader "dpp_pch.h"



-- ==============================================================================


group ""

-- ------------------------------------------------------------------------------
function SetupOuroveonLayer( isFinalRing, layerName )

    language "C++"
    cppdialect "C++20"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories(layerName)

    if isFinalRing == true then
       targetdir   ( "$(SolutionDir)../../bin/" .. layerName .. "/build_%{cfg.shortname}" )
    else
        targetdir   ( "$(SolutionDir)_" .. layerName .. "/build_%{cfg.shortname}" )
    end

    -- bundle Superluminal performance API into win builds
    filter "system:Windows"
        defines 
        {
            "PERF_SUPERLUMINAL"
        }
        libdirs
        {
            SrcRoot() .. "r0.sys/superluminal/lib/x64"
        }

        filter "configurations:Debug"
        links ( "PerformanceAPI_MDd.lib" )
        filter "configurations:Release or Release-AVX2"
        links ( "PerformanceAPI_MD.lib" )
        filter {}

        includedirs
        {
            SrcRoot() .. "r0.sys/superluminal/include",
        }
    filter {}

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
        SrcRoot() .. "r0.closed/steinberg",

        SrcRoot() .. "r0.external/concurrent",
        SrcRoot() .. "r0.external/utf8",
        SrcRoot() .. "r0.external/cereal/include",
        SrcRoot() .. "r0.external/cereal/include/cereal_optional",
        SrcRoot() .. "r0.external/taskflow",
        SrcRoot() .. "r0.external/robinhood",
        SrcRoot() .. "r0.external/cppcodec",
        SrcRoot() .. "r0.external/json",

        SrcRoot() .. "r0.core",
        SrcRoot() .. "r0.platform",
        SrcRoot() .. "r1.endlesss",
        SrcRoot() .. "r2.action",
    }

    -- sdk layer compiles all code
    if isFinalRing == false then
    files 
    { 
        SrcRoot() .. "r0.core/**.cpp",
        SrcRoot() .. "r0.core/**.ispc",
        SrcRoot() .. "r0.core/**.isph",

        SrcRoot() .. "r0.platform/**.cpp",

        SrcRoot() .. "r1.endlesss/**.cpp",

        SrcRoot() .. "r2.action/**.cpp",
    }
    end

    -- headers
    files 
    { 
        SrcRoot() .. "r0.closed/steinberg/**.h",

        SrcRoot() .. "r0.external/concurrent/*.h",
        SrcRoot() .. "r0.external/utf8/*.h",
        SrcRoot() .. "r0.external/cereal/include/**.hpp",
        SrcRoot() .. "r0.external/taskflow/**.hpp",
        SrcRoot() .. "r0.external/robinhood/**.h",
        SrcRoot() .. "r0.external/cppcodec/**.hpp",
        SrcRoot() .. "r0.external/httplib/**.h",
        SrcRoot() .. "r0.external/json/**.h",
        SrcRoot() .. "r0.external/date/include/**.h",

        SrcRoot() .. "r0.core/**.h",
        SrcRoot() .. "r0.core/**.inl",

        SrcRoot() .. "r0.platform/**.h",

        SrcRoot() .. "r1.endlesss/**.h",

        SrcRoot() .. "r2.action/**.h",
    }

end

-- ------------------------------------------------------------------------------
function CommonAppLink()

    for libName, libFn in pairs(ModuleRefLink) do
        libFn()
    end

    links
    {
        "ext-zlib",
        "ext-flac",
        "ext-flac-cpp",
        "ext-sqlite3",
        "ext-dragonbox",
        "ext-opus",
        "ext-sodium",
        "ext-dpp",
        "ext-uriparser",
        "ext-date-tz",
        "ext-brotli",
        "ext-pfold",
        "ext-r8brain",
        "ext-fmt",
        "ext-kissfft",
        "ext-cityhash",
        "ext-stb",

        "sdk"
    }

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
    }
    filter {}

end


-- ==============================================================================


group "ouroveon"

project "sdk"
    kind "StaticLib"
    SetupOuroveonLayer( false, "sdk" )

    pchsource "../src/r0.core/pch.cpp"
    pchheader "pch.h"

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
        SrcRoot() .. "r3.beam/pch.cpp",

        SrcRoot() .. "r3.beam/**.cpp",
        SrcRoot() .. "r3.beam/**.h",
        SrcRoot() .. "r3.beam/**.inl",

        SrcRoot() .. "r3.beam/*.rc",
        SrcRoot() .. "r3.beam/*.ico",
    }

    pchsource "../src/r3.beam/pch.cpp"
    pchheader "pch.h"

-- ------------------------------------------------------------------------------
project "LORE"

    kind "ConsoleApp"
    SetupOuroveonLayer( true, "lore" )
    CommonAppLink()

    files 
    {
        SrcRoot() .. "r3.lore/pch.cpp",

        SrcRoot() .. "r3.lore/**.cpp",
        SrcRoot() .. "r3.lore/**.h",
        SrcRoot() .. "r3.lore/**.inl",

        SrcRoot() .. "r3.lore/*.rc",
        SrcRoot() .. "r3.lore/*.ico",
    }

    pchsource "../src/r3.lore/pch.cpp"
    pchheader "pch.h"

-- ------------------------------------------------------------------------------
project "PONY"

    kind "ConsoleApp"
    SetupOuroveonLayer( true, "pony" )
    CommonAppLink()

    files
    {
        SrcRoot() .. "r3.pony/pch.cpp",

        SrcRoot() .. "r3.pony/**.cpp",
        SrcRoot() .. "r3.pony/**.h",
        SrcRoot() .. "r3.pony/**.inl",

        SrcRoot() .. "r3.pony/*.rc",
        SrcRoot() .. "r3.pony/*.ico",
    }

    pchsource "../src/r3.pony/pch.cpp"
    pchheader "pch.h"

group ""
