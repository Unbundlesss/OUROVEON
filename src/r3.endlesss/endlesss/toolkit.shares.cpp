//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"

#include "spacetime/moment.h"

#include "endlesss/api.h"
#include "endlesss/toolkit.shares.h"
#include "endlesss/config.h"

using namespace std::chrono_literals;

namespace endlesss {
namespace toolkit {

// ---------------------------------------------------------------------------------------------------------------------
tf::Taskflow Shares::taskFetchLatest(
    const endlesss::api::NetConfiguration& apiCfg,
    std::string username,
    std::function< void( StatusOrData ) > completionFunc )
{
    tf::Taskflow taskResult;
    taskResult.emplace( [&apiCfg, usernameToFetch = std::move( username ), this, onCompletion = std::move( completionFunc )]()
    {
        const int32_t count = 5;    // how many shared riffs to pull each time (5 is what the website uses at time of writing)
        int32_t offset = 0;

        SharedData newData = std::make_shared<config::endlesss::SharedRiffsCache>();

        newData->m_username     = usernameToFetch;
        newData->m_lastSyncTime = spacetime::getUnixTimeNow().count();

        while ( true )
        {
            api::SharedRiffsByUser sharedRiffs;
            if ( sharedRiffs.fetch( apiCfg, usernameToFetch, count, offset ) )
            {
                for ( const auto& riffData : sharedRiffs.data )
                {
                    std::string jamCID = m_riffBandExtractor.estimateJamCouchID( riffData );
                    
                    // remove any invalid UTF8 characters from the title string before storage
                    std::string sanitisedTitle;
                    utf8::replace_invalid( riffData.title.begin(), riffData.title.end(), back_inserter( sanitisedTitle ) );

                    newData->m_names.emplace_back( sanitisedTitle );
                    newData->m_images.emplace_back( riffData.image_url );
                    newData->m_sharedRiffIDs.emplace_back( riffData._id );
                    newData->m_riffIDs.emplace_back( riffData.doc_id );
                    newData->m_jamIDs.emplace_back( jamCID );
                    newData->m_private.emplace_back( riffData.is_private );

                    // public jams are all prefixed 'band'; personal ones are just the usename
                    const bool bFromPersonalJam = ( jamCID == usernameToFetch || jamCID.rfind( "band", 0 ) != 0 );
                    newData->m_personal.emplace_back( bFromPersonalJam );

                    const uint64_t timestampUnix = riffData.action_timestamp / 1000; // from unix nano
                    
                    newData->m_timestamps.emplace_back( timestampUnix );

                    newData->m_count++;
                }
            }
            else
            {
                // abort on a net failure
                blog::api( "shared riff fetch() failure, aborting" );

                if ( onCompletion != nullptr )
                    onCompletion( absl::AbortedError( "network fetch failure, aborted" ) );
                
                return;
            }

            // less than we expected, we've fetched all we can
            if ( sharedRiffs.data.size() < count )
                break;

            offset += count;
        }

        // successfully got some data
        if ( offset > 0 )
        {
            if ( onCompletion != nullptr )
                onCompletion( newData );
        }
        else
        {
            if ( onCompletion != nullptr )
                onCompletion( absl::NotFoundError( "no shared riffs found" ) );
        }
    });

    return taskResult;
}

static constexpr auto cRegexBandNameExtract = "/(band[a-f0-9]+)/";

// ---------------------------------------------------------------------------------------------------------------------
RiffBandExtractor::RiffBandExtractor()
    : m_regexExtractBandName( cRegexBandNameExtract )
{
}

// ---------------------------------------------------------------------------------------------------------------------
std::string RiffBandExtractor::estimateJamCouchID( const api::SharedRiffsByUser::Data& sharedData ) const
{
    std::string jamCID = sharedData.band;

    // sometimes there's no top-level "bandXXXX" identifier in shared riffs, so we go look through the loops'
    // audio URLs to find it via regex & consensus
    if ( jamCID.empty() )
    {
        for ( const auto& riffLoop : sharedData.loops )
        {
            // try formats in order, one should be not empty at least
            const std::string& loopUrl = (riffLoop.cdn_attachments.oggAudio.url.empty()) ?
                riffLoop.cdn_attachments.flacAudio.url :
                riffLoop.cdn_attachments.oggAudio.url;

            std::smatch m;
            if ( !loopUrl.empty() && std::regex_search( loopUrl, m, m_regexExtractBandName ) )
            {
                const auto& extractedBandID = m[1].str();

                // we assume all the band IDs across the loops should be consistent - check for this
                // and fail out if this doesn't hold up
                if ( !jamCID.empty() && jamCID != extractedBandID )
                {
                    blog::error::api( FMTX( "multiple jam band IDs found inside shread riff loop data, unexpected ( existing {}, new {} )" ), extractedBandID, jamCID );
                    jamCID.clear();
                    break;
                }

                jamCID = extractedBandID;
            }
        }

        // failed to reach a jam ID consensus (hit the error above)
        if ( jamCID.empty() )
            return jamCID;
    }

    return jamCID;
}

} // namespace toolkit
} // namespace endlesss
