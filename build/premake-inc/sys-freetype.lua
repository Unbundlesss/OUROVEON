-- ==============================================================================
ModuleRefInclude["freetype"] = function()

    filter "system:Windows"
    externalincludedirs
    {
        GetPrebuiltLibs_Win64() .. "freetype/include/",
    }
    filter {}

    filter "system:linux"
    externalincludedirs
    {
        "/usr/include/freetype2/",
    }
    filter {}

    filter "system:macosx"
    externalincludedirs
    {
        GetMacOSPackgesDir() .. "freetype/include/freetype2/",
    }
    filter {}
end

-- ==============================================================================
ModuleRefLink["freetype"] = function()
    
    -- windows has debug/release static builds
    filter "system:Windows"
        links ( "freetype.lib" )
        filter { "system:Windows", "configurations:Release" }
            libdirs ( GetPrebuiltLibs_Win64() .. "freetype/lib/release_static" )
        filter { "system:Windows", "configurations:Debug" }
            libdirs ( GetPrebuiltLibs_Win64() .. "freetype/lib/debug_static" )
    filter {}

    -- 
    filter "system:linux"
        links ( "freetype" )
    filter {}

    -- 
    filter "system:macosx"
        libdirs ( GetPrebuiltLibs_MacUniversal() )
        links
        {
            "freetype",
            "png",
            "bz2",
        }
    filter {}
end
