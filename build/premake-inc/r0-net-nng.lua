
LibRoot.NNG = SrcDir() .. "r0.net/nng"

-- ==============================================================================
ModuleRefInclude["nng"] = function()
    includedirs
    {
        LibRoot.NNG .. "/include",
    }
    defines 
    {
        "NNG_STATIC_LIB",
        "URI_STATIC_BUILD",
        "NNG_ENABLE_STATS",
        "NNG_HAVE_STRNLEN=1",
        "NNG_HAVE_CONDVAR=1",
        "NNG_HAVE_SNPRINTF=1",
        "NNG_HAVE_BUS0",
        "NNG_HAVE_PAIR0",
        "NNG_HAVE_PAIR1",
        "NNG_HAVE_PUSH0",
        "NNG_HAVE_PULL0",
        "NNG_HAVE_PUB0",
        "NNG_HAVE_SUB0",
        "NNG_HAVE_REQ0",
        "NNG_HAVE_REP0",
        "NNG_HAVE_SURVEYOR0",
        "NNG_HAVE_RESPONDENT0",
        "NNG_TRANSPORT_INPROC",
        "NNG_TRANSPORT_IPC",
        "NNG_TRANSPORT_TCP",
        "NNG_TRANSPORT_WS",
        "NNG_SUPP_HTTP",
        "NNG_STATIC_LIB",
        "NNG_PRIVATE",
    }

    filter "system:Windows"
    defines 
    {
        "NNG_PLATFORM_WINDOWS",
        "_CRT_RAND_S",
        "_CRT_RAND_S",
    }
    filter {}

    filter "system:linux"
    defines 
    {
        "NNG_PLATFORM_POSIX",
        "NNG_PLATFORM_LINUX",
        "NNG_USE_EVENTFD",
        "NNG_HAVE_ABSTRACT_SOCKETS",
    }
    filter {}

    filter "system:macosx"
    defines 
    {
        "NNG_PLATFORM_POSIX",
        "NNG_PLATFORM_DARWIN",
    }
    filter {}
end

-- ==============================================================================
function addFilesExcludeTests( rootpath )
    local foundfiles = os.matchfiles( rootpath )
    for idx,val in pairs(foundfiles) do
        if string.match( val, "_test" ) or 
        string.match( val, "_benchmark" ) then
        else
            files( val )
        end
    end
end

-- ==============================================================================
project "r0-nng"
    kind "StaticLib"
    language "C"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("r0")

    ModuleRefInclude["nng"]()

    includedirs
    {
        LibRoot.NNG .. "/src",
    }

    addFilesExcludeTests( LibRoot.NNG .. "/src/core/**.c" )
    addFilesExcludeTests( LibRoot.NNG .. "/src/sp/**.c" )
    addFilesExcludeTests( LibRoot.NNG .. "/src/supplemental/**.c" )
    files
    {
        LibRoot.NNG .. "/src/core/**.h",
        LibRoot.NNG .. "/src/sp/**.h",
        LibRoot.NNG .. "/src/supplemental/**.h",
    }
    excludes
    {
        LibRoot.NNG .. "/src/sp/transport/tls/**.*",
        LibRoot.NNG .. "/src/sp/transport/zerotier/**.*",
        LibRoot.NNG .. "/src/supplemental/tls/**.*",
        LibRoot.NNG .. "/src/supplemental/websocket/stub.*",
    }

    filter "system:Windows"
    files
    {
        LibRoot.NNG .. "/src/platform/windows/**.c",
        LibRoot.NNG .. "/src/platform/windows/**.h",
    }
    filter {}

    filter "system:linux"
    files
    {
        LibRoot.NNG .. "/src/platform/posix/**.c",
        LibRoot.NNG .. "/src/platform/posix/**.h",
    }
    filter {}
    filter "system:macosx"
    files
    {
        LibRoot.NNG .. "/src/platform/posix/**.c",
        LibRoot.NNG .. "/src/platform/posix/**.h",
    }
    filter {}

ModuleRefLink["nng"] = function()
    links { "r0-nng" }
end
