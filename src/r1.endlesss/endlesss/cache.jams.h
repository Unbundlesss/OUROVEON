//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

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
              const std::string& timestampString,
              const int64_t timestampSeconds,
              bool isPublic )
            : m_jamCID( jamCID )
            , m_displayName( name )
            , m_timestampISO8601( timestampString )
            , m_timestampSeconds( timestampSeconds )
            , m_isPublic( isPublic )
        {}

        endlesss::types::JamCouchID     m_jamCID;           // couch ID
        std::string                     m_displayName;
        std::string                     m_description;
        std::string                     m_timestampISO8601;
        int64_t                         m_timestampSeconds;
        bool                            m_isPublic;

        template<class Archive>
        inline void serialize( Archive& archive )
        {
            archive( CEREAL_NVP( m_jamCID )
                   , CEREAL_NVP( m_displayName )
                   , CEREAL_NVP( m_description )
                   , CEREAL_NVP( m_timestampISO8601 )
                   , CEREAL_NVP( m_timestampSeconds )
                   , CEREAL_NVP( m_isPublic )
            );
        }
    };

    enum class AsyncFetchState
    {
        None,
        Working,
        Failed,
        Success
    };
    using AsyncCallback = std::function< void( const AsyncFetchState state, const std::string& status )>;

    bool load( const fs::path& cachePath );
    bool save( const fs::path& cachePath );
    void asyncCacheRebuild( const endlesss::api::NetConfiguration& apiCfg, tf::Taskflow& taskFlow, const AsyncCallback& asyncCallback );


    inline const std::string& getCacheFileState() const { return m_cacheFileState; }

    inline bool hasJamData() const { return !m_jamData.empty(); }
    inline size_t getJamCount() const { return m_jamData.size(); }

    inline void unpackJamNames( std::vector<const char*>& jamNameData ) const
    {
        jamNameData.clear();
        jamNameData.reserve( m_jamData.size() );
        for ( const auto& jamData : m_jamData )
            jamNameData.push_back( jamData.m_displayName.c_str() );
    }

    inline const endlesss::types::JamCouchID& getDatabaseID( const size_t index ) const { return m_jamData[index].m_jamCID; }
    inline const std::string& getDisplayName( const size_t index ) const { return m_jamData[index].m_displayName; }

    inline bool getIndexForDatabaseID( const std::string& databaseID, size_t& outIndex ) const
    {
        const auto indexIter = m_jamDbToIndexMap.find( databaseID );
        if ( indexIter != m_jamDbToIndexMap.end() )
        {
            outIndex = indexIter->second;
            return true;
        }
        return false;
    }

    enum IteratorSortingOption : int32_t
    {
        eIterateSortDefault,
        eIterateSortByName
    };

    template< typename _iterFn >
    inline void iterateJams( const _iterFn& iteratorFunction, int32_t sortOption ) const
    {
        if ( sortOption == eIterateSortDefault )
        {
            for ( const auto& jamData : m_jamData )
                iteratorFunction( jamData );
        }
        else
        {
            for ( const auto& jamData : m_jamDataSortedByName )
                iteratorFunction( jamData );
        }
    }

private:

    using JamDbToIndexMap = std::unordered_map< std::string, size_t >;

    std::string                 m_cacheFileState;

    std::vector< Data >         m_jamData;
    std::vector< Data >         m_jamDataSortedByName;
    JamDbToIndexMap             m_jamDbToIndexMap;


    inline void postProcessNewData()
    {
        // sort by join timestamp
        sort( m_jamData.begin(), m_jamData.end(),
            []( const Data& a, const Data& b ) -> bool
            {
                return a.m_timestampSeconds < b.m_timestampSeconds;
            } );

        // build a sorted-by-name list
        m_jamDataSortedByName.assign( m_jamData.begin(), m_jamData.end() );
        sort( m_jamDataSortedByName.begin(), m_jamDataSortedByName.end(),
            []( const Data& a, const Data& b ) -> bool
            {
                return a.m_displayName < b.m_displayName;
            });

        m_jamDbToIndexMap.clear();
        for ( size_t idx = 0; idx < m_jamData.size(); idx++ )
        {
            m_jamDbToIndexMap.try_emplace( m_jamData[idx].m_jamCID, idx );
        }
    }

    template<class Archive>
    inline void serialize( Archive& archive )       { archive( CEREAL_NVP( m_jamData ) ); }
    template<class Archive>
    inline void serialize( Archive& archive ) const { archive( CEREAL_NVP( m_jamData ) ); }
};

} // namespace cache
} // namespace endlesss
