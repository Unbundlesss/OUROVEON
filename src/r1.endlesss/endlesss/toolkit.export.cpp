//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"

#include "app/core.h"
#include "filesys/fsutil.h"

#include "endlesss/core.constants.h"
#include "endlesss/live.stem.h"
#include "endlesss/toolkit.export.h"

#include "ssp/ssp.file.wav.h"
#include "ssp/ssp.file.flac.h"


// ---------------------------------------------------------------------------------------------------------------------
namespace endlesss {
namespace xp {

using TokenReplacements = robin_hood::unordered_flat_map< std::string, std::string >;

constexpr void tokenReplacement( std::string& source, const std::string& find, const std::string& replace )
{
    for ( std::string::size_type i = 0; (i = source.find( find, i )) != std::string::npos; )
    {
        source.replace( i, find.length(), replace );
        i += replace.length();
    }
}

// ---------------------------------------------------------------------------------------------------------------------
std::vector<fs::path> exportRiff(
    const ExportMode                exportMode,
    const app::StoragePaths&        storagePaths,
    const OutputSpec&               outputSpec,
    const endlesss::live::RiffPtr&  riffPtr )
{
    TokenReplacements tokenReplacements;

    std::vector<fs::path> outputFiles;
    outputFiles.reserve( 8 );

    const auto currentRiff = riffPtr.get();

    // jam level tokens
    {
        std::string jamNameSanitised;
        base::asciifyString( currentRiff->m_riffData.jam.displayName, jamNameSanitised );
        const std::string jamUID = currentRiff->m_riffData.jam.couchID.substr( outputSpec.custom.uniqueIDLength );

        tokenReplacements.emplace( OutputTokens::toString( OutputTokens::Enum::Jam_Name ),     jamNameSanitised );
        tokenReplacements.emplace( OutputTokens::toString( OutputTokens::Enum::Jam_UniqueID ), jamUID );
    }
    // riff level
    {
        const auto riffTimestampZoned = date::make_zoned(
            date::current_zone(),
            date::floor<std::chrono::seconds>( currentRiff->m_stTimestamp )
        );
        const auto riffTimestampString = date::format( outputSpec.custom.timestampFormatRiff, riffTimestampZoned );
        tokenReplacements.emplace( OutputTokens::toString( OutputTokens::Enum::Riff_Timestamp ), riffTimestampString );

        const std::string riffUID = currentRiff->m_riffData.riff.couchID.substr( outputSpec.custom.uniqueIDLength );
        tokenReplacements.emplace( OutputTokens::toString( OutputTokens::Enum::Riff_UniqueID ), riffUID );

        tokenReplacements.emplace( OutputTokens::toString( OutputTokens::Enum::Riff_BPM ),
            fmt::format( "{}bpm", currentRiff->m_riffData.riff.BPMrnd ) );

        tokenReplacements.emplace( OutputTokens::toString( OutputTokens::Enum::Riff_Root ),
            endlesss::constants::cRootNames[currentRiff->m_riffData.riff.root] );

        tokenReplacements.emplace( OutputTokens::toString( OutputTokens::Enum::Riff_Scale ),
            endlesss::constants::cScaleNamesFilenameSanitize[currentRiff->m_riffData.riff.scale] );
    }

    std::string rootPathString = outputSpec.riff;
    rootPathString.reserve( rootPathString.size() * 2 );
    for ( const auto& pair : tokenReplacements )
    {
        tokenReplacement( rootPathString, pair.first, pair.second );
    }
    rootPathString += "/";

    const auto rootPath = fs::path{ storagePaths.outputApp } /
                          fs::path{ rootPathString };

    if ( exportMode != ExportMode::DryRun )
    {
        filesys::ensureDirectoryExists( rootPath );

        // export the raw metadata out along with the stem data
        const auto riffMetadataPath = rootPath / "metadata.json";
        try
        {
            std::ofstream is( riffMetadataPath.string() );
            cereal::JSONOutputArchive archive( is );

            archive( currentRiff->m_riffData );
        }
        catch ( const std::exception& ex )
        {
            blog::error::cfg( "exportRiff was unable to save metadata, {}", ex.what() );
        }
    }

    const uint32_t exportSampleRate = currentRiff->m_targetSampleRate;
    currentRiff->exportToDisk( [&]( const uint32_t stemIndex, const endlesss::live::Stem& stemData ) -> ssp::SampleStreamProcessorInstance
        {
            const auto stemTimestamp = spacetime::InSeconds{ std::chrono::seconds{ stemData.m_data.creationTimeUnix } };
            const auto stemTimestampZoned = date::make_zoned(
                date::current_zone(),
                date::floor<std::chrono::seconds>( stemTimestamp )
            );
            const auto stemTimestampString = date::format( outputSpec.custom.timestampFormatRiff, stemTimestampZoned );
            tokenReplacements.insert_or_assign( OutputTokens::toString( OutputTokens::Enum::Stem_Timestamp ), stemTimestampString );

            const std::string stemUID = stemData.m_data.couchID.substr( outputSpec.custom.uniqueIDLength );
            tokenReplacements.insert_or_assign( OutputTokens::toString( OutputTokens::Enum::Stem_UniqueID ), stemUID );

            tokenReplacements.insert_or_assign( OutputTokens::toString( OutputTokens::Enum::Stem_Index ), fmt::format( "{}", stemIndex ) );
            tokenReplacements.insert_or_assign( OutputTokens::toString( OutputTokens::Enum::Stem_Author ), stemData.m_data.user );
            tokenReplacements.insert_or_assign( OutputTokens::toString( OutputTokens::Enum::Stem_Preset ), stemData.m_data.preset );


            std::string stemPathString = outputSpec.stem;
            stemPathString.reserve( stemPathString.size() * 2 );
            for ( const auto& pair : tokenReplacements )
            {
                tokenReplacement( stemPathString, pair.first, pair.second );
            }

            switch ( outputSpec.format )
            {
                case AudioFormat::FLAC: stemPathString += ".flac"; break;
                case AudioFormat::WAV:  stemPathString += ".wav"; break;
                default:
                    break;
            }

            const auto stemPath = fs::path{ rootPath } /
                                  fs::path{ stemPathString };

            const auto finalFilename = fs::absolute( stemPath ).string();
            outputFiles.emplace_back( finalFilename );

            if ( exportMode != ExportMode::DryRun )
            {
                auto stemPathNoFile = stemPath;
                     stemPathNoFile.remove_filename();

                filesys::ensureDirectoryExists( stemPathNoFile );

                switch ( outputSpec.format )
                {
                    case AudioFormat::FLAC: return ssp::FLACWriter::Create( finalFilename, exportSampleRate, 60.0f );
                    case AudioFormat::WAV:  return ssp::WAVWriter::Create( finalFilename, exportSampleRate, 60 );
                    default:
                        assert( 0 );
                        break;
                }
            }

            return nullptr;
        });

    return outputFiles;
}

} // namespace xp
} // namespace endlesss
