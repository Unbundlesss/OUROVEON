
-- ------------------------------------------------------------------------------
function _OpenSSL_Include()

    filter "system:Windows"
    sysincludedirs
    {
        SrcRoot() .. "r0.sys/openssl-1.1.1j/win64/include"
    }
    filter {}

    filter "system:macosx"
    sysincludedirs
    {
        GetHomebrewDir() .. "openssl@1.1/include/",
    }
    filter {}
end

ModuleRefInclude["openssl"] = _OpenSSL_Include

-- ==============================================================================
function _OpenSSL_LinkPrebuilt()

    filter "system:Windows"
    libdirs
    {
        SrcRoot() .. "r0.sys/openssl-1.1.1j/win64/lib"
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
        GetHomebrewDir() .. "openssl@1.1//lib"
    }
    links
    {
        "ssl",
        "crypto",
    }
    filter {}
end

ModuleRefLink["openssl"] = _OpenSSL_LinkPrebuilt