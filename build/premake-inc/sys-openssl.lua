
-- ------------------------------------------------------------------------------
function _OpenSSL_Include()

    filter "system:Windows"
    sysincludedirs
    {
        GetPrebuiltLibs_Win64() .. "openssl/include"
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
    libdirs ( GetPrebuiltLibs_Win64() .. "openssl/lib" )
    links
    {
        "libcrypto-1_1.lib",
        "libssl-1_1.lib",
    }
    postbuildcommands { "copy \"" .. GetPrebuiltLibs_Win64_VSMacro() .. "openssl\\bin\\*.dll\" \"$(TargetDir)\"" }
    filter {}

    filter "system:linux"
    links
    {
        "ssl",
        "crypto",
    }
    filter {}

    filter "system:macosx"
    libdirs ( GetPrebuiltLibs_MacUniversal() )
    links
    {
        "ssl",
        "crypto",
    }
    filter {}
end

ModuleRefLink["openssl"] = _OpenSSL_LinkPrebuilt