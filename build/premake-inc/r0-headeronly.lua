
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

function addSimpleHeaderOnly( moduleName, pathTo )
    ModuleRefInclude[moduleName] = function()
        includedirs
        {
            SrcDir() .. pathTo,
        }
    end
    ModuleHeaderOnlyFiles[moduleName] = function()
        files
        {
            SrcDir() .. pathTo .. "/**.h",
            SrcDir() .. pathTo .. "/**.hpp",
        }
    end
end

addSimpleHeaderOnly("concurrent",   "r0.async/concurrent")
addSimpleHeaderOnly("taskflow",     "r0.async/taskflow")
addSimpleHeaderOnly("utf8",         "r0.data/utf8")
addSimpleHeaderOnly("json",         "r0.data/json")
addSimpleHeaderOnly("basen",        "r0.codec/basen")

