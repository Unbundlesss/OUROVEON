//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#include "pch.h"

#include "base/text.h"
#include "base/text.transform.h"
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
std::vector<fs::path> exportRiff(
    endlesss::api::NetConfiguration&    netCfg,
    const RiffExportMode                exportMode,
    const RiffExportDestination&        destination,
    const RiffExportAdjustments&        adjustments,
    const endlesss::live::RiffPtr&      riffPtr )
{
    TokenReplacements tokenReplacements;

    std::vector<fs::path> outputFiles;
    outputFiles.reserve( 8 );

    const auto currentRiff = riffPtr.get();

    // jam level tokens
    {
        std::string jamNameSanitised, jamDescriptionSanitised;
        base::sanitiseNameForPath( currentRiff->m_riffData.jam.displayName, jamNameSanitised );
        base::sanitiseNameForPath( currentRiff->m_riffData.jam.description, jamDescriptionSanitised );

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
        base::sanitiseNameForPath( currentRiff->m_riffData.riff.description, riffDescriptionSanitised );

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

        // only try for image downloads if we have Endlessss access - without it, we assume the CDN might be 
        // unavailable too. this code was written in a bit of a panic during the shutdown week
        if ( netCfg.hasAccess( endlesss::api::NetConfiguration::Access::Authenticated ) )
        {
            // wrap the whole lot in a filthy exception trap, last thing we want is some goofy bug in here
            // breaking shared riff downloads
            try
            {
                if ( !currentRiff->m_riffData.riff.attachedImageURL.empty() )
                {
                    const auto& imageUrl = currentRiff->m_riffData.riff.attachedImageURL;
                    const base::UriParse parser( imageUrl );
                    if ( !parser.isValid() )
                    {
                        blog::error::core( "[Riff Image Download] URL Parse fail : {}", imageUrl );
                    }
                    else
                    {
                        std::string imagePath = parser.path();

                        auto dataClient = std::make_unique< httplib::SSLClient >( parser.host() );

                        dataClient->set_ca_cert_path( netCfg.api().certBundleRelative.c_str() );
                        dataClient->enable_server_certificate_verification( true );

                        auto res = netCfg.attempt( [&]() -> httplib::Result {
                            return dataClient->Get( parser.path() );
                            } );

                        if ( res == nullptr )
                        {
                            blog::error::core( "[Riff Image Download] http GET failed entirely" );
                        }
                        else if ( res->status != 200 )
                        {
                            blog::error::core( "[Riff Image Download] http GET failed, status {}", res->status );
                        }
                        else
                        {
                            std::string imageFilename = "cover_image";

                            // lol help
                            const std::string imageMIME = base::StrToLwrExt( res->get_header_value( "Content-Type" ) );
                            if ( imageMIME == "image/gif" )
                                imageFilename += ".gif";
                            if ( imageMIME == "image/jpeg" )
                                imageFilename += ".jpg";
                            if ( imageMIME == "image/png" )
                                imageFilename += ".png";

                            auto imageCoverPath = rootPathU8 / imageFilename;

#if OURO_PLATFORM_WIN
                            FILE* fpImage = _wfopen( reinterpret_cast<const wchar_t*>(imageCoverPath.c_str()), L"wb" );
#else
                            FILE* fpImage = fopen( imageCoverPath.c_str(), "wb" );
#endif
                            if ( fpImage != nullptr )
                            {
                                fwrite( (void*)res->body.data(), 1, res->body.size(), fpImage );
                                fclose( fpImage );
                            }
                            else
                            {
                                blog::error::core( "[Riff Image Download] unable to save to '{}'", imageCoverPath.string() );
                            }
                        }
                    }
                } // if has attached image
            }
            catch ( ... )
            {
                blog::error::core( "[Riff Image Download] unhandled exception saving the cover image" );
            }
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
