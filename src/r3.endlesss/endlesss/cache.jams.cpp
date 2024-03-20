//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#include "pch.h"

#include "base/text.transform.h"

#include "spacetime/chronicle.h"
#include "spacetime/moment.h"

#include "app/module.frontend.h"
#include "config/base.h"
#include "config/frontend.h"

#include "endlesss/api.h"
#include "endlesss/cache.jams.h"
#include "endlesss/config.h"

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
        blog::error::cfg( FMTX("Unable to load Endlesss public jam manifest [{}]"),
            config::getFullPath< config::endlesss::PublicJamManifest >( pathProvider ).string() );
    }
    blog::core( "loaded {} public jams from snapshot manifest", m_configEndlesssPublics.jams.size() );

    // try and load the cached collectibles data
    m_configEndlesssCollectibles.jams.clear();
    const auto collectibleLoadResult = config::load( pathProvider, m_configEndlesssCollectibles );
    if ( collectibleLoadResult != config::LoadResult::Success )
    {
        // if it failed to load or wasn't present
        // try and copy over the snapshot data, then try and reload it
        const fs::path fromPath = getFullPath<config::endlesss::CollectibleJamManifestSnapshot>( pathProvider );
        const fs::path toPath   = getFullPath<config::endlesss::CollectibleJamManifest>( pathProvider );

        std::error_code copyErrorCode;
        fs::copy_file( fromPath, toPath, fs::copy_options::overwrite_existing, copyErrorCode );

        if ( copyErrorCode )
        {
            blog::error::cfg( FMTX( "Error occured while copying collectibles snapshot : {}"), copyErrorCode.message() );
        }
        else
        {
            blog::core( FMTX("copied collectible snapshot, reloading...") );
            config::load( pathProvider, m_configEndlesssCollectibles );
        }
    }
    blog::core( FMTX("loaded {} collectible jams from snapshot manifest"), m_configEndlesssCollectibles.jams.size());


    // dynamic jam cache is shared between apps
    fs::path jamLibraryCacheFile = pathProvider.getPath( config::IPathProvider::PathFor::SharedConfig );
    jamLibraryCacheFile.append( cFilename );

    if ( fs::exists( jamLibraryCacheFile ) )
    {
        blog::cache( FMTX("loading dynamic jam cache [{}]"), jamLibraryCacheFile.string() );
        try
        {
            std::ifstream is( jamLibraryCacheFile );
            cereal::JSONInputArchive archive( is );

            serialize( archive );

            // snag the last write time of the cache file
            {
                const auto filetime       = std::filesystem::last_write_time( jamLibraryCacheFile );
#if OURO_PLATFORM_WIN
                const auto systemTime     = std::chrono::utc_clock::to_sys( std::chrono::file_clock::to_utc( filetime ) );
#else
                const auto systemTime     = std::chrono::file_clock::to_sys( filetime );
#endif
                const auto timePointSec   = std::chrono::time_point_cast<std::chrono::seconds>(systemTime);

                const auto cacheTimeDelta = spacetime::calculateDeltaFromNow( timePointSec ).asPastTenseString(2);

                m_cacheFileState = fmt::format( "Updated {}", cacheTimeDelta );
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

    m_cacheFileState = "Not found";
    return false;
}

// ---------------------------------------------------------------------------------------------------------------------
bool Jams::save( const config::IPathProvider& pathProvider )
{
    const auto collectibleSaveResult = config::save( pathProvider, m_configEndlesssCollectibles );
    if ( collectibleSaveResult != config::SaveResult::Success )
    {
        blog::error::cache( "unable to save collectibles manifest" );
    }


    fs::path jamLibraryCacheFile = pathProvider.getPath( config::IPathProvider::PathFor::SharedConfig );
    jamLibraryCacheFile.append( cFilename );

    try
    {
        std::ofstream is( jamLibraryCacheFile );
        cereal::JSONOutputArchive archive( is );

        blog::cache( "... saving jam library cache to '{}'", jamLibraryCacheFile.string() );

        serialize( archive );

        {
            m_cacheFileState = fmt::format( "Synchronised" );
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
void Jams::asyncCacheRebuild(
    const endlesss::api::NetConfiguration& netConfig,
    const config::endlesss::SyncOptions& syncOptions,
    tf::Taskflow& taskFlow,
    const AsyncCallback& asyncCallback )
{
    taskFlow.emplace( [this, &netConfig, syncOptions, asyncCallback]()
    {
        static constexpr std::array< char, 4> busyAscii = { '\\', '|', '/', '-' };
        int32_t busyCounter = 0;

        asyncCallback( AsyncFetchState::Working, "Fetching subscribed jams ..." );

        api::SubscribedJams jamSubscribed;
        if ( !jamSubscribed.fetch( netConfig, netConfig.auth().user_id ) )
        {
            asyncCallback( AsyncFetchState::Failed, "Failed to get subscribed jam data" );
            return;
        }

        asyncCallback( AsyncFetchState::Working, "Fetching current public jams ..." );

        api::CurrentJoinInJams jamJoinIn;
        if ( !jamJoinIn.fetch( netConfig ) )
        {
            asyncCallback( AsyncFetchState::Failed, "Failed to get public jam data" );
            return;
        }

        if ( syncOptions.sync_collectibles )
        {
            // fetch all known pages of collectibles
            std::vector< api::CurrentCollectibleJams::Data > collectedCollectibles;
            for ( int32_t page = 0; page < 100; page++ )    // just putting some kind of limit on this nonsense, don't know if there's a way to get total pages
            {
                asyncCallback( AsyncFetchState::Working, fmt::format( FMTX( "Fetching collectibles, page {} ..." ), page ) );

                // if the fetch works, bolt the new list onto the existing pile
                api::CurrentCollectibleJams collectibles;
                bool fetchOk = collectibles.fetch( netConfig, page );
                if ( fetchOk && collectibles.ok && !collectibles.data.empty() )
                {
                    collectedCollectibles.insert( collectedCollectibles.end(), collectibles.data.begin(), collectibles.data.end() );
                }
                else
                {
                    break;
                }
            }
            // convert to our cached collectibles type
            {
                m_configEndlesssCollectibles.jams.clear();
                for ( const api::CurrentCollectibleJams::Data& cdata : collectedCollectibles )
                {
                    if ( cdata.name.empty() )
                        continue;

                    config::endlesss::CollectibleJamManifest::Jam cjam;

                    cjam.jamId    = cdata.jamId;
                    cjam.name     = cdata.name;
                    cjam.bio      = cdata.bio;
                    cjam.bandId   = cdata.legacy_id;
                    cjam.owner    = cdata.owner;
                    cjam.members  = cdata.members;
                    cjam.rifftime = cdata.rifff.created;

                    asyncCallback( AsyncFetchState::Working, fmt::format( FMTX( "Analysing Collectibles ({})" ), busyAscii[busyCounter] ) );
                    busyCounter = (busyCounter + 1) % 4;

                    if ( syncOptions.sync_state )
                    {
                        api::JamRiffCount riffCount;
                        if ( riffCount.fetch( netConfig, endlesss::types::JamCouchID{ cjam.bandId } ) )
                        {
                            cjam.riffCount = riffCount.total_rows;
                        }
                    }

                    m_configEndlesssCollectibles.jams.emplace_back( cjam );
                }
                blog::cache( "extracted {} collectible jams", m_configEndlesssCollectibles.jams.size() );
            }
        }

        asyncCallback( AsyncFetchState::Working, "Updating jam metadata ..." );


        m_jamDataJoinIn.clear();

        int64_t dummyTimestamp = 0;
        for ( const auto& jdb : jamJoinIn.band_ids )
        {
            api::JamProfile jamProfile;
            if ( !jamProfile.fetch( netConfig, endlesss::types::JamCouchID{ jdb } ) )
            {
                blog::error::cache( "jam profile failed on {}", jdb );
                continue;
            }

            auto& newJamData = m_jamDataJoinIn.emplace_back( jdb,
                                                             jamProfile.displayName,
                                                             jamProfile.bio,
                                                             dummyTimestamp++ );

            asyncCallback( AsyncFetchState::Working, fmt::format( FMTX( "Analysing Public ({})" ), busyAscii[busyCounter] ) );
            busyCounter = (busyCounter + 1) % 4;

            if ( syncOptions.sync_state )
            {
                api::JamRiffCount riffCount;
                if ( riffCount.fetch( netConfig, endlesss::types::JamCouchID{ jdb } ) )
                {
                    newJamData.m_riffCount = riffCount.total_rows;
                }
            }
        }

        m_jamDataUserSubscribed.clear();

        for ( const auto& jdb : jamSubscribed.rows )
        {
            api::JamProfile jamProfile;
            if ( !jamProfile.fetch( netConfig, endlesss::types::JamCouchID{ jdb.id }  ) )
            {
                blog::error::cache( "jam profile failed on {}", jdb.id );
                continue;
            }

            const auto jamTimestamp = spacetime::parseISO8601( jdb.key );

            auto& newJamData = m_jamDataUserSubscribed.emplace_back( jdb.id,
                                                                     jamProfile.displayName,
                                                                     jamProfile.bio,
                                                                     jamTimestamp );

            asyncCallback( AsyncFetchState::Working, fmt::format( FMTX( "Analysing Subscribed ({})" ), busyAscii[busyCounter] ) );
            busyCounter = (busyCounter + 1) % 4;

            if ( syncOptions.sync_state )
            {
                api::JamRiffCount riffCount;
                if ( riffCount.fetch( netConfig, endlesss::types::JamCouchID{ jdb.id } ) )
                {
                    newJamData.m_riffCount = riffCount.total_rows;
                }
            }
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

    std::scoped_lock<std::mutex> lockProc( m_dataProcessMutex );

    m_jamDataPublicArchive.clear();

    for ( const auto& pjam : m_configEndlesssPublics.jams )
    {
        auto& pjd = m_jamDataPublicArchive.emplace_back( pjam.band_id,
                                                         pjam.jam_name,
                                                         fmt::format("started by [{}]\nest. {} days of activity", pjam.earliest_user, pjam.estimated_days_of_activity ),
                                                         pjam.earliest_unixtime );
        pjd.m_riffCount = pjam.total_riffs;
    }
    for ( const auto& cjam : m_configEndlesssCollectibles.jams )
    {
        auto& pjd = m_jamDataCollectibles.emplace_back(  cjam.bandId,
                                                         cjam.name,
                                                         fmt::format("owned by [{}], with:\n{}", cjam.owner, fmt::join(cjam.members, "\n")),
                                                         cjam.rifftime / 1000 );    // riff time is unix-nano
        pjd.m_riffCount = cjam.riffCount;
    }

    for ( auto& innerVec : m_idxSortedByTime )
        innerVec.clear();
    for ( auto& innerVec : m_idxSortedByName )
        innerVec.clear();
    for ( auto& innerVec : m_idxSortedByRiffs )
        innerVec.clear();

    m_jamCouchIDToJamIndexMap.clear();

    for ( const auto jamType : cEachJamType )
    {
        const std::vector< Data >* dataArray = getArrayPtrForType( jamType );
        ABSL_ASSERT( dataArray );

        const size_t jamTypeIndex = (size_t)jamType;

        for ( size_t idx = 0; idx < dataArray->size(); idx++ )
        {
            m_idxSortedByTime[jamTypeIndex].push_back( idx );
            m_idxSortedByName[jamTypeIndex].push_back( idx );
            m_idxSortedByRiffs[jamTypeIndex].push_back( idx );

            // insert_or_assign allows overwrite; given the jam-type ordering, this means items that exist in our
            // subscribed jams will correctly take precedence over ones in the generic public jam archive (as we are 
            // updating the riff counts etc for those regularly, whereas we do not do so for the general jam archive at runtime)
            m_jamCouchIDToJamIndexMap.insert_or_assign( dataArray->at(idx).m_jamCID, CacheIndex( jamType, idx ) );
        }

        std::sort( m_idxSortedByTime[jamTypeIndex].begin(), m_idxSortedByTime[jamTypeIndex].end(),
            [dataArray]( const size_t lhs, const size_t rhs ) -> bool
            {
                // newest first
                return dataArray->at(lhs).m_timestampOrdering > dataArray->at(rhs).m_timestampOrdering;
            });

        std::sort( m_idxSortedByName[jamTypeIndex].begin(), m_idxSortedByName[jamTypeIndex].end(),
            [dataArray]( const size_t lhs, const size_t rhs ) -> bool
            {
                return base::StrToLwrExt( dataArray->at( lhs ).m_displayName ) < base::StrToLwrExt( dataArray->at( rhs ).m_displayName );
            });

        std::sort( m_idxSortedByRiffs[jamTypeIndex].begin(), m_idxSortedByRiffs[jamTypeIndex].end(),
            [dataArray]( const size_t lhs, const size_t rhs ) -> bool
            {
                // largest first
                return dataArray->at(lhs).m_riffCount > dataArray->at(rhs).m_riffCount;
            });
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
            data.m_timestampOrderingDescription.clear();

        for ( Data& data : m_jamDataUserSubscribed )
            data.m_timestampOrderingDescription = spacetime::datestampStringFromUnix( data.m_timestampOrdering );

        for ( Data& data : m_jamDataCollectibles )
            data.m_timestampOrderingDescription = spacetime::datestampStringFromUnix( data.m_timestampOrdering );
    }
}

} // namespace cache
} // namespace endlesss
