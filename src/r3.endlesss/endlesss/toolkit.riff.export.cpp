//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"

#include "base/text.h"
#include "app/core.h"
#include "filesys/fsutil.h"

#include "endlesss/core.constants.h"
#include "endlesss/live.stem.h"
#include "endlesss/toolkit.riff.export.h"

#include "ssp/ssp.file.wav.h"
#include "ssp/ssp.file.flac.h"


// ---------------------------------------------------------------------------------------------------------------------
namespace endlesss {
namespace toolkit {
namespace xp {

using TokenReplacements = absl::flat_hash_map< std::string, std::string >;

inline void tokenReplacement( std::string& source, const std::string& find, const std::string& replace )
{
    for ( std::string::size_type i = 0; (i = source.find( find, i )) != std::string::npos; )
    {
        source.replace( i, find.length(), replace );
        i += replace.length();
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// given a jam or riff name, try and remove anything too problematic for creating a file/directory using it 
//
inline void sanitiseNameForPath( const std::string_view source, std::string& dest, const char32_t replacementChar = '_' )
{
    dest.clear();
    dest.reserve( source.length() );

    const char* w = source.data();
    const char* sourceEnd = w + source.length();

    // decode the source as a UTF8 stream to preserve any interesting, valid characters
    while ( w != sourceEnd )
    {
        char32_t cp = utf8::next( w, sourceEnd );
        
        // blitz control characters
        if ( cp >= 0x00 && cp <= 0x1f )
            cp = replacementChar;
        if ( cp >= 0x80 && cp <= 0x9f )
            cp = replacementChar;

        // strip out problematic pathname characters
        switch ( cp )
        {
            case '/':
            case '?':
            case '<':
            case '>':
            case '\\':
            case ':':
            case '*':
            case '|':
            case '\"':
            case '~':
            case '.':
                cp = replacementChar;

            default:
                break;
        }

        utf8::append( cp, dest );
    }
}

// ---------------------------------------------------------------------------------------------------------------------
std::vector<fs::path> exportRiff(
    const RiffExportMode            exportMode,
    const RiffExportDestination&    destination,
    const RiffExportAdjustments&    adjustments,
    const endlesss::live::RiffPtr&  riffPtr )
{
    TokenReplacements tokenReplacements;

    std::vector<fs::path> outputFiles;
    outputFiles.reserve( 8 );

    const auto currentRiff = riffPtr.get();

    // jam level tokens
    {
        std::string jamNameSanitised, jamDescriptionSanitised;
        sanitiseNameForPath( currentRiff->m_riffData.jam.displayName, jamNameSanitised );
        sanitiseNameForPath( currentRiff->m_riffData.jam.description, jamDescriptionSanitised );

        // ensure we have some kind of jam name, just in case - this should not happen though, so assert and trace
        ABSL_ASSERT( !jamNameSanitised.empty() );
        if ( jamNameSanitised.empty() )
        {
            jamNameSanitised = "UnknownJamName";
        }
        
        // bolt on some kind of separator if we have a description
        if ( !jamDescriptionSanitised.empty() )
            jamDescriptionSanitised += destination.m_spec.custom.jamDescriptionSeparator;

        const std::string jamUID = currentRiff->m_riffData.jam.couchID.substr( destination.m_spec.custom.uniqueIDLength );

        tokenReplacements.emplace( OutputTokens::toString( OutputTokens::Enum::Jam_Name ),          jamNameSanitised );
        tokenReplacements.emplace( OutputTokens::toString( OutputTokens::Enum::Jam_UniqueID ),      jamUID );
        tokenReplacements.emplace( OutputTokens::toString( OutputTokens::Enum::Jam_Description ),   jamDescriptionSanitised );
    }
    // riff level
    {
        std::string riffDescriptionSanitised;
        sanitiseNameForPath( currentRiff->m_riffData.riff.description, riffDescriptionSanitised );

        const auto riffTimestampZoned = date::make_zoned(
            date::current_zone(),
            date::floor<std::chrono::seconds>( currentRiff->m_stTimestamp )
        );
        const auto riffTimestampString = date::format( destination.m_spec.custom.timestampFormatRiff, riffTimestampZoned );
        tokenReplacements.emplace( OutputTokens::toString( OutputTokens::Enum::Riff_Timestamp ), riffTimestampString );

        const std::string riffUID = currentRiff->m_riffData.riff.couchID.substr( destination.m_spec.custom.uniqueIDLength );
        tokenReplacements.emplace( OutputTokens::toString( OutputTokens::Enum::Riff_UniqueID ), riffUID );

        tokenReplacements.emplace( OutputTokens::toString( OutputTokens::Enum::Riff_BPM ),
            fmt::format( "{}bpm", currentRiff->m_riffData.riff.BPMrnd ) );

        tokenReplacements.emplace( OutputTokens::toString( OutputTokens::Enum::Riff_Root ),
            endlesss::constants::cRootNames[currentRiff->m_riffData.riff.root] );

        tokenReplacements.emplace( OutputTokens::toString( OutputTokens::Enum::Riff_Scale ),
            endlesss::constants::cScaleNamesFilenameSanitize[currentRiff->m_riffData.riff.scale] );

        tokenReplacements.emplace( OutputTokens::toString( OutputTokens::Enum::Riff_Description ), riffDescriptionSanitised );
    }

    std::string rootPathStringU8;
    {
        std::string rootPathString = destination.m_spec.riff;
        rootPathString.reserve( rootPathString.size() * 2 );
        for ( const auto& pair : tokenReplacements )
        {
            tokenReplacement( rootPathString, pair.first, pair.second );
        }
        rootPathString += "/";

        // utf8 pre-sanitize
        utf8::replace_invalid( rootPathString.begin(), rootPathString.end(), back_inserter( rootPathStringU8 ) );
    }
    // ensure any utf-8 data is preseved as we construct an fs::path (this is so dumb)
    const char8_t* rootPathAsChar8 = reinterpret_cast< const char8_t* >( rootPathStringU8.c_str() );

    // forge the basic path to export to 
    const auto path_baseOutput  = fs::path{ destination.m_paths.outputApp };
    const auto path_fileRoot    = fs::path{ rootPathAsChar8 };
    const auto rootPathU8       = path_baseOutput / path_fileRoot;

    if ( exportMode != RiffExportMode::DryRun )
    {
        const auto rootPathStatus = filesys::ensureDirectoryExists( rootPathU8 );
        if ( !rootPathStatus.ok() )
        {
            blog::error::core( "unable to create output path [{}], {}", rootPathU8.string(), rootPathStatus.ToString() );
            return outputFiles;
        }

        // export the raw metadata out along with the stem data
        const auto riffMetadataPath = rootPathU8 / "metadata.json";
        try
        {
            std::ofstream is( riffMetadataPath );
            cereal::JSONOutputArchive archive( is );

            archive( currentRiff->m_riffData );
        }
        catch ( const std::exception& ex )
        {
            blog::error::core( "exportRiff was unable to save metadata, {}", ex.what() );
            // not a fatal case
        }
    }

    const uint32_t exportSampleRate = currentRiff->m_stemSampleRate;
    currentRiff->exportToDisk( [&]( const uint32_t stemIndex, const endlesss::live::Stem& stemData ) -> ssp::SampleStreamProcessorInstance
        {
            const auto stemTimestamp = spacetime::InSeconds{ std::chrono::seconds{ stemData.m_data.creationTimeUnix } };
            const auto stemTimestampZoned = date::make_zoned(
                date::current_zone(),
                date::floor<std::chrono::seconds>( stemTimestamp )
            );
            const auto stemTimestampString = date::format( destination.m_spec.custom.timestampFormatRiff, stemTimestampZoned );
            tokenReplacements.insert_or_assign( OutputTokens::toString( OutputTokens::Enum::Stem_Timestamp ), stemTimestampString );

            const std::string stemUID = stemData.m_data.couchID.substr( destination.m_spec.custom.uniqueIDLength );
            tokenReplacements.insert_or_assign( OutputTokens::toString( OutputTokens::Enum::Stem_UniqueID ), stemUID );

            tokenReplacements.insert_or_assign( OutputTokens::toString( OutputTokens::Enum::Stem_Index ), fmt::format( "{}", stemIndex ) );
            tokenReplacements.insert_or_assign( OutputTokens::toString( OutputTokens::Enum::Stem_Author ), stemData.m_data.user );
            tokenReplacements.insert_or_assign( OutputTokens::toString( OutputTokens::Enum::Stem_Preset ), stemData.m_data.preset );


            std::string stemPathStringU8;
            {
                std::string stemPathString = destination.m_spec.stem;
                stemPathString.reserve( stemPathString.size() * 2 );
                for ( const auto& pair : tokenReplacements )
                {
                    tokenReplacement( stemPathString, pair.first, pair.second );
                }

                // utf8 pre-sanitize
                utf8::replace_invalid( stemPathString.begin(), stemPathString.end(), back_inserter( stemPathStringU8 ) );
            }

            switch ( destination.m_spec.format )
            {
                case AudioFormat::FLAC: stemPathStringU8 += ".flac"; break;
                case AudioFormat::WAV:  stemPathStringU8 += ".wav"; break;
                default:
                    break;
            }

            // ensure any utf-8 data is preseved as we construct an fs::path (this is so dumb)
            const char8_t* stemPathStringAsChar8 = reinterpret_cast<const char8_t*>(stemPathStringU8.c_str());
            const auto stemPath = fs::absolute( rootPathU8 /
                                                fs::path{ stemPathStringAsChar8 } );

            outputFiles.emplace_back( stemPath );

            if ( exportMode != RiffExportMode::DryRun )
            {
                auto stemPathNoFile = stemPath;
                     stemPathNoFile.remove_filename();

                const auto stemPathStatus = filesys::ensureDirectoryExists( stemPathNoFile );
                if ( !stemPathStatus.ok() )
                {
                    return nullptr;
                }

                switch ( destination.m_spec.format )
                {
                    case AudioFormat::FLAC: return ssp::FLACWriter::Create( stemPath, exportSampleRate, 60.0f );
                    case AudioFormat::WAV:  return ssp::WAVWriter::Create( stemPath, exportSampleRate, 60 );
                    default:
                        ABSL_ASSERT( false );
                        break;
                }
            }

            return nullptr;
        }, 
        adjustments.m_exportSampleOffset );

    return outputFiles;
}

} // namespace xp
} // namespace toolkit
} // namespace endlesss
