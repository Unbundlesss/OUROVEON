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
#include "endlesss/cache.shares.h"
#include "endlesss/config.h"

using namespace std::chrono_literals;

namespace endlesss {
namespace cache {

static constexpr auto cRegexBandNameExtract = "/(band[a-f0-9]+)/";

// ---------------------------------------------------------------------------------------------------------------------
Shares::Shares()
    : m_regexExtractBandName( cRegexBandNameExtract )
{
}

// ---------------------------------------------------------------------------------------------------------------------
bool Shares::fetchAsync(
    const endlesss::api::NetConfiguration& apiCfg,
    const std::string_view username,
    tf::Executor& taskExecutor,
    const std::function< void( StatusOrData ) > completionFunc )
{
    // create a copy to hand to the task
    std::string usernameCopy( username );

    m_taskFlow.clear();
    m_taskFlow.emplace( [&apiCfg, usernameToFetch = std::move( usernameCopy ), this, completionFunc]()
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
                    std::string jamCID = riffData.band;

                    // sometimes there's no top-level "bandXXXX" identifier in shared riffs, so we go look through the loops'
                    // audio URLs to find it via regex
                    if ( jamCID.empty() )
                    {
                        for ( const auto& riffLoop : riffData.loops )
                        {
                            const auto& loopUrl = riffLoop.cdn_attachments.oggAudio.url;

                            std::smatch m;
                            if ( std::regex_search( loopUrl, m, m_regexExtractBandName ) )
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

                        // failed to reach a jam ID consensus
                        if ( jamCID.empty() )
                            continue;
                    }

                    // remove any invalid UTF8 characters from the title string before storage
                    std::string sanitisedTitle;
                    utf8::replace_invalid( riffData.title.begin(), riffData.title.end(), back_inserter( sanitisedTitle ) );

                    newData->m_names.emplace_back( sanitisedTitle );
                    newData->m_images.emplace_back( riffData.image_url );
                    newData->m_riffIDs.emplace_back( riffData.doc_id );
                    newData->m_jamIDs.emplace_back( jamCID );
                    newData->m_private.emplace_back( riffData.is_private );

                    const uint64_t timestampUnix = riffData.action_timestamp / 1000; // from unix nano
                    // const auto deltaTime = spacetime::calculateDeltaFromNow( spacetime::InSeconds( std::chrono::seconds{timestampUnix} ) );

                    newData->m_timestamps.emplace_back( timestampUnix );

                    newData->m_count++;
                }
            }
            else
            {
                // abort on a net failure
                blog::api( "shared riff fetch() failure, aborting" );

                if ( completionFunc != nullptr )
                    completionFunc( absl::AbortedError( "network fetch failure, aborted" ) );
                
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
            if ( completionFunc != nullptr )
                completionFunc( newData );
        }
        else
        {
            if ( completionFunc != nullptr )
                completionFunc( absl::NotFoundError( "no shared riffs found" ) );
        }
    });

    taskExecutor.run( m_taskFlow );
    return true;
}

} // namespace cache
} // namespace endlesss
