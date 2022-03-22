

-- ==============================================================================
ModuleRefInclude["superluminal"] = function()

    if ( os.host() == "windows" ) then
        defines 
        {
            "OURO_FEATURE_SUPERLUMINAL"
        }
        includedirs
        {
            GetPrebuiltLibs_Win64() .. "superluminal/include",
        }
    end

end

-- ==============================================================================
ModuleRefLinkWin["superluminal"] = function()

    libdirs
    {
        GetPrebuiltLibs_Win64() .. "superluminal/lib/x64"
    }

    filter "configurations:Debug"
        links ( "PerformanceAPI_MDd.lib" )
    filter "configurations:Release"
        links ( "PerformanceAPI_MD.lib" )
    filter {}
end
