--   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
--  |       |   |   |   __ \       |   |   |    ___|       |    |  |
--  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
--  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|

-- ------------------------------------------------------------------------------
function SilenceMSVCSecurityWarnings()
    filter "system:Windows"
    defines 
    {
        -- shut up shut up shut up
        "_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES",
        "_CRT_NONSTDC_NO_WARNINGS",
        "_CRT_SECURE_NO_WARNINGS",
    }
    filter {}
end

-- ------------------------------------------------------------------------------
function SetDefaultBuildConfiguration()
    
    cppdialect "C++20"

    SilenceMSVCSecurityWarnings()

    filter "configurations:Debug"
        defines   { "DEBUG", "OURO_DEBUG=1", "OURO_RELEASE=0" }
        symbols   "On"
        ispcVars {
            GenerateDebugInformation = true,
            Opt         = "disabled",
            CPU         = "core2",
            TargetISA   = "sse2-i32x8",
        }
    filter {}

    filter "configurations:Release"
        defines   { "NDEBUG", "OURO_DEBUG=0", "OURO_RELEASE=1" }
        flags     { "LinkTimeOptimization" }
        optimize  "Full"
        ispcVars { 
            Opt         = "maximum",
            CPU         = "core2",
            TargetISA   = "sse2-i32x8",
        }
    filter {}

end

-- ------------------------------------------------------------------------------
function SetDefaultOutputDirectories(subgrp)

    targetdir   ( GetBuildRootToken() .. "_artefact/bin_" .. subgrp .. "/%{cfg.shortname}/%{prj.name}/" )
    objdir      ( "!" .. GetBuildRootToken() .. "_artefact/obj_" .. subgrp .. "/%{cfg.shortname}/%{prj.name}/" )
    debugdir    ( "$(OutDir)" )

end

-- ------------------------------------------------------------------------------
function AddPCH( sourceFile, headerFilePath, headerFile )
    pchsource ( sourceFile )
    if ( os.host() == "windows" ) then
        pchheader ( headerFile )
    else
        pchheader ( headerFilePath .. headerFile )
    end
end