
-- ------------------------------------------------------------------------------
ModuleRefInclude["httplib"] = function()

    includedirs
    {
        SrcDir() .. "r0.net/httplib",
    }
    defines
    {
        "CPPHTTPLIB_OPENSSL_SUPPORT",
        "CPPHTTPLIB_BROTLI_SUPPORT",
        "CPPHTTPLIB_ZLIB_SUPPORT"
    }
    ModuleRefInclude["openssl"]()
end

ModuleHeaderOnlyFiles["httplib"] = function()
    files 
    { 
        SrcDir() .. "r0.net/httplib/**.h",
    }
end

-- ------------------------------------------------------------------------------
ModuleRefInclude["cereal"] = function()

    includedirs
    {
        SrcDir() .. "r0.data/cereal/include",
        SrcDir() .. "r0.data/cereal/include/cereal_optional",
    }
end

ModuleHeaderOnlyFiles["cereal"] = function()
    files 
    { 
        SrcDir() .. "r0.data/cereal/**.h",
    }
end

-- ------------------------------------------------------------------------------
ModuleRefInclude["asio"] = function()

    externalincludedirs
    {
        SrcDir() .. "r0.net/asio/include"
    }

    ModuleRefInclude["openssl"]()

    defines 
    {
        "ASIO_STANDALONE",
        "ASIO_NO_DEFAULT_LINKED_LIBS",
    }
end

ModuleHeaderOnlyFiles["asio"] = function()
    files 
    { 
        SrcDir() .. "r0.net/asio/include/*.h",
    }
end


-- ------------------------------------------------------------------------------
ModuleRefInclude["websocketpp"] = function()

    externalincludedirs
    {
        SrcDir() .. "r0.net/websocketpp"
    }

    ModuleRefInclude["asio"]()

    filter "system:Windows"
    defines 
    {
        "_WEBSOCKETPP_CPP11_FUNCTIONAL_",
        "_WEBSOCKETPP_CPP11_SYSTEM_ERROR_",
        "_WEBSOCKETPP_CPP11_RANDOM_DEVICE_",
        "_WEBSOCKETPP_CPP11_MEMORY_",
        "_WEBSOCKETPP_CPP11_TYPE_TRAITS_",
    }
    filter {}

    filter "system:linux"
    defines 
    {
        "_WEBSOCKETPP_CPP11_STL_",
    }
    filter {}

    filter "system:macosx"
    defines 
    {
        "_WEBSOCKETPP_CPP11_STL_",
    }
    filter {}
end

ModuleHeaderOnlyFiles["websocketpp"] = function()
    files 
    { 
        SrcDir() .. "r0.net/websocketpp/websocketpp/**.h",
    }
end



-- ------------------------------------------------------------------------------

function addSimpleHeaderOnly( isExternal, moduleName, pathTo )

    if ( isExternal ) then
        ModuleRefInclude[moduleName] = function()
            externalincludedirs
            {
                SrcDir() .. pathTo,
            }
        end
    else
        ModuleRefInclude[moduleName] = function()
            includedirs
            {
                SrcDir() .. pathTo,
            }
        end
    end

    ModuleHeaderOnlyFiles[moduleName] = function()
        files
        {
            SrcDir() .. pathTo .. "/**.h",
            SrcDir() .. pathTo .. "/**.hpp",
        }
    end
end

addSimpleHeaderOnly( true,  "concurrent",       "r0.async/concurrent")
addSimpleHeaderOnly( true,  "taskflow",         "r0.async/taskflow/taskflow")
addSimpleHeaderOnly( true,  "utf8",             "r0.data/utf8")
addSimpleHeaderOnly( true,  "json",             "r0.data/json")
addSimpleHeaderOnly( true,  "trie",             "r0.data/trie/include")
addSimpleHeaderOnly( true,  "basen",            "r0.codec/basen")
addSimpleHeaderOnly( true,  "q_lib",            "r0.dsp/q_lib/include")
addSimpleHeaderOnly( true,  "sol",              "r0.scripting/sol-330")
addSimpleHeaderOnly( false, "pcg",              "r0.scaffold/pcg/include")

