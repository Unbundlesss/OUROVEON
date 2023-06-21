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
Shares::~Shares()
{
}

// ---------------------------------------------------------------------------------------------------------------------
bool Shares::fetchAsync( const endlesss::api::NetConfiguration& apiCfg, const std::string& username, tf::Executor& taskExecutor )
{
    // can't start over if we're already working
    if ( isFetchingData() )
        return false;

    // flip the status to show the background update will begin working
    // and clear out the current data set to invalidate it
    m_asyncRefreshing = true;
    m_data.reset();

    m_taskFlow.clear();
    m_taskFlow.emplace( [&apiCfg, username, this]()
    {
        const int32_t count = 5;    // how many shared riffs to pull each time (5 is what the website uses at time of writing)
        int32_t offset = 0;

        SharedData newData = std::make_shared<Data>();
        newData->m_username = username;

        while ( true )
        {
            api::SharedRiffsByUser sharedRiffs;
            if ( sharedRiffs.fetch( apiCfg, username, count, offset ) )
            {
                for ( const auto& riffData : sharedRiffs.data )
                {
                    // there's no top-level "bandXXXX" identifier in shared riffs, so we go look through the loops'
                    // audio URLs to find it via regex
                    std::string jamCID;
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
                                blog::error::api( "multiple jam band IDs found inside shread riff loop data, unexpected" );
                                jamCID.clear();
                                break;
                            }

                            jamCID = extractedBandID;
                        }
                    }

                    // failed to reach a jam ID consensus
                    if ( jamCID.empty() )
                        continue;

                    // remove any invalid UTF8 characters from the title string before storage
                    std::string sanitisedTitle;
                    utf8::replace_invalid( riffData.title.begin(), riffData.title.end(), back_inserter( sanitisedTitle ) );

                    newData->m_names.emplace_back( sanitisedTitle );
                    newData->m_riffIDs.emplace_back( riffData.doc_id );
                    newData->m_jamIDs.emplace_back( jamCID );

                    const uint64_t timestampUnix = riffData.action_timestamp / 1000; // from unix nano
                    const auto deltaTime = spacetime::calculateDeltaFromNow( spacetime::InSeconds( std::chrono::seconds{timestampUnix} ) );

                    newData->m_timestamps.emplace_back( timestampUnix );
                    newData->m_timestampDeltas.emplace_back( deltaTime );
                }
            }
            else
            {
                // abort on a net failure
                blog::api( "shared riff fetch() failure, aborting" );
                newData.reset();
                break;
            }

            // less than we expected, we've fetched all we can
            if ( sharedRiffs.data.size() < count )
                break;

            offset += count;
        }

        if ( offset > 0 )
        {
            m_data = newData;
        }
        m_asyncRefreshing = false;
    });

    taskExecutor.run( m_taskFlow );
    return true;
}

} // namespace cache
} // namespace endlesss
