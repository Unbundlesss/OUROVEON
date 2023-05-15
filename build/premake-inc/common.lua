--   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
--  |       |   |   |   __ \       |   |   |    ___|       |    |  |
--  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
--  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|

-- ------------------------------------------------------------------------------
function SetDefaultBuildConfiguration()
    
    conformancemode "on"
    cppdialect "C++20"

    filter "system:Windows"
        flags {
            "MultiProcessorCompile"     -- /MP on MSVC to locally distribute compile
        }
        buildoptions {
            "/Zc:__cplusplus",          -- enable __cplusplus compiler macro
            "/Zc:__STDC__",             -- as above so below
        }
        -- silence all non-portable MSVC security whining
        defines {
            "_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES=1",
            "_CRT_NONSTDC_NO_WARNINGS",
            "_CRT_SECURE_NO_WARNINGS",
        }
        -- https://learn.microsoft.com/en-us/cpp/c-runtime-library/compatibility?view=msvc-170
        defines {
            "_CRT_DECLARE_NONSTDC_NAMES=1",
        }
    filter {}

    filter "configurations:Debug"
        defines   { "DEBUG", "OURO_DEBUG=1", "OURO_RELEASE=0" }
        symbols   "FastLink"
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