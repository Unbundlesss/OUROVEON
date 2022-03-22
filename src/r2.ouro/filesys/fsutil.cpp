//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"

namespace filesys {

// ---------------------------------------------------------------------------------------------------------------------
bool recursiveSearchBackwards( const fs::path& startPath, const char* filenameToFind, fs::path& resultPath )
{
    // hunt backwards to the drive root from startPath
    fs::path searchPath = startPath;
    for ( ;; )
    {
        searchPath /= filenameToFind;

        if ( fs::exists( searchPath ) )
        {
            resultPath = searchPath;

            return true;
        }

        searchPath = searchPath.parent_path();
        if ( searchPath == searchPath.root_path() )
            return false;

        if ( searchPath.has_parent_path() )
            searchPath = searchPath.parent_path();
        else
            return false;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
bool ensureDirectoryExists( const fs::path& path )
{
    if ( fs::exists( path ) )
    {
        return true;
    }
    else
    {
        try
        {
            fs::create_directories( path );
        }
        catch ( fs::filesystem_error& fsE )
        {
            // TODO return fsE.what instead of dumping to the log
            blog::error::core( fsE.what() );
            return false;
        }
    }

    if ( !fs::exists( path ) )
    {
        blog::error::core( "could not validate created directory" );
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
bool appendAndCreateSubDir( fs::path& path, const char* suffix )
{
    fs::path startPath( path );

    const auto extendedPathFs = ( startPath / suffix );
    if ( !fs::exists( extendedPathFs ) )
    {
        try
        {
            fs::create_directories( extendedPathFs );
        }
        catch ( fs::filesystem_error& fsE )
        {
            blog::error::cfg( "unable to create directory [{}], {}", extendedPathFs.string(), fsE.what() );
            return false;
        }
    }

    if ( !fs::is_directory( extendedPathFs ) )
    {
        blog::error::cfg( "cannot create / validate local config path '{}'", extendedPathFs.string() );
        return false;
    }

    path = extendedPathFs.string();
    return true;
}

} // namespace filesys

