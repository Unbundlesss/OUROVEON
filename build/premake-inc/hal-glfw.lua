
function Root_GLFW()
    return SrcRoot() .. "r0.hal/glfw/"
end
function Root_GLFW_Ext()
    return SrcRoot() .. "r0.hal/glfw_ext/"
end

-- ------------------------------------------------------------------------------
function _GLFW_Include()
    includedirs
    {
        Root_GLFW() .. "include",
        Root_GLFW_Ext() .. "include",
    }
    sysincludedirs
    {
        Root_GLFW() .. "deps",
    }

    filter "system:Windows"
    defines 
    {
        "_GLFW_WIN32",
        "_GLFW_USE_HYBRID_HPG",
    }
    filter {}

    filter "system:linux"
    defines 
    {
        "_GLFW_X11"
    }
    filter {}

    filter "system:macosx"
    defines 
    {
        "_GLFW_COCOA"
    }
    filter {}
end

ModuleRefInclude["glfw"] = _GLFW_Include

-- ==============================================================================
project "hal-glfw"
    kind "StaticLib"
    language "C"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("hal")
    
    ModuleRefInclude["glfw"]()

    local common_headers = {
        "src/internal.h",
        "src/mappings.h",
        "include/glfw/glfw3.h",
        "include/glfw/glfw3native.h",
    }
    local common_sources = {
        "src/context.c",
        "src/init.c",
        "src/input.c",
        "src/monitor.c",
        "src/vulkan.c",
        "src/window.c",
        "deps/glad_gl.c",
    }

    local windows_headers = {
        "src/win32_platform.h",
        "src/win32_joystick.h",
        "src/wgl_context.h",
        "src/egl_context.h",
        "src/osmesa_context.h",
    }
    local windows_sources = {
        "src/win32_init.c",
        "src/win32_joystick.c",
        "src/win32_monitor.c",
        "src/win32_time.c",
        "src/win32_thread.c",
        "src/win32_window.c",
        "src/wgl_context.c",
        "src/egl_context.c",
        "src/osmesa_context.c",
    }

    local linux_headers = {
        "src/x11_platform.h",
        "src/xkb_unicode.h",
        "src/posix_time.h",
        "src/posix_thread.h",
        "src/glx_context.h",
        "src/egl_context.h",
        "src/osmesa_context.h",
        "src/linux_joystick.h",
    }
    local linux_sources = {
        "src/x11_init.c",
        "src/x11_monitor.c",
        "src/x11_window.c",
        "src/xkb_unicode.c",
        "src/posix_time.c",
        "src/posix_thread.c",
        "src/glx_context.c",
        "src/egl_context.c",
        "src/osmesa_context.c",
        "src/linux_joystick.c",
    }

    local osx_headers = {
        "src/cocoa_platform.h",
        "src/cocoa_joystick.h",
        "src/posix_thread.h",
        "src/nsgl_context.h",
        "src/egl_context.h",
        "src/osmesa_context.h",
    }
    local osx_sources = {
        "src/cocoa_init.m",
        "src/cocoa_joystick.m",
        "src/cocoa_monitor.m",
        "src/cocoa_window.m",
        "src/cocoa_time.c",
        "src/posix_thread.c",
        "src/nsgl_context.m",
        "src/egl_context.c",
        "src/osmesa_context.c",
    }

    for k,v in pairs(common_headers) do 
        includedirs { Root_GLFW() .. v }
    end
    for k,v in pairs(common_sources) do 
        files       { Root_GLFW() .. v }
    end

    files ( Root_GLFW() .. "include/**.h" )
    files ( Root_GLFW_Ext() .. "**.h" )
    files ( Root_GLFW_Ext() .. "**.c" )

    filter "system:Windows"
    defines { "_CRT_SECURE_NO_WARNINGS" }
    for k,v in pairs(windows_headers) do 
        includedirs { Root_GLFW() .. v }
    end
    for k,v in pairs(windows_sources) do 
        files       { Root_GLFW() .. v }
    end
    filter {}

    filter "system:linux"
    for k,v in pairs(linux_headers) do 
        includedirs { Root_GLFW() .. v }
    end
    for k,v in pairs(linux_sources) do 
        files       { Root_GLFW() .. v }
    end
    filter {}

    filter "system:macosx"
    for k,v in pairs(osx_headers) do 
        includedirs { Root_GLFW() .. v }
    end
    for k,v in pairs(osx_sources) do 
        files       { Root_GLFW() .. v }
    end
    filter {}

function _GLFW_LinkProject()
    links { "hal-glfw" }
end
ModuleRefLink["glfw"] = _GLFW_LinkProject