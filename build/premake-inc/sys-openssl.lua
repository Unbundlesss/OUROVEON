
-- ------------------------------------------------------------------------------
function _OpenSSL_Include()

    filter "system:Windows"
    sysincludedirs
    {
        SrcDir() .. "r0.sys/openssl-1.1.1j/win64/include"
    }
    filter {}

    filter "system:macosx"
    sysincludedirs
    {
        GetMacOSPackgesDir() .. "openssl@1.1/include/",
    }
    filter {}
end

ModuleRefInclude["openssl"] = _OpenSSL_Include

-- ==============================================================================
function _OpenSSL_LinkPrebuilt()

    filter "system:Windows"
    libdirs
    {
        SrcDir() .. "r0.sys/openssl-1.1.1j/win64/lib"
    }
    links
    {
        "libcrypto.lib",
        "libssl.lib",
    }
    postbuildcommands { "copy $(SolutionDir)..\\..\\" .. GetSourceDir() .. "\\r0.sys\\openssl-1.1.1j\\win64\\bin\\*.dll $(TargetDir)" }
    filter {}

    filter "system:linux"
    links
    {
        "ssl",
        "crypto",
    }
    filter {}

    filter "system:macosx"
    libdirs
    {
        GetMacOSFatLibs()
    }
    links
    {
        "ssl",
        "crypto",
    }
    filter {}
end

ModuleRefLink["openssl"] = _OpenSSL_LinkPrebuilt