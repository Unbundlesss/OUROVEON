//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  

#include "pch.h"

#include "base/fio.h"

#include <cerrno>
#include <fstream>

namespace base
{
    absl::Status readBinaryFile( const fs::path& path, TBinaryFileBuffer& contents )
    {
        if ( std::ifstream stream{ path, std::ios::in | std::ios::binary } )
        {
            stream.seekg( 0, std::ios::end );
            const auto size = stream.tellg();
            stream.seekg( 0, std::ios::beg );

            contents.resize( size );

            stream.read( reinterpret_cast< char* >( std::data( contents ) ), std::size( contents ) );

            return absl::OkStatus();
        }

        return absl::Status( absl::ErrnoToStatusCode( errno ), fmt::format( FMTX( "Failed to open binary file '{}': {}." ), path.string(), std::strerror(errno)));
    }

    absl::StatusOr< TBinaryFileBuffer > readBinaryFile( const fs::path& path )
    {
        TBinaryFileBuffer contents;
        if ( const auto status = readBinaryFile( path, contents ); !status.ok() )
        {
            return status;
        }
        return absl::StatusOr< TBinaryFileBuffer >( std::move( contents ) );
    }

    absl::Status readTextFile( const fs::path& path, TTextFileBuffer& contents )
    {
        if ( std::ifstream stream{ path, std::ios::in } )
        {
            stream.seekg( 0, std::ios::end );
            const auto size = stream.tellg();
            stream.seekg( 0, std::ios::beg );

            contents.resize( size );

            stream.read( std::data( contents ), std::size( contents ) );

            // trim back down any trailing null bytes; CRLF conversion will mean that the tellg() value reported
            // above may be quite a bit larger than the result that is read in.
            const auto contentsLength = strlen( contents.data() );
            ABSL_ASSERT( contents.size() >= contentsLength );
            contents.resize( contentsLength ); // keep a remaining terminator

            return absl::OkStatus();
        }

        return absl::Status( absl::ErrnoToStatusCode( errno ), fmt::format( FMTX( "Failed to open text file '{}': {}." ), path.string(), std::strerror( errno ) ) );
    }

    absl::StatusOr< TTextFileBuffer > readTextFile( const fs::path& path )
    {
        TTextFileBuffer contents;
        if ( const auto status = readTextFile( path, contents ); !status.ok() )
        {
            return status;
        }
        return absl::StatusOr< TTextFileBuffer >( std::move( contents ) );
    }
} // namespace base
