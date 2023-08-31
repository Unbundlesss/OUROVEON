
LibRoot.Foxen = SrcDir() .. "r0.codec/foxen"

-- ==============================================================================
ModuleRefInclude["foxen"] = function()

    externalincludedirs ( LibRoot.Foxen .. "/include" )

end

project "r0-foxen"
    kind "StaticLib"
    language "C"

    disablewarnings { "4018", "4244" }

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("r0")

    ModuleRefInclude["foxen"]()

    files 
    {
        LibRoot.Foxen .. "/src/*.c"
    }


ModuleRefLink["foxen"] = function()
    links { "r0-foxen" }
end

