
LibRoot.SimpleCpp = SrcDir() .. "r0.scaffold/simplecpp"

-- ==============================================================================
ModuleRefInclude["simplecpp"] = function()
    includedirs
    {
        LibRoot.SimpleCpp ,
    }
end

-- ==============================================================================
project "r0-simplecpp"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("r0")

    ModuleRefInclude["simplecpp"]()

    disablewarnings { "4267" } -- conversion from 'size_t' to 'unsigned int', possible loss of data

    files 
    { 
        LibRoot.SimpleCpp .. "/*.cpp",
        LibRoot.SimpleCpp .. "/*.h",
    }

ModuleRefLink["simplecpp"] = function()
    links { "r0-simplecpp" }
end
