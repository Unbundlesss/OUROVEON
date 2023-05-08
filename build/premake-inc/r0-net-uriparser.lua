
LibRoot.UriParser = SrcDir() .. "r0.net/uriparser"

-- ==============================================================================
ModuleRefInclude["uriparser"] = function()
    externalincludedirs
    {
        LibRoot.UriParser .. "/include",
    }
    defines 
    {
        "URI_NO_UNICODE",
        "URI_STATIC_BUILD"
    }
end

-- ==============================================================================
project "r0-uriparser"
    kind "StaticLib"
    language "C"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("r0")

    ModuleRefInclude["uriparser"]()

    files
    {
        LibRoot.UriParser .. "/**.c",
        LibRoot.UriParser .. "/**.h",
    }

ModuleRefLink["uriparser"] = function()
    links { "r0-uriparser" }
end
