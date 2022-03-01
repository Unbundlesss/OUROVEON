

-- ------------------------------------------------------------------------------
function _Superluminal_Include()

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

ModuleRefInclude["superluminal"] = _Superluminal_Include


-- ------------------------------------------------------------------------------
function _Superluminal_LinkPrebuilt()

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

ModuleRefLinkWin["superluminal"]      = _Superluminal_LinkPrebuilt