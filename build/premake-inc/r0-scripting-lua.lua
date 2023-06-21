
function Root_LUA()
    return SrcDir() .. "r0.scripting/lua-546/src/"
end

-- ==============================================================================
ModuleRefInclude["lua"] = function()
    includedirs
    {
        Root_LUA(),
    }
end

-- ==============================================================================
project "r0-lua"
    kind "StaticLib"
    language "C"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("r0")
    
    ModuleRefInclude["lua"]()

    defines
    {
    }

    files 
    { 
        Root_LUA() .. "*.c",
        Root_LUA() .. "*.h",
    }
    removefiles { Root_LUA() .. "luac.c" }

    
ModuleRefLink["lua"] = function()
    links { "r0-lua" }
end