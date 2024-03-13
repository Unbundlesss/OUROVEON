
LibRoot.Sqlite = SrcDir() .. "r0.data/sqlite3"

-- ==============================================================================
ModuleRefInclude["sqlite3"] = function()
    includedirs
    {
        LibRoot.Sqlite,
    }
    defines
    {
        "SQLITE_ENABLE_FTS5",
        "SQLITE_ENABLE_JSON1",
        "SQLITE_CORE"
    }
end

-- ==============================================================================
project "r0-sqlite3"
    kind "StaticLib"
    language "C"

    SetDefaultBuildConfiguration()
    SetDefaultOutputDirectories("r0")

    ModuleRefInclude["sqlite3"]()

    files
    {
        LibRoot.Sqlite .. "/**.c",
        LibRoot.Sqlite .. "/**.h",
    }

ModuleRefLink["sqlite3"] = function()
    links { "r0-sqlite3" }
end
