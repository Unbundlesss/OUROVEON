
LibRoot.DPP = SrcDir() .. "r0.net/dpp"

-- ==============================================================================
ModuleRefInclude["dpp"] = function()
    externalincludedirs
    {
        LibRoot.DPP .. "/include",
    }
    defines { "DPP_ENABLE_VOICE" }
end

-- ==============================================================================
project "r0-dpp"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("r0")

    defines { "_WINSOCK_DEPRECATED_NO_WARNINGS" }

    ModuleRefInclude["dpp"]()

    ModuleRefInclude["json"]()
    ModuleRefInclude["zlib"]()
    ModuleRefInclude["brotli"]()
    ModuleRefInclude["opus"]()
    ModuleRefInclude["sodium"]()
    ModuleRefInclude["fmt"]()
    ModuleRefInclude["httplib"]()

    files
    {
        LibRoot.DPP .. "/src/**.cpp",
        LibRoot.DPP .. "/include/**.h",
    }
    AddPCH(
        "../../src/r0.net/dpp/src/dpp_pch.cpp",
        SrcDir() .. "r0.net/dpp/include/",
        "dpp_pch.h" )


ModuleRefLink["dpp"] = function()
    links { "r0-dpp" }
end

