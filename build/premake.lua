
include "ispc-premake/premake.lua"

local initialDir = os.getcwd()
function GetInitialDir()
    return initialDir
end

function SrcRoot()
    return initialDir .. "/../src/"
end


-- ------------------------------------------------------------------------------
function SetDefaultBuildConfiguration()

    filter "configurations:Debug"
        defines   { "DEBUG" }
        symbols   "On"
        ispcVars {
            GenerateDebugInformation = true,
            Opt         = "disabled",
            CPU         = "core2",
            TargetISA   = "sse2-i32x8",
        }

    filter "configurations:Release"
        defines   { "NDEBUG" }
        flags     { "LinkTimeOptimization" }
        optimize  "Full"

        ispcVars { 
            Opt         = "maximum",
            CPU         = "core2",
            TargetISA   = "sse2-i32x8",
        }


    filter "configurations:Release-AVX2"
        defines   { "NDEBUG" }
        flags     { "LinkTimeOptimization" }
        optimize  "Full"
		
        vectorextensions "AVX2"
        ispcVars  {
            Opt         = "maximum",
            CPU         = "core-avx2",
            TargetISA   = "avx2-i32x16",
        }

    filter {}

end

-- ------------------------------------------------------------------------------
function SetDefaultOutputDirectories(subgrp)

    targetdir   ( "$(SolutionDir)_artefact/bin_" .. subgrp .. "/$(Configuration)/%{cfg.platform}" )
    objdir      ( "!$(SolutionDir)_artefact/obj_" .. subgrp .. "/%{cfg.shortname}/$(ProjectName)/" )
    debugdir    ( "$(OutDir)" )

end


-- ==============================================================================
workspace ("ouroveon_" .. _ACTION)

    configurations  { "Debug", "Release", "Release-AVX2" }
    platforms       { "x86_64" }
    architecture      "x64"

    location "_sln"

    useISPC()

    filter "platforms:x86_64"
        system "windows"
        defines 
        {
            "WIN32",
            "_WINDOWS",

            "OURO_PLATFORM_WIN=1",
            "OURO_PLATFORM_OSX=0",
            "OURO_PLATFORM_NIX=0",

            "OURO_FEATURE_VST24=1",

            -- for DPP
            "FD_SETSIZE=1024"
        }
        ispcVars { 
            OS              = "windows",
            Architecture    = "x64",
        }

    filter {}


group "external"

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
function AddInclude_PORTAUDIO()
    includedirs
    {
        SrcRoot() .. "r0.external/portaudio/include",
    }
end

project "ext-portaudio"
    kind "StaticLib"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")
    
    defines
    {
        "_CRT_SECURE_NO_WARNINGS",

        "PA_USE_ASIO",
        "PA_USE_DS",
        "PA_USE_WASAPI",
        "PA_USE_WDMKS",
    }
    AddInclude_PORTAUDIO()
    includedirs
    {
        SrcRoot() .. "r0.external/portaudio/src/common",
        SrcRoot() .. "r0.external/portaudio/src/os/win",

        SrcRoot() .. "r0.external/steinberg/asio_minimal_2.3.3/common",
        SrcRoot() .. "r0.external/steinberg/asio_minimal_2.3.3/host",
        SrcRoot() .. "r0.external/steinberg/asio_minimal_2.3.3/host/pc",
    }
    files 
    { 
        SrcRoot() .. "r0.external/portaudio/src/common/**.*",
        SrcRoot() .. "r0.external/portaudio/src/os/win/**.*",
        SrcRoot() .. "r0.external/portaudio/src/hostapi/asio/*.cpp",
        SrcRoot() .. "r0.external/portaudio/src/hostapi/asio/*.h",
        SrcRoot() .. "r0.external/portaudio/src/hostapi/dsound/*.*",
        SrcRoot() .. "r0.external/portaudio/src/hostapi/wasapi/*.c",
        SrcRoot() .. "r0.external/portaudio/src/hostapi/wdmks/*.c",
        SrcRoot() .. "r0.external/portaudio/src/hostapi/wmme/*.c",
        SrcRoot() .. "r0.external/portaudio/**.h",

        SrcRoot() .. "r0.external/steinberg/asio_minimal_2.3.3/common/asio.cpp",
        SrcRoot() .. "r0.external/steinberg/asio_minimal_2.3.3/host/asiodrivers.cpp",
        SrcRoot() .. "r0.external/steinberg/asio_minimal_2.3.3/host/pc/asiolist.cpp",
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
function AddInclude_GLAD()
    includedirs
    {
        SrcRoot() .. "r0.external/glad/include",
    }
end

project "ext-glad"
    kind "StaticLib"
    language "C"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")

    AddInclude_GLAD()
    files
    {
        SrcRoot() .. "r0.external/glad/src/glad.c"
    }

-- ------------------------------------------------------------------------------
function AddInclude_SDL2()
    includedirs
    {
        SrcRoot() .. "r0.external/sdl2/include",
    }
end

project "ext-sdl"
    kind "StaticLib"
    language "C"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")

    defines 
    {
        "HAVE_LIBC=1"
    }
    files 
    { 
        SrcRoot() .. "r0.external/sdl2/include/**.h",
        SrcRoot() .. "r0.external/sdl2/src/atomic/SDL_atomic.c",
        SrcRoot() .. "r0.external/sdl2/src/atomic/SDL_spinlock.c",
        SrcRoot() .. "r0.external/sdl2/src/audio/directsound/SDL_directsound.c",
        SrcRoot() .. "r0.external/sdl2/src/audio/disk/SDL_diskaudio.c",
        SrcRoot() .. "r0.external/sdl2/src/audio/dummy/SDL_dummyaudio.c",
        SrcRoot() .. "r0.external/sdl2/src/audio/SDL_audio.c",
        SrcRoot() .. "r0.external/sdl2/src/audio/SDL_audiocvt.c",
        SrcRoot() .. "r0.external/sdl2/src/audio/SDL_audiodev.c",
        SrcRoot() .. "r0.external/sdl2/src/audio/SDL_audiotypecvt.c",
        SrcRoot() .. "r0.external/sdl2/src/audio/SDL_mixer.c",
        SrcRoot() .. "r0.external/sdl2/src/audio/SDL_wave.c",
        SrcRoot() .. "r0.external/sdl2/src/audio/winmm/SDL_winmm.c",
        SrcRoot() .. "r0.external/sdl2/src/audio/wasapi/SDL_wasapi.c",
        SrcRoot() .. "r0.external/sdl2/src/audio/wasapi/SDL_wasapi_win32.c",
        SrcRoot() .. "r0.external/sdl2/src/core/windows/SDL_hid.c",
        SrcRoot() .. "r0.external/sdl2/src/core/windows/SDL_windows.c",
        SrcRoot() .. "r0.external/sdl2/src/core/windows/SDL_xinput.c",
        SrcRoot() .. "r0.external/sdl2/src/cpuinfo/SDL_cpuinfo.c",
        SrcRoot() .. "r0.external/sdl2/src/dynapi/SDL_dynapi.c",
        SrcRoot() .. "r0.external/sdl2/src/events/SDL_clipboardevents.c",
        SrcRoot() .. "r0.external/sdl2/src/events/SDL_displayevents.c",
        SrcRoot() .. "r0.external/sdl2/src/events/SDL_dropevents.c",
        SrcRoot() .. "r0.external/sdl2/src/events/SDL_events.c",
        SrcRoot() .. "r0.external/sdl2/src/events/SDL_gesture.c",
        SrcRoot() .. "r0.external/sdl2/src/events/SDL_keyboard.c",
        SrcRoot() .. "r0.external/sdl2/src/events/SDL_mouse.c",
        SrcRoot() .. "r0.external/sdl2/src/events/SDL_quit.c",
        SrcRoot() .. "r0.external/sdl2/src/events/SDL_touch.c",
        SrcRoot() .. "r0.external/sdl2/src/events/SDL_windowevents.c",
        SrcRoot() .. "r0.external/sdl2/src/file/SDL_rwops.c",
        SrcRoot() .. "r0.external/sdl2/src/filesystem/windows/SDL_sysfilesystem.c",
        SrcRoot() .. "r0.external/sdl2/src/haptic/dummy/SDL_syshaptic.c",
        SrcRoot() .. "r0.external/sdl2/src/haptic/SDL_haptic.c",
        SrcRoot() .. "r0.external/sdl2/src/haptic/windows/SDL_dinputhaptic.c",
        SrcRoot() .. "r0.external/sdl2/src/haptic/windows/SDL_windowshaptic.c",
        SrcRoot() .. "r0.external/sdl2/src/haptic/windows/SDL_xinputhaptic.c",
        SrcRoot() .. "r0.external/sdl2/src/hidapi/SDL_hidapi.c",
        SrcRoot() .. "r0.external/sdl2/src/joystick/dummy/SDL_sysjoystick.c",
        SrcRoot() .. "r0.external/sdl2/src/joystick/hidapi/SDL_hidapijoystick.c",
        SrcRoot() .. "r0.external/sdl2/src/joystick/hidapi/SDL_hidapi_gamecube.c",
        SrcRoot() .. "r0.external/sdl2/src/joystick/hidapi/SDL_hidapi_luna.c",
        SrcRoot() .. "r0.external/sdl2/src/joystick/hidapi/SDL_hidapi_ps4.c",
        SrcRoot() .. "r0.external/sdl2/src/joystick/hidapi/SDL_hidapi_ps5.c",
        SrcRoot() .. "r0.external/sdl2/src/joystick/hidapi/SDL_hidapi_rumble.c",
        SrcRoot() .. "r0.external/sdl2/src/joystick/hidapi/SDL_hidapi_stadia.c",
        SrcRoot() .. "r0.external/sdl2/src/joystick/hidapi/SDL_hidapi_switch.c",
        SrcRoot() .. "r0.external/sdl2/src/joystick/hidapi/SDL_hidapi_xbox360.c",
        SrcRoot() .. "r0.external/sdl2/src/joystick/hidapi/SDL_hidapi_xbox360w.c",
        SrcRoot() .. "r0.external/sdl2/src/joystick/hidapi/SDL_hidapi_xboxone.c",
        SrcRoot() .. "r0.external/sdl2/src/joystick/SDL_gamecontroller.c",
        SrcRoot() .. "r0.external/sdl2/src/joystick/SDL_joystick.c",
        SrcRoot() .. "r0.external/sdl2/src/joystick/virtual/SDL_virtualjoystick.c",
        SrcRoot() .. "r0.external/sdl2/src/joystick/windows/SDL_dinputjoystick.c",
        SrcRoot() .. "r0.external/sdl2/src/joystick/windows/SDL_mmjoystick.c",
        SrcRoot() .. "r0.external/sdl2/src/joystick/windows/SDL_rawinputjoystick.c",
        SrcRoot() .. "r0.external/sdl2/src/joystick/windows/SDL_windowsjoystick.c",
        SrcRoot() .. "r0.external/sdl2/src/joystick/windows/SDL_windows_gaming_input.c",
        SrcRoot() .. "r0.external/sdl2/src/joystick/windows/SDL_xinputjoystick.c",
        SrcRoot() .. "r0.external/sdl2/src/libm/e_atan2.c",
        SrcRoot() .. "r0.external/sdl2/src/libm/e_exp.c",
        SrcRoot() .. "r0.external/sdl2/src/libm/e_fmod.c",
        SrcRoot() .. "r0.external/sdl2/src/libm/e_log.c",
        SrcRoot() .. "r0.external/sdl2/src/libm/e_log10.c",
        SrcRoot() .. "r0.external/sdl2/src/libm/e_pow.c",
        SrcRoot() .. "r0.external/sdl2/src/libm/e_rem_pio2.c",
        SrcRoot() .. "r0.external/sdl2/src/libm/e_sqrt.c",
        SrcRoot() .. "r0.external/sdl2/src/libm/k_cos.c",
        SrcRoot() .. "r0.external/sdl2/src/libm/k_rem_pio2.c",
        SrcRoot() .. "r0.external/sdl2/src/libm/k_sin.c",
        SrcRoot() .. "r0.external/sdl2/src/libm/k_tan.c",
        SrcRoot() .. "r0.external/sdl2/src/libm/s_atan.c",
        SrcRoot() .. "r0.external/sdl2/src/libm/s_copysign.c",
        SrcRoot() .. "r0.external/sdl2/src/libm/s_cos.c",
        SrcRoot() .. "r0.external/sdl2/src/libm/s_fabs.c",
        SrcRoot() .. "r0.external/sdl2/src/libm/s_floor.c",
        SrcRoot() .. "r0.external/sdl2/src/libm/s_scalbn.c",
        SrcRoot() .. "r0.external/sdl2/src/libm/s_sin.c",
        SrcRoot() .. "r0.external/sdl2/src/libm/s_tan.c",
        SrcRoot() .. "r0.external/sdl2/src/loadso/windows/SDL_sysloadso.c",
        SrcRoot() .. "r0.external/sdl2/src/locale/SDL_locale.c",
        SrcRoot() .. "r0.external/sdl2/src/locale/windows/SDL_syslocale.c",
        SrcRoot() .. "r0.external/sdl2/src/misc/SDL_url.c",
        SrcRoot() .. "r0.external/sdl2/src/misc/windows/SDL_sysurl.c",
        SrcRoot() .. "r0.external/sdl2/src/power/SDL_power.c",
        SrcRoot() .. "r0.external/sdl2/src/power/windows/SDL_syspower.c",
        SrcRoot() .. "r0.external/sdl2/src/render/direct3d11/SDL_shaders_d3d11.c",
        SrcRoot() .. "r0.external/sdl2/src/render/direct3d/SDL_render_d3d.c",
        SrcRoot() .. "r0.external/sdl2/src/render/direct3d11/SDL_render_d3d11.c",
        SrcRoot() .. "r0.external/sdl2/src/render/direct3d/SDL_shaders_d3d.c",
        SrcRoot() .. "r0.external/sdl2/src/render/opengl/SDL_render_gl.c",
        SrcRoot() .. "r0.external/sdl2/src/render/opengl/SDL_shaders_gl.c",
        SrcRoot() .. "r0.external/sdl2/src/render/opengles2/SDL_render_gles2.c",
        SrcRoot() .. "r0.external/sdl2/src/render/opengles2/SDL_shaders_gles2.c",
        SrcRoot() .. "r0.external/sdl2/src/render/SDL_d3dmath.c",
        SrcRoot() .. "r0.external/sdl2/src/render/SDL_render.c",
        SrcRoot() .. "r0.external/sdl2/src/render/SDL_yuv_sw.c",
        SrcRoot() .. "r0.external/sdl2/src/render/software/SDL_blendfillrect.c",
        SrcRoot() .. "r0.external/sdl2/src/render/software/SDL_blendline.c",
        SrcRoot() .. "r0.external/sdl2/src/render/software/SDL_blendpoint.c",
        SrcRoot() .. "r0.external/sdl2/src/render/software/SDL_drawline.c",
        SrcRoot() .. "r0.external/sdl2/src/render/software/SDL_drawpoint.c",
        SrcRoot() .. "r0.external/sdl2/src/render/software/SDL_render_sw.c",
        SrcRoot() .. "r0.external/sdl2/src/render/software/SDL_rotate.c",
        SrcRoot() .. "r0.external/sdl2/src/SDL.c",
        SrcRoot() .. "r0.external/sdl2/src/SDL_assert.c",
        SrcRoot() .. "r0.external/sdl2/src/SDL_dataqueue.c",
        SrcRoot() .. "r0.external/sdl2/src/SDL_error.c",
        SrcRoot() .. "r0.external/sdl2/src/SDL_hints.c",
        SrcRoot() .. "r0.external/sdl2/src/SDL_log.c",
        SrcRoot() .. "r0.external/sdl2/src/sensor/dummy/SDL_dummysensor.c",
        SrcRoot() .. "r0.external/sdl2/src/sensor/SDL_sensor.c",
        SrcRoot() .. "r0.external/sdl2/src/sensor/windows/SDL_windowssensor.c",
        SrcRoot() .. "r0.external/sdl2/src/stdlib/SDL_crc32.c",
        SrcRoot() .. "r0.external/sdl2/src/stdlib/SDL_getenv.c",
        SrcRoot() .. "r0.external/sdl2/src/stdlib/SDL_iconv.c",
        SrcRoot() .. "r0.external/sdl2/src/stdlib/SDL_malloc.c",
        SrcRoot() .. "r0.external/sdl2/src/stdlib/SDL_qsort.c",
        SrcRoot() .. "r0.external/sdl2/src/stdlib/SDL_stdlib.c",
        SrcRoot() .. "r0.external/sdl2/src/stdlib/SDL_string.c",
        SrcRoot() .. "r0.external/sdl2/src/stdlib/SDL_strtokr.c",
        SrcRoot() .. "r0.external/sdl2/src/thread/generic/SDL_syscond.c",
        SrcRoot() .. "r0.external/sdl2/src/thread/SDL_thread.c",
        SrcRoot() .. "r0.external/sdl2/src/thread/windows/SDL_syscond_srw.c",
        SrcRoot() .. "r0.external/sdl2/src/thread/windows/SDL_sysmutex.c",
        SrcRoot() .. "r0.external/sdl2/src/thread/windows/SDL_syssem.c",
        SrcRoot() .. "r0.external/sdl2/src/thread/windows/SDL_systhread.c",
        SrcRoot() .. "r0.external/sdl2/src/thread/windows/SDL_systls.c",
        SrcRoot() .. "r0.external/sdl2/src/timer/SDL_timer.c",
        SrcRoot() .. "r0.external/sdl2/src/timer/windows/SDL_systimer.c",
        SrcRoot() .. "r0.external/sdl2/src/video/dummy/SDL_nullevents.c",
        SrcRoot() .. "r0.external/sdl2/src/video/dummy/SDL_nullframebuffer.c",
        SrcRoot() .. "r0.external/sdl2/src/video/dummy/SDL_nullvideo.c",
        SrcRoot() .. "r0.external/sdl2/src/video/SDL_blit.c",
        SrcRoot() .. "r0.external/sdl2/src/video/SDL_blit_0.c",
        SrcRoot() .. "r0.external/sdl2/src/video/SDL_blit_1.c",
        SrcRoot() .. "r0.external/sdl2/src/video/SDL_blit_A.c",
        SrcRoot() .. "r0.external/sdl2/src/video/SDL_blit_auto.c",
        SrcRoot() .. "r0.external/sdl2/src/video/SDL_blit_copy.c",
        SrcRoot() .. "r0.external/sdl2/src/video/SDL_blit_N.c",
        SrcRoot() .. "r0.external/sdl2/src/video/SDL_blit_slow.c",
        SrcRoot() .. "r0.external/sdl2/src/video/SDL_bmp.c",
        SrcRoot() .. "r0.external/sdl2/src/video/SDL_clipboard.c",
        SrcRoot() .. "r0.external/sdl2/src/video/SDL_egl.c",
        SrcRoot() .. "r0.external/sdl2/src/video/SDL_fillrect.c",
        SrcRoot() .. "r0.external/sdl2/src/video/SDL_pixels.c",
        SrcRoot() .. "r0.external/sdl2/src/video/SDL_rect.c",
        SrcRoot() .. "r0.external/sdl2/src/video/SDL_RLEaccel.c",
        SrcRoot() .. "r0.external/sdl2/src/video/SDL_shape.c",
        SrcRoot() .. "r0.external/sdl2/src/video/SDL_stretch.c",
        SrcRoot() .. "r0.external/sdl2/src/video/SDL_surface.c",
        SrcRoot() .. "r0.external/sdl2/src/video/SDL_video.c",
        SrcRoot() .. "r0.external/sdl2/src/video/SDL_vulkan_utils.c",
        SrcRoot() .. "r0.external/sdl2/src/video/SDL_yuv.c",
        SrcRoot() .. "r0.external/sdl2/src/video/windows/SDL_windowsclipboard.c",
        SrcRoot() .. "r0.external/sdl2/src/video/windows/SDL_windowsevents.c",
        SrcRoot() .. "r0.external/sdl2/src/video/windows/SDL_windowsframebuffer.c",
        SrcRoot() .. "r0.external/sdl2/src/video/windows/SDL_windowskeyboard.c",
        SrcRoot() .. "r0.external/sdl2/src/video/windows/SDL_windowsmessagebox.c",
        SrcRoot() .. "r0.external/sdl2/src/video/windows/SDL_windowsmodes.c",
        SrcRoot() .. "r0.external/sdl2/src/video/windows/SDL_windowsmouse.c",
        SrcRoot() .. "r0.external/sdl2/src/video/windows/SDL_windowsopengl.c",
        SrcRoot() .. "r0.external/sdl2/src/video/windows/SDL_windowsopengles.c",
        SrcRoot() .. "r0.external/sdl2/src/video/windows/SDL_windowsshape.c",
        SrcRoot() .. "r0.external/sdl2/src/video/windows/SDL_windowsvideo.c",
        SrcRoot() .. "r0.external/sdl2/src/video/windows/SDL_windowsvulkan.c",
        SrcRoot() .. "r0.external/sdl2/src/video/windows/SDL_windowswindow.c",
        SrcRoot() .. "r0.external/sdl2/src/video/yuv2rgb/yuv_rgb.c",
    }

    AddInclude_SDL2() 
    includedirs
    {
        SrcRoot() .. "r0.external/sdl2/src/hidapi/hidapi",
        SrcRoot() .. "r0.external/sdl2/src/video/khronos",
    }    

-- ------------------------------------------------------------------------------
function AddInclude_IMGUI()
    includedirs
    {
        SrcRoot() .. "r0.external/imgui/",
        SrcRoot() .. "r0.external/imgui/backends/",
        SrcRoot() .. "r0.external/imgui/freetype/",
        SrcRoot() .. "r0.external/imgui/misc/cpp",

        SrcRoot() .. "r0.external/freetype/include/",

        SrcRoot() .. "r0.external/imgui-ex/",
        SrcRoot() .. "r0.external/imgui-nodes/",
        SrcRoot() .. "r0.external/imgui-implot/"
    }
end

project "ext-imgui"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")

    defines 
    {
        "IMGUI_IMPL_OPENGL_LOADER_GLAD",
    }
    files
    {
        SrcRoot() .. "r0.external/imgui/*.cpp",
        SrcRoot() .. "r0.external/imgui/*.h",
        SrcRoot() .. "r0.external/imgui/freetype/*.cpp",
        SrcRoot() .. "r0.external/imgui/freetype/*.h",
        SrcRoot() .. "r0.external/imgui/backends/imgui_impl_sdl.*",
        SrcRoot() .. "r0.external/imgui/backends/imgui_impl_opengl3.*",

        SrcRoot() .. "r0.external/imgui/misc/cpp/*.*",

        SrcRoot() .. "r0.external/imgui-nodes/*.cpp",
        SrcRoot() .. "r0.external/imgui-nodes/*.h",

        SrcRoot() .. "r0.external/imgui-implot/*.cpp",
        SrcRoot() .. "r0.external/imgui-implot/*.h"
    }

    AddInclude_GLAD()
    AddInclude_SDL2()
    AddInclude_IMGUI()

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
function AddInclude_RTMIDI()
    includedirs
    {
        SrcRoot() .. "r0.external/rtmidi",
    }
end

project "ext-rtmidi"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")
    
    AddInclude_RTMIDI()
    files 
    { 
        SrcRoot() .. "r0.external/rtmidi/**.cpp",
        SrcRoot() .. "r0.external/rtmidi/**.h",
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
function AddInclude_OpenSSL()
    includedirs
    {
        SrcRoot() .. "r0.external/openssl-1.1.1j/x64/include"
    }
end

function AddOpenSSL()
    AddInclude_OpenSSL()
    libdirs
    {
        SrcRoot() .. "r0.external/openssl-1.1.1j/x64/lib"
    }
    links
    {
        "libcrypto.lib",
        "libssl.lib",
    }
    postbuildcommands { "copy $(SolutionDir)..\\..\\src\\r0.external\\openssl-1.1.1j\\x64\\bin\\*.dll $(TargetDir)" }
end

-- ------------------------------------------------------------------------------
function AddFreetype()
    libdirs
    {
        SrcRoot() .. "r0.external/freetype/win64"
    }
    links
    {
        "freetype.lib",
    }
    postbuildcommands { "copy $(SolutionDir)..\\..\\src\\r0.external\\freetype\\win64\\*.dll $(TargetDir)" }
end


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
    AddInclude_OpenSSL()
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

    defines 
    {
        "IMGUI_IMPL_OPENGL_LOADER_GLAD",
        
        "VST_2_4_EXTENSIONS",
        "VST_64BIT_PLATFORM=1",
    }

    AddInclude_OpenSSL()

    AddInclude_BROTLI()
    AddInclude_ZLIB()
    AddInclude_FLAC()
    AddInclude_SQLITE3()
    AddInclude_DRAGONBOX()
    AddInclude_OPUS()
    AddInclude_SODIUM()
    AddInclude_DPP()
    AddInclude_URIPARSER()
    AddInclude_PORTAUDIO()
    AddInclude_DATE_TZ()
    AddInclude_GLAD()
    AddInclude_PFOLD()
    AddInclude_SDL2()
    AddInclude_IMGUI()
    AddInclude_R8BRAIN()
    AddInclude_RTMIDI()
    AddInclude_FMT()
    AddInclude_FFT()
    AddInclude_CITY()
    AddInclude_STB()
    AddInclude_HTTPLIB()

    includedirs
    {
        SrcRoot() .. "r0.external/steinberg",
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
        SrcRoot() .. "r0.external/steinberg/**.h",
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

    AddFreetype()
    AddOpenSSL()

    links
    {
        "shlwapi",
        "ws2_32",
        "opengl32",
        "winmm",
        "dxguid.lib",
        "version.lib",
        "setupapi.lib",
        "imm32.lib",

        "ext-zlib",
        "ext-flac",
        "ext-flac-cpp",
        "ext-sqlite3",
        "ext-dragonbox",
        "ext-opus",
        "ext-sodium",
        "ext-dpp",
        "ext-uriparser",
        "ext-portaudio",
        "ext-date-tz",
        "ext-brotli",
        "ext-glad",
        "ext-pfold",
        "ext-imgui",
        "ext-r8brain",
        "ext-sdl",
        "ext-fmt",
        "ext-kissfft",
        "ext-cityhash",
        "ext-stb",

        "sdk"
    }

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
