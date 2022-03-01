//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"

#include "spacetime/chronicle.h"
#include "spacetime/moment.h"

#include "app/module.frontend.h"
#include "config/base.h"
#include "config/frontend.h"

#include "endlesss/api.h"
#include "endlesss/cache.jams.h"

using namespace std::chrono_literals;


namespace endlesss {
namespace cache {

// ---------------------------------------------------------------------------------------------------------------------
bool Jams::load( const config::IPathProvider& pathProvider )
{
    // load the fixed public jam snapshot, assuming it shipped with the build correctly
    const auto publicsLoadResult = config::load( pathProvider, m_configEndlesssPublics );
    if ( publicsLoadResult != config::LoadResult::Success )
    {
        // we expect the public jam manifest, although we could survive without it tbh
        blog::error::cfg( "Unable to load Endlesss public jam manifest [{}]",
            config::getFullPath< config::endlesss::PublicJamManifest >( pathProvider ).string() );
    }
    blog::core( "loaded {} public jams from snapshot manifest", m_configEndlesssPublics.jams.size() );


    // dynamic jam cache is shared between apps
    fs::path jamLibraryCacheFile = pathProvider.getPath( config::IPathProvider::PathFor::SharedConfig );
    jamLibraryCacheFile.append( cFilename );

    if ( fs::exists( jamLibraryCacheFile ) )
    {
        blog::cache( "loading dynamic jam cache [{}]", jamLibraryCacheFile.string() );
        try
        {
            std::ifstream is( jamLibraryCacheFile );
            cereal::JSONInputArchive archive( is );

            serialize( archive );

            // snag the last write time of the cache file
            {
#if OURO_PLATFORM_WIN
                const auto filetime       = std::filesystem::last_write_time( jamLibraryCacheFile );
                const auto systemTime     = std::chrono::clock_cast<std::chrono::system_clock>(filetime);
                const auto timePointSec   = std::chrono::time_point_cast<std::chrono::seconds>(systemTime);

                const auto cacheTimeDelta = spacetime::calculateDeltaFromNow( timePointSec ).asPastTenseString(2);

                m_cacheFileState = fmt::format( "Jam cache updated {}", cacheTimeDelta );
#else
                m_cacheFileState = fmt::format( "Jam cache updated" );
#endif                 
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
bool Jams::save( const config::IPathProvider& pathProvider )
{
    fs::path jamLibraryCacheFile = pathProvider.getPath( config::IPathProvider::PathFor::SharedConfig );
    jamLibraryCacheFile.append( cFilename );

    try
    {
        std::ofstream is( jamLibraryCacheFile );
        cereal::JSONOutputArchive archive( is );

        blog::cache( "... saving jam library cache to '{}'", jamLibraryCacheFile.string() );

        serialize( archive );

        {
            m_cacheFileState = fmt::format( "Jam cache synchronised" );
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
        asyncCallback( AsyncFetchState::Working, "Fetching subscribed jams ..." );

        api::SubscribedJams jamSubscribed;
        if ( !jamSubscribed.fetch( ncfg, ncfg.auth().user_id ) )
        {
            asyncCallback( AsyncFetchState::Failed, "Failed to get subscribed jam data" );
            return;
        }

        asyncCallback( AsyncFetchState::Working, "Fetching current public jams ..." );

        api::CurrentJoinInJams jamJoinIn;
        if ( !jamJoinIn.fetch( ncfg ) )
        {
            asyncCallback( AsyncFetchState::Failed, "Failed to get public jam data" );
            return;
        }

        asyncCallback( AsyncFetchState::Working, "Updating jam metadata ..." );


        m_jamDataJoinIn.clear();

        int64_t dummyTimestamp = 0;
        for ( const auto& jdb : jamJoinIn.band_ids )
        {
            api::JamProfile jamProfile;
            if ( !jamProfile.fetch( ncfg, endlesss::types::JamCouchID{ jdb } ) )
            {
                blog::error::cache( "jam profile failed on {}", jdb );
                continue;
            }

            m_jamDataJoinIn.emplace_back( jdb,
                                          jamProfile.displayName,
                                          jamProfile.bio,
                                          dummyTimestamp++ );
        }

        m_jamDataUserSubscribed.clear();

        for ( const auto& jdb : jamSubscribed.rows )
        {
            api::JamProfile jamProfile;
            if ( !jamProfile.fetch( ncfg, endlesss::types::JamCouchID{ jdb.id }  ) )
            {
                blog::error::cache( "jam profile failed on {}", jdb.id );
                continue;
            }

            const auto jamTimestamp = spacetime::parseISO8601( jdb.key );

            m_jamDataUserSubscribed.emplace_back( jdb.id,
                                                  jamProfile.displayName,
                                                  jamProfile.bio,
                                                  jamTimestamp );
        }

        postProcessNewData();
        asyncCallback( AsyncFetchState::Success, "" );
    });

    asyncCallback( AsyncFetchState::Working, "Fetching data ..." );
}

// ---------------------------------------------------------------------------------------------------------------------
bool Jams::loadDataForCacheIndex( const CacheIndex& index, Data& result ) const
{
    if ( !index.valid() )
        return false;

    const std::vector< Data >* dataArray = getArrayPtrForType( index.type() );
    result = dataArray->at( index.index() );
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
void Jams::postProcessNewData()
{
    spacetime::ScopedTimer perfTimer( __FUNCTION__ );

    m_jamDataPublicArchive.clear();

    for ( const auto& pjam : m_configEndlesssPublics.jams )
    {
        auto& pjd = m_jamDataPublicArchive.emplace_back( pjam.band_id,
                                                         pjam.jam_name,
                                                         fmt::format("started by [{}]\nest. {} days of activity", pjam.earliest_user, pjam.estimated_days_of_activity ),
                                                         pjam.earliest_unixtime );
        pjd.m_riffCount = pjam.total_riffs;
    }

    for ( auto& innerVec : m_idxSortedByTime )
        innerVec.clear();
    for ( auto& innerVec : m_idxSortedByName )
        innerVec.clear();

    m_jamCouchIDToJamIndexMap.clear();

    for ( const auto jamType : cEachJamType )
    {
        const std::vector< Data >* dataArray = getArrayPtrForType( jamType );
        assert( dataArray );

        const size_t jamTypeIndex = (size_t)jamType;

        for ( size_t idx = 0; idx < dataArray->size(); idx++ )
        {
            m_idxSortedByTime[jamTypeIndex].push_back( idx );
            m_idxSortedByName[jamTypeIndex].push_back( idx );

            std::sort( m_idxSortedByTime[jamTypeIndex].begin(), m_idxSortedByTime[jamTypeIndex].end(),
                [dataArray]( const size_t lhs, const size_t rhs ) -> bool
                {
                    return dataArray->at(lhs).m_timestampOrdering < dataArray->at(rhs).m_timestampOrdering;
                });

            std::sort( m_idxSortedByName[jamTypeIndex].begin(), m_idxSortedByName[jamTypeIndex].end(),
                [dataArray]( const size_t lhs, const size_t rhs ) -> bool
                {
                    return dataArray->at( lhs ).m_displayName < dataArray->at( rhs ).m_displayName;
                });

            m_jamCouchIDToJamIndexMap.try_emplace( dataArray->at(idx).m_jamCID, CacheIndex( jamType, idx ) );
        }
    }

    {
        for ( Data& data : m_jamDataPublicArchive )
        {
            if ( data.m_timestampOrdering > 0 )
                data.m_timestampOrderingDescription = spacetime::datestampStringFromUnix( data.m_timestampOrdering );
            else
                data.m_timestampOrderingDescription = " - ";
        }

        for ( Data& data : m_jamDataJoinIn )
            data.m_timestampOrderingDescription = "";

        for ( Data& data : m_jamDataUserSubscribed )
            data.m_timestampOrderingDescription = spacetime::datestampStringFromUnix( data.m_timestampOrdering );
    }
}

} // namespace cache
} // namespace endlesss
