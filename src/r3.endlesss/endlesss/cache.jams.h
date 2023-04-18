//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "endlesss/config.h"

namespace endlesss {

namespace api { struct NetConfiguration; }

namespace cache {

// ---------------------------------------------------------------------------------------------------------------------
// The jams cache holds the top-level metadata stash of all the jams the user is aware of, both privately subscribed
// and all the current publics
//
struct Jams
{
    static constexpr auto cFilename = "cache.jams.json";

    struct Data
    {
        Data() = default;
        Data( const std::string& jamCID,
              const std::string& name,
              const std::string& description,
              const int64_t timestampOrdering )
            : m_jamCID( jamCID )
            , m_displayName( name )
            , m_description( description )
            , m_timestampOrdering( timestampOrdering )
        {}

        endlesss::types::JamCouchID     m_jamCID;
        std::string                     m_displayName;
        std::string                     m_description;
        int32_t                         m_riffCount             = -1;
        int64_t                         m_timestampOrdering     = -1;

        int64_t                         m_timestampEarliestStem = -1;
        int64_t                         m_timestampLatestStem   = -1;


        std::string                     m_timestampOrderingDescription; // not serialised, built on load

        template<class Archive>
        inline void serialize( Archive& archive )
        {
            archive( CEREAL_NVP( m_jamCID )
                   , CEREAL_NVP( m_displayName )
                   , CEREAL_NVP( m_description )
                   , CEREAL_OPTIONAL_NVP( m_riffCount )     // may be calculated by the app and cached
                   , CEREAL_NVP( m_timestampOrdering )
                   , CEREAL_NVP( m_timestampEarliestStem )
                   , CEREAL_NVP( m_timestampLatestStem )
            );
        }
    };


    enum class JamType
    {
        PublicArchive   = 0,          // static snapshot of all known public jams, scraped from Discord/etc
        PublicJoinIn    = 1,          // dynamic list of currently active "join in" public jams
        UserSubscribed  = 2,          // dynamic list of user's subscribed-to list of "My Jams"
        Collectible     = 3,          // the NFT stuff
    };
    static constexpr size_t cJamTypeCount = 4;
    static constexpr std::array< JamType, cJamTypeCount > cEachJamType = {
        JamType::PublicArchive,
        JamType::PublicJoinIn,
        JamType::UserSubscribed,
        JamType::Collectible
    };


    // immutable pairing of [ type + index ]
    struct CacheIndex
    {
        CacheIndex()
            : m_valid(false)
        {}
        CacheIndex( const JamType type, const size_t index )
            : m_type(type)
            , m_index(index)
            , m_valid(true)
        {}
        CacheIndex& operator=( const CacheIndex& ) = default;
        CacheIndex( const CacheIndex& ) = default;

        ouro_nodiscard constexpr JamType type() const { return m_type;  }
        ouro_nodiscard constexpr size_t index() const { return m_index; }
        ouro_nodiscard constexpr bool valid()   const { return m_valid; }
    private:
        JamType m_type;
        size_t  m_index;
        bool    m_valid;
    };



    bool load( const config::IPathProvider& pathProvider );
    bool save( const config::IPathProvider& pathProvider );

    ouro_nodiscard constexpr bool hasJamData() const
    { 
        // overbearing but ensures on the ouro boot page we aren't talking to vectors that are being
        // heavily modified during a postProcessNewData() call (leading to very rare crash)
        std::scoped_lock<std::mutex> lockProc( m_dataProcessMutex );

        return !m_jamDataJoinIn.empty() &&
               !m_jamDataUserSubscribed.empty();
    }


    enum class AsyncFetchState
    {
        None,
        Working,
        Failed,
        Success
    };
    using AsyncCallback = std::function< void( const AsyncFetchState state, const std::string& status )>;

    // fetch the users' latest jam membership state + list of active publics from the servers
    void asyncCacheRebuild(
        const endlesss::api::NetConfiguration& apiCfg,
        const bool syncCollectibles,                        // go fetch the collectible jam data from (buggy) web endpoints?
        const bool syncRiffCounts,                          // fetch riff counts for all private jams (may take a while)
        tf::Taskflow& taskFlow,
        const AsyncCallback& asyncCallback );

    ouro_nodiscard constexpr const std::string& getCacheFileState() const { return m_cacheFileState; }



    ouro_nodiscard bool getCacheIndexForDatabaseID( const endlesss::types::JamCouchID& couchID, CacheIndex& outIndex ) const
    {
        const auto indexIter = m_jamCouchIDToJamIndexMap.find( couchID );
        if ( indexIter != m_jamCouchIDToJamIndexMap.end() )
        {
            outIndex = indexIter->second;
            return true;
        }
        return false;
    }
    
    bool loadDataForCacheIndex( const CacheIndex& index, Data& result ) const;

    // convenience function to go from couchID to data without fetching cache index
    ouro_nodiscard bool loadDataForDatabaseID( const endlesss::types::JamCouchID& couchID, Data& result ) const
    {
        CacheIndex ci;
        if ( getCacheIndexForDatabaseID( couchID, ci ) )
        {
            return loadDataForCacheIndex( ci, result );
        }
        return false;
    }

    // convenience function to fetch any known riff counts for a cached jam, or 0 if unknown, -1 on any error
    ouro_nodiscard int32_t loadKnownRiffCountForDatabaseID( const endlesss::types::JamCouchID& couchID ) const
    {
        CacheIndex ci;
        if ( getCacheIndexForDatabaseID( couchID, ci ) )
        {
            const std::vector< Data >* dataArray = getArrayPtrForType( ci.type() );
            return dataArray->at( ci.index() ).m_riffCount;
        }
        return -1;
    }


    // #HDD replace with metaenum for imgui use?
    enum IteratorSortingOption : int32_t
    {
        eIterateSortByTime,
        eIterateSortByName,
        eIterateSortByRiffs,
    };

    // walk particular type of jam data, sorted by given order
    template< typename _iterFn >
    inline void iterateJams( const _iterFn& iteratorFunction, const JamType type, const int32_t sortOption ) const
    {
        const size_t typeAsIndex = (size_t)type;

        const std::vector< Data >* dataArray = getArrayPtrForType( type );
        assert( dataArray != nullptr );
        if ( dataArray == nullptr )
            return;

        switch ( sortOption )
        {
            case eIterateSortByTime:
            {
                for ( const size_t jIndex : m_idxSortedByTime[typeAsIndex] )
                    iteratorFunction( dataArray->at( jIndex ) );
            }
            break;

            case eIterateSortByName:
            {
                for ( const size_t jIndex : m_idxSortedByName[typeAsIndex] )
                    iteratorFunction( dataArray->at( jIndex ) );
            }
            break;

            case eIterateSortByRiffs:
            {
                for ( const size_t jIndex : m_idxSortedByRiffs[typeAsIndex] )
                    iteratorFunction( dataArray->at( jIndex ) );
            }
            break;
        }
    }


    // plain, unsorted walk across all jam data we have
    template< typename _iterFn >
    inline void iterateAllJams( const _iterFn& iteratorFunction ) const
    {
        for ( const Data& data : m_jamDataPublicArchive )
            iteratorFunction( data );
        for ( const Data& data : m_jamDataCollectibles )
            iteratorFunction( data );
        for ( const Data& data : m_jamDataJoinIn )
            iteratorFunction( data );
        for ( const Data& data : m_jamDataUserSubscribed )
            iteratorFunction( data );
    }


private:

    using JamIndicesPerType = std::array< std::vector< size_t >, cJamTypeCount >;

    using JamCouchIDToJamIndexMap = absl::flat_hash_map< endlesss::types::JamCouchID, CacheIndex >;

    // externally siphoned manifest of all known public jams from various sources; merged with live data
    // this is always loaded from the shared data, it isn't stored to the dynamic cache on disk
    config::endlesss::PublicJamManifest
                                m_configEndlesssPublics;

    // .. similar data block for the marketplace/collectible jams
    config::endlesss::CollectibleJamManifest
                                m_configEndlesssCollectibles;


    uint32_t                    m_jamSerialisedVersion = 0;     // version of the data on disk, used to force obsolescence 
    std::vector< Data >         m_jamDataPublicArchive;         // conversion of data from PublicJamManifest to Data type
    std::vector< Data >         m_jamDataCollectibles;          // conversion of data from CollectibleJamManifest to Data type
    std::vector< Data >         m_jamDataJoinIn;                // data from servers (persisted to disk)
    std::vector< Data >         m_jamDataUserSubscribed;        // data from servers (persisted to disk)

    JamIndicesPerType           m_idxSortedByTime;
    JamIndicesPerType           m_idxSortedByName;
    JamIndicesPerType           m_idxSortedByRiffs;


    std::string                 m_cacheFileState;

    JamCouchIDToJamIndexMap     m_jamCouchIDToJamIndexMap;

    mutable std::mutex          m_dataProcessMutex;

    void postProcessNewData();


    ouro_nodiscard constexpr const std::vector< Data >* getArrayPtrForType( const JamType type ) const
    {
        switch ( type )
        {
            case JamType::PublicArchive:    return &m_jamDataPublicArchive;
            case JamType::PublicJoinIn:     return &m_jamDataJoinIn;
            case JamType::UserSubscribed:   return &m_jamDataUserSubscribed;
            case JamType::Collectible:      return &m_jamDataCollectibles;
        }
        return nullptr;
    }

    template<class Archive>
    inline void serialize( Archive& archive )
    { 
        archive( CEREAL_NVP( m_jamSerialisedVersion ),
                 CEREAL_NVP( m_jamDataJoinIn ),
                 CEREAL_NVP( m_jamDataUserSubscribed ) );
    }
    template<class Archive>
    inline void serialize( Archive& archive ) const 
    { 
        archive( CEREAL_NVP( m_jamSerialisedVersion ), 
                 CEREAL_NVP( m_jamDataJoinIn ),
                 CEREAL_NVP( m_jamDataUserSubscribed ) );
    }
};

} // namespace cache
} // namespace endlesss
