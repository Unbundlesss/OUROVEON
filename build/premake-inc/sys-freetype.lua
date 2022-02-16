
-- ------------------------------------------------------------------------------
function _Freetype_Include()

    filter "system:Windows"
    includedirs
    {
        SrcRoot() .. "r0.sys/freetype/include/",
    }
    filter {}

    filter "system:linux"
    sysincludedirs
    {
        "/usr/include/freetype2/",
    }
    filter {}

    filter "system:macosx"
    sysincludedirs
    {
        GetHomebrewDir() .. "freetype/include/freetype2/",
    }
    filter {}
end

ModuleRefInclude["freetype"] = _Freetype_Include

-- ==============================================================================
function _Freetype_LinkPrebuilt()

    filter "system:Windows"
    -- https://github.com/ubawurinna/freetype-windows-binaries/releases/tag/v2.11.1
    libdirs
    {
        SrcRoot() .. "r0.sys/freetype/lib/win64_release_dll"
    }
    links
    {
        "freetype.lib",
    }
    postbuildcommands { "copy $(SolutionDir)..\\..\\" .. GetSourceDir() .. "\\r0.sys\\freetype\\lib\\win64_release_dll\\*.dll $(TargetDir)" }
    filter {}

    filter "system:linux"
    links
    {
        "freetype",
    }
    filter {}

    filter "system:macosx"
    libdirs
    {
        GetHomebrewDir() .. "freetype/lib"
    }
    links
    {
        "freetype",
    }
    filter {}
end

ModuleRefLink["freetype"] = _Freetype_LinkPrebuilt