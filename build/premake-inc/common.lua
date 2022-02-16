
-- ------------------------------------------------------------------------------
function SetDefaultBuildConfiguration()
    
    cppdialect "C++20"

    filter "configurations:Debug"
        defines   { "DEBUG" }
        symbols   "On"
        ispcVars {
            GenerateDebugInformation = true,
            Opt         = "disabled",
            CPU         = "core2",
            TargetISA   = "sse2-i32x8",
        }

    filter "configurations:Release"
        defines   { "NDEBUG" }
        flags     { "LinkTimeOptimization" }
        optimize  "Full"

        ispcVars { 
            Opt         = "maximum",
            CPU         = "core2",
            TargetISA   = "sse2-i32x8",
        }


    filter "configurations:Release-AVX2"
        defines   { "NDEBUG" }
        flags     { "LinkTimeOptimization" }
        optimize  "Full"
        
        vectorextensions "AVX2"
        ispcVars  {
            Opt         = "maximum",
            CPU         = "core-avx2",
            TargetISA   = "avx2-i32x16",
        }

    filter {}

end

-- ------------------------------------------------------------------------------
function SetDefaultOutputDirectories(subgrp)

    targetdir   ( GetBuildRootToken() .. "_artefact/bin_" .. subgrp .. "/%{cfg.shortname}/%{prj.name}/" )
    objdir      ( GetBuildRootToken() .. "_artefact/obj_" .. subgrp .. "/%{cfg.shortname}/%{prj.name}/" )
    debugdir    ( "$(OutDir)" )

end

