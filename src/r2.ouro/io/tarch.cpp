//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#include "pch.h"

#include "base/utils.h"
#include "filesys/fsutil.h"
#include "io/tarch.h"

namespace io {


struct THeader
{
    enum class FileType : char
    {
        NORMAL      = '0',
        HARDLINK    = '1',
        SYMLINK     = '2',
        CHAR        = '3',
        BLOCK       = '4',
        DIRECTORY   = '5',
        FIFO        = '6',
        CONTIGUOUS  = '7',
    };

    union
    {
        union
        {
            // Pre-POSIX.1-1988 format
            struct
            {
                char name[100];             // file name
                char mode[8];               // permissions
                char uid[8];                // user id (octal)
                char gid[8];                // group id (octal)
                char size[12];              // size (octal)
                char mtime[12];             // modification time (octal)
                char check[8];              // sum of unsigned characters in block, with spaces in the check field while calculation is done (octal)
                FileType link;              // link indicator
                char link_name[100];        // name of linked file
            };

            // UStar format (POSIX IEEE P1003.1)
            struct
            {
                char old[156];              // first 156 octets of Pre-POSIX.1-1988 format
                FileType type;              // file type
                char also_link_name[100];   // name of linked file
                char ustar[8];              // ustar\000
                char owner[32];             // user name (string)
                char group[32];             // group name (string)
                char major[8];              // device major number
                char minor[8];              // device minor number
                char prefix[155];
            };
        };

        char block[512];                    // raw memory (500 octets of actual data, padded to 1 block)
    };

    constexpr uint32_t sumBlockData() const 
    {
        // sum of entire metadata
        uint32_t checksum = 0;
        for ( int i = 0; i < 512; i++ )
        {
            checksum += (uint32_t)block[i];
        }
        return checksum;
    }

    void computeChecksum()
    {
        // use spaces for the checksum bytes while calculating the checksum
        memset( check, ' ', 8 );

        const uint32_t checksum = sumBlockData();
        snprintf( check, sizeof( check ), "%06o0", checksum );

        check[6] = '\0';
        check[7] = ' ';
    }

    // create a default header setup with sensible empty defaults
    static void createDefault( THeader& result )
    {
        memset( &result, 0, sizeof( THeader ) );

        strcpy( result.mode, "0000777" );
        strcpy( result.uid,  "0000000" );
        strcpy( result.gid,  "0000000" );
        strcpy( result.size, "00000000000" );

        memcpy( result.ustar, "ustar  \x00", 8 );
    }
};

static_assert(sizeof( THeader ) == 512, "size of THeader should be exactly 512 bytes");

template< std::size_t inputSize >
inline constexpr uint32_t oct2uint( const char (&oct)[inputSize] )
{
    uint32_t out = 0;
    std::size_t i = 0;
    while ( ( i < inputSize - 1 ) && oct[i] )
    {
        out = (out << 3) | (uint32_t)(oct[i++] - '0');
    }
    return out;
}



absl::Status archiveFilesInDirectoryToTAR(
    const std::filesystem::path& inputPath,
    const std::filesystem::path& outputTarFile,
    const ArchiveProgressCallback& archivingProgressFunction )
{
    // allocate a buffer to use as staging for read/writing from the input files to the output stream
    static constexpr std::size_t ioBufferSize = 1 * 1024 * 1024;
    void* ioBuffer = mem::alloc16<char>( ioBufferSize );

    FILE* tarOutputFile = fopen( outputTarFile.string().c_str(), "wb" );

    // auto close tar file and free the ioBuffer memory on scope exit
    absl::Cleanup cleanupOnScopeExit = [&]() noexcept
    {
        fclose( tarOutputFile );
        tarOutputFile = nullptr;

        mem::free16( ioBuffer );
        ioBuffer = nullptr;
        };


    const fs::path baseInputPath = inputPath.parent_path();


    // keep track of bytes processed so the UI can show something happening
    std::size_t bytesProcessedIntoTar = 0;
    std::size_t filesProcessedIntoTar = 0;
    if ( archivingProgressFunction != nullptr )
        archivingProgressFunction( 0, 0 );

    // create a blob of zeroed memory we use as padding to 512-byte edges at the end of files and the
    // end of the whole tar stream
    char ioPadding[512];
    memset( ioPadding, 0, 512 );

    const auto appendToTar = [&](
        const bool bIsDirectory,
        const fs::path& entryFullFilename,
        const fs::path& entryRelativePath ) noexcept -> absl::Status
        {
            THeader tarHeader;
            THeader::createDefault( tarHeader );

            std::error_code lwtError;
            const auto fileModTime          = fs::last_write_time( entryFullFilename, lwtError );
#if OURO_PLATFORM_WIN
            const auto fileModSystemTime    = std::chrono::utc_clock::to_sys( std::chrono::file_clock::to_utc( fileModTime ) );
#else
            const auto fileModSystemTime    = std::chrono::file_clock::to_sys( fileModTime );
#endif
            const auto fileModTimeT         = std::chrono::system_clock::to_time_t(
#if OURO_PLATFORM_OSX
                                                                                   std::chrono::time_point_cast<std::chrono::microseconds>(
#endif
                                                                                   fileModSystemTime
#if OURO_PLATFORM_OSX
                                                                                   )
#endif
                                                                                   );
            
            snprintf( tarHeader.mtime, sizeof( tarHeader.mtime ), "%011o", (int32_t)fileModTimeT );

            // get output name from relative path, ensure all separators are forward-slash
            std::string entryNamePath = entryRelativePath.string();
            std::replace( entryNamePath.begin(), entryNamePath.end(), '\\', '/' );

            if ( bIsDirectory )
            {
                entryNamePath += '/';

                tarHeader.link = tarHeader.type = THeader::FileType::DIRECTORY;
            }
            else
            {
                std::error_code fileSizeError;
                const std::uintmax_t fileSize = fs::file_size( entryFullFilename, fileSizeError );

                if ( fileSizeError )
                {
                    return absl::InternalError( fmt::format( FMTX( "error ({}) trying to get file size for [{}]" ), fileSizeError.message(), entryFullFilename.string() ) );
                }
                snprintf( tarHeader.size, sizeof( tarHeader.size ), "%011o", (int32_t)fileSize );

                tarHeader.link = tarHeader.type = THeader::FileType::NORMAL;
            }

            // copy in name
            strncpy( tarHeader.name, entryNamePath.c_str(), 100 );

            tarHeader.computeChecksum();

            // write header into current tar stream
            fwrite( &tarHeader, sizeof( tarHeader ), 1, tarOutputFile );

            // .. and then a copy of the file data verbatim
            if ( !bIsDirectory )
            {
                FILE* entryInputFile = fopen( entryFullFilename.string().c_str(), "rb" );
                absl::Cleanup closeFileOnScopeExit = [&]() noexcept {
                    fclose( entryInputFile );
                    entryInputFile = nullptr;
                    };

                // simple in/out pump of data from the file
                std::size_t bytesRead = 0;
                std::size_t bytesWritten = 0;
                while ( static_cast<void>(bytesRead = fread( ioBuffer, 1, ioBufferSize, entryInputFile )), bytesRead > 0 )
                {
                    fwrite( ioBuffer, bytesRead, 1, tarOutputFile );
                    bytesWritten += bytesRead;
                }
                bytesProcessedIntoTar += bytesWritten;

                // check and null-pad out to 512 byte boundary
                const std::size_t paddingBytes = 512 - bytesWritten % 512;
                if ( paddingBytes != 512 )
                {
                    fwrite( ioPadding, paddingBytes, 1, tarOutputFile );
                }
            }

            return absl::OkStatus();
        };

    // begin with the initial directory
    std::ignore = appendToTar( true, inputPath, inputPath.filename() );

    // create an iterator to walk the rest
    auto fileIterator = fs::recursive_directory_iterator( inputPath, std::filesystem::directory_options::skip_permission_denied );

    std::error_code osError;
    for ( auto fIt = fs::begin( fileIterator ); fIt != fs::end( fileIterator ); fIt = fIt.increment( osError ) )
    {
        if ( osError )
        {
            return absl::AbortedError( fmt::format( FMTX( "error ({}) during file iteration, aborted" ), osError.message() ) );
        }

        const auto& entryFullFilename   = fIt->path();
        const bool bIsDirectory         = fIt->is_directory();

        const auto entryRelativePath = entryFullFilename.lexically_relative( baseInputPath );

        const auto appendStatus = appendToTar( bIsDirectory, entryFullFilename, entryRelativePath );
        filesProcessedIntoTar++;

        if ( !appendStatus.ok() )
            return appendStatus;

        if ( archivingProgressFunction != nullptr )
            archivingProgressFunction( bytesProcessedIntoTar, filesProcessedIntoTar );
    }

    // EOF marker for TAR is two blank 512-byte chunks
    fwrite( ioPadding, 512, 1, tarOutputFile );
    fwrite( ioPadding, 512, 1, tarOutputFile );

    return absl::OkStatus();
}

absl::Status unarchiveTARIntoDirectory(
    const std::filesystem::path& inputTarFile,
    const std::filesystem::path& outputPath,
    const ArchiveProgressCallback& archivingProgressFunction )
{
    FILE* tarInputFile = fopen( inputTarFile.string().c_str(), "rb" );
    absl::Cleanup closeFileOnScopeExit = [&]() noexcept {
        fclose( tarInputFile );
        tarInputFile = nullptr;
        };

    std::vector<uint8_t> loadBuffer;
    loadBuffer.reserve( 1 * 1024 * 1024 );

    // keep track of bytes processed so the UI can show something happening
    std::size_t bytesProcessedFromTar = 0;
    std::size_t filesProcessedFromTar = 0;
    if ( archivingProgressFunction != nullptr )
        archivingProgressFunction( 0, 0 );

    while ( true )
    {
        THeader tarHeader;
        THeader::createDefault( tarHeader );

        std::size_t bytesRead = fread( &tarHeader, sizeof( THeader ), 1, tarInputFile );
        if ( bytesRead != 1 )
        {
            return absl::AbortedError( fmt::format( FMTX( "unable to read Tar header, fread returned {}" ), bytesRead ) );
        }

        // header is entirely zero? marks the end of the file
        if ( tarHeader.sumBlockData() == 0 )
        {
            break;
        }

        // check for a 'ustar' as a magic identifier
        if ( tarHeader.ustar[0] != 'u' || tarHeader.ustar[1] != 's' || tarHeader.ustar[2] != 't' )
        {
            return absl::AbortedError( "Tar header mising ustar identifier, aborting" );
        }

        const fs::path outputFilePath = fs::absolute( outputPath / fs::path( tarHeader.name ) );

        if ( tarHeader.link == THeader::FileType::DIRECTORY )
        {
            const absl::Status directoryOk = filesys::ensureDirectoryExists( outputFilePath );
            if ( !directoryOk.ok() )
            {
                return directoryOk;
            }
        }
        else
        if ( tarHeader.link == THeader::FileType::NORMAL )
        {
            const uint32_t fileSize = oct2uint( tarHeader.size );

            loadBuffer.resize( fileSize );
            const std::size_t loadBufferRead = fread( loadBuffer.data(), 1, fileSize, tarInputFile );
            if ( loadBufferRead != fileSize )
            {
                return absl::AbortedError( fmt::format( FMTX( "unable to read file data from tar, fread returned {}" ), loadBufferRead ) );
            }

            const std::size_t paddingBytes = 512 - fileSize % 512;
            if ( paddingBytes != 512 )
            {
                fseek( tarInputFile, static_cast<long>( paddingBytes ), SEEK_CUR );
            }

            {
                FILE* stemOutputFile = fopen( outputFilePath.string().c_str(), "wb" );
                absl::Cleanup closeOutputFileOnScopeExit = [&]() noexcept {
                    fclose( stemOutputFile );
                    stemOutputFile = nullptr;
                    };

                fwrite( loadBuffer.data(), fileSize, 1, stemOutputFile );
            }

            bytesProcessedFromTar += fileSize;
            filesProcessedFromTar++;

            if ( archivingProgressFunction != nullptr )
                archivingProgressFunction( bytesProcessedFromTar, filesProcessedFromTar );
        }
        else
        {
            return absl::UnimplementedError( fmt::format( FMTX( "Unknown header link type [{}] in tar" ), (uint32_t)tarHeader.link ) );
        }

    }

    return absl::OkStatus();
}

} // namespace io
