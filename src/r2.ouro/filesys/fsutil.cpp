//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
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
absl::Status ensureDirectoryExists( const fs::path& path )
{
    std::error_code stdError;

    if ( fs::exists( path, stdError ) )
    {
        return absl::OkStatus();
    }
    else
    {
        // we use try/catch here due to the dumb way that msvc currently handles return values from create_directories
        // https://developercommunity.visualstudio.com/t/stdfilesystemcreate-directories-returns-false-if-p/278829
        try
        {
            fs::create_directories( path );
        }
        catch ( fs::filesystem_error& fsE )
        {
            return absl::UnknownError( fsE.what() );
        }
    }

    if ( !fs::exists( path, stdError ) )
    {
        return absl::NotFoundError( fmt::format("could not validate created directory (err:{})", stdError.message() ) );
    }

    return absl::OkStatus();
}

} // namespace filesys

