
LibRoot.Abseil = SrcDir() .. "r0.ext-scaffold/abseil"

-- ------------------------------------------------------------------------------
ModuleRefInclude["abseil"] = function()
    includedirs
    {
        LibRoot.Abseil .. "/",
    }
end

-- ==============================================================================
project "ext-abseil"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("ext")

    ModuleRefInclude["abseil"]()

    -- https://abseil.io/docs/cpp/platforms/compilerflags

    filter "system:macosx"
    defines 
    {
        "ABSL_USING_CLANG",
    }
    filter {}

    filter "system:windows"
    warnings "default"
    disablewarnings { "4005", "4068", "4244", "4267", "4800" }
    defines 
    {
        "WIN32_LEAN_AND_MEAN",
        "NOMINMAX",
    }
    filter {}

    -- this is a bit goofy but absl weaves test and benchmark code so 
    -- we have some generic string search to root them out
    local abslcode = os.matchfiles( LibRoot.Abseil .. "/absl/**.cc" )
    for idx,val in pairs(abslcode) do
        if string.match( val, "_test" ) or 
           string.match( val, "_benchmark" ) or
           string.match( val, "benchmarks.cc" ) then
        else
            files( val )
        end
    end

    files
    {
        LibRoot.Abseil .. "/absl/**.h",
    }

ModuleRefLink["abseil"] = function()
    links { "ext-abseil" }
end
