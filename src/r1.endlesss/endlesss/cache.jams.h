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
        UserSubscribed  = 2           // dynamic list of user's subscribed-to list of "My Jams"
    };
    static constexpr size_t cJamTypeCount = 3;
    static constexpr std::array< JamType, cJamTypeCount > cEachJamType = {
        JamType::PublicArchive,
        JamType::PublicJoinIn,
        JamType::UserSubscribed
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

        constexpr JamType type() const { return m_type;  }
        constexpr size_t index() const { return m_index; }
        constexpr bool valid()   const { return m_valid; }
    private:
        JamType m_type;
        size_t  m_index;
        bool    m_valid;
    };



    bool load( const config::IPathProvider& pathProvider );
    bool save( const config::IPathProvider& pathProvider );

    inline bool hasJamData() const
    { 
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
    void asyncCacheRebuild( const endlesss::api::NetConfiguration& apiCfg, tf::Taskflow& taskFlow, const AsyncCallback& asyncCallback );

    inline const std::string& getCacheFileState() const { return m_cacheFileState; }



    inline bool getCacheIndexForDatabaseID( const endlesss::types::JamCouchID& couchID, CacheIndex& outIndex ) const
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
    inline bool loadDataForDatabaseID( const endlesss::types::JamCouchID& couchID, Data& result ) const
    {
        CacheIndex ci;
        if ( getCacheIndexForDatabaseID( couchID, ci ) )
        {
            return loadDataForCacheIndex( ci, result );
        }
        return false;
    }


    // #HDD replace with metaenum for imgui use?
    enum IteratorSortingOption : int32_t
    {
        eIterateSortByTime,
        eIterateSortByName
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

        if ( sortOption == eIterateSortByTime )
        {
            for ( const size_t jIndex : m_idxSortedByTime[typeAsIndex] )
                iteratorFunction( dataArray->at(jIndex) );
        }
        else
        {
            for ( const size_t jIndex : m_idxSortedByName[typeAsIndex] )
                iteratorFunction( dataArray->at(jIndex) );
        }
    }


    // plain, unsorted walk across all jam data we have
    template< typename _iterFn >
    inline void iterateAllJams( const _iterFn& iteratorFunction ) const
    {
        for ( const Data& data : m_jamDataPublicArchive )
            iteratorFunction( data );
        for ( const Data& data : m_jamDataJoinIn )
            iteratorFunction( data );
        for ( const Data& data : m_jamDataUserSubscribed )
            iteratorFunction( data );
    }


private:

    using JamIndicesPerType = std::array< std::vector< size_t >, cJamTypeCount >;

    using JamCouchIDToJamIndexMap = robin_hood::unordered_flat_map< endlesss::types::JamCouchID, CacheIndex, cid_hash<endlesss::types::JamCouchID> >;

    // externally siphoned manifest of all known public jams from various sources; merged with live data
    // this is always loaded from the shared data, it isn't stored to the dynamic cache on disk
    config::endlesss::PublicJamManifest
                                m_configEndlesssPublics;

    uint32_t                    m_jamSerialisedVersion = 0;     // version of the data on disk, used to force obsolescence 
    std::vector< Data >         m_jamDataPublicArchive;         // conversion of data from PublicJamManifest to Data type
    std::vector< Data >         m_jamDataJoinIn;                // data from servers (persisted to disk)
    std::vector< Data >         m_jamDataUserSubscribed;        // data from servers (persisted to disk)

    JamIndicesPerType           m_idxSortedByTime;
    JamIndicesPerType           m_idxSortedByName;


    std::string                 m_cacheFileState;

    JamCouchIDToJamIndexMap     m_jamCouchIDToJamIndexMap;


    void postProcessNewData();


    const std::vector< Data >* getArrayPtrForType( const JamType type ) const
    {
        switch ( type )
        {
            case JamType::PublicArchive:  return &m_jamDataPublicArchive;
            case JamType::PublicJoinIn:   return &m_jamDataJoinIn;
            case JamType::UserSubscribed: return &m_jamDataUserSubscribed;
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
