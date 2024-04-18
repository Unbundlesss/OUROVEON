
LibRoot.AbletonLink = SrcDir() .. "r0.net/link"

-- ==============================================================================
ModuleRefInclude["link"] = function()
    externalincludedirs
    {
        LibRoot.AbletonLink .. "/include",
    }

    filter "system:Windows"
        defines 
        {
            "LINK_PLATFORM_WINDOWS=1",
        }
    filter {}
    
    filter "system:linux"
        defines 
        {
            "LINK_PLATFORM_LINUX=1",
        }
    filter {}

    filter "system:macosx"
        defines 
        {
            "LINK_PLATFORM_MACOSX=1",
        }
    filter {}

end

-- ==============================================================================
project "r0-link"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("r0")

    ModuleRefInclude["link"]()
    ModuleRefInclude["asio"]()

    files
    {
        LibRoot.AbletonLink .. "/include/**.hpp",
        LibRoot.AbletonLink .. "/src/**.cpp",
    }

ModuleRefLink["link"] = function()
    links { "r0-link" }
end
