//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"

#include "base/spacetime.h"

#include "app/module.frontend.h"
#include "config/frontend.h"

#include "endlesss/api.h"
#include "endlesss/cache.jams.h"


// ---------------------------------------------------------------------------------------------------------------------
using namespace std::chrono_literals;


namespace endlesss {
namespace cache {

// ---------------------------------------------------------------------------------------------------------------------
bool Jams::load( const fs::path& cachePath )
{
    fs::path jamLibraryCacheFile = cachePath;
    jamLibraryCacheFile.append( cFilename );

    blog::cache( "Jam cache at [{}]", jamLibraryCacheFile.string() );

    if ( fs::exists( jamLibraryCacheFile ) )
    {
        try
        {
            std::ifstream is( jamLibraryCacheFile );
            cereal::JSONInputArchive archive( is );

            blog::cache( "Loading existing cache ..." );

            serialize( archive );

            // snag the last write time of the cache file
            {
                const auto filetime       = std::filesystem::last_write_time( jamLibraryCacheFile );
                const auto systemTime     = std::chrono::clock_cast<std::chrono::system_clock>(filetime);
                const auto timePointSec   = std::chrono::time_point_cast<std::chrono::seconds>(systemTime);

                const auto cacheTimeDelta = base::spacetime::calculateDeltaFromNow( timePointSec ).asPastTenseString(2);

                m_cacheFileState = fmt::format( "Jam cache updated {} ( {} items )", cacheTimeDelta, m_jamData.size() );
            }

            postProcessNewData();
            return true;
        }
        catch ( cereal::Exception& cEx )
        {
            blog::error::cache( "{}", jamLibraryCacheFile.string() );
            blog::error::cache( "cache load failure | {}", cEx.what() );
        }
    }
    m_cacheFileState = "No jam cache found";
    return false;
}

// ---------------------------------------------------------------------------------------------------------------------
bool Jams::save( const fs::path& cachePath )
{
    fs::path jamLibraryCacheFile = cachePath;
    jamLibraryCacheFile.append( cFilename );

    try
    {
        std::ofstream is( jamLibraryCacheFile );
        cereal::JSONOutputArchive archive( is );

        blog::cache( "... saving jam library cache to '{}'", jamLibraryCacheFile.string() );

        serialize( archive );

        {
            m_cacheFileState = fmt::format( "Jam cache synchronised ( {} items )", m_jamData.size() );
        }

        return true;
    }
    catch ( cereal::Exception& cEx )
    {
        blog::error::cache( "{}", jamLibraryCacheFile.string() );
        blog::error::cache( "cache save failure | {}", cEx.what() );
    }
    return false;
}

// ---------------------------------------------------------------------------------------------------------------------
void Jams::asyncCacheRebuild( const endlesss::api::NetConfiguration& ncfg, tf::Taskflow& taskFlow, const AsyncCallback& asyncCallback )
{
    taskFlow.emplace( [&, asyncCallback]()
    {
        asyncCallback( AsyncFetchState::Working, "Fetching private jams ..." );

        api::PrivateJams jamPrivate;
        if ( !jamPrivate.fetch( ncfg, ncfg.auth().user_id ) )
        {
            asyncCallback( AsyncFetchState::Failed, "Failed to get private jam data" );
            return;
        }

        asyncCallback( AsyncFetchState::Working, "Fetching public jams ..." );

        api::PublicJams jamPublic;
        if ( !jamPublic.fetch( ncfg ) )
        {
            asyncCallback( AsyncFetchState::Failed, "Failed to get public jam data" );
            return;
        }

        asyncCallback( AsyncFetchState::Working, "Pulling jam metadata ..." );

        m_jamData.clear();

        int64_t dummyTimestamp = 0;
        for ( const auto& jdb : jamPublic.band_ids )
        {
            api::JamProfile jamProfile;
            if ( !jamProfile.fetch( ncfg, endlesss::types::JamCouchID{ jdb } ) )
            {
                blog::error::cache( "jam profile failed on {}", jdb );
                continue;
            }
            m_jamData.emplace_back( jdb,
                                    jamProfile.displayName,
                                    "unknown",
                                    dummyTimestamp++,
                                    true );
        }

        for ( const auto& jdb : jamPrivate.rows )
        {
            api::JamProfile jamProfile;
            if ( !jamProfile.fetch( ncfg, endlesss::types::JamCouchID{ jdb.id }  ) )
            {
                blog::error::cache( "jam profile failed on {}", jdb.id );
                continue;
            }
            
            const auto jamTimestamp = base::spacetime::parseISO8601( jdb.key );

            m_jamData.emplace_back( jdb.id,
                                    jamProfile.displayName,
                                    jdb.key,
                                    jamTimestamp,
                                    false );
        }

        postProcessNewData();
        asyncCallback( AsyncFetchState::Success, "" );
    });

    asyncCallback( AsyncFetchState::Working, "Fetching data ..." );
}

} // namespace cache
} // namespace endlesss
