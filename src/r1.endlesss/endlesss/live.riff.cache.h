//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "endlesss/core.types.h"
#include "endlesss/live.riff.h"

namespace endlesss {
namespace live {

#define RIFF_CACHE_VERBOSE_DEBUG   0

#if RIFF_CACHE_VERBOSE_DEBUG
#define riff_verbose_log(_msg)   debugLog(_msg)
#else
#define riff_verbose_log(_msg)
#endif // RIFF_CACHE_VERBOSE_DEBUG

// simple least-recently-used cache for live Riff instances; ideal for apps that want to keep some recently played
// bits in memory for faster scheduling rather than pulling back from disk
struct RiffCacheLRU
{
    RiffCacheLRU( std::size_t cacheSize )
        : m_cacheSize( cacheSize )
        , m_used( 0 )
    {
        m_cache.reserve( m_cacheSize );
        m_age.reserve( m_cacheSize );

        for ( auto i = 0; i< m_cacheSize; i++ )
        {
            m_cache.emplace_back( nullptr );
            m_age.emplace_back( 0 );
        }
    }

    inline bool search( const endlesss::types::RiffCouchID& cid, endlesss::live::RiffPtr& result )
    {
        // early out on empty cache
        if ( m_used == 0 )
            return false;

        // compare CIDs by hash
        const auto incomingRiffCIDHash = endlesss::live::Riff::computeHashForRiffCID( cid );

        for ( auto idx = 0; idx < m_used; idx++ )
        {
            assert( m_cache[idx] != nullptr );

            m_age[idx] ++;

            if ( m_cache[idx]->getCIDHash() == incomingRiffCIDHash )
            {
                // reset age
                m_age[idx] = 0;

                result = m_cache[idx];

                // complete rest of run before we're done
                idx++;
                for ( ; idx < m_used; idx++ )
                    m_age[idx] ++;

                riff_verbose_log( "search-hit" );

                return true;
            }
        }
        return false;
    }

    inline void store( endlesss::live::RiffPtr& riffPtr )
    {
        if ( m_used < m_cacheSize )
        {
            // store new value in unused slot
            m_cache[m_used] = riffPtr;
            
            // age all other entries
            for ( auto prvI = 0; prvI < m_used; prvI++ )
                m_age[prvI] ++;

            m_used++;

            riff_verbose_log( "store-unfilled" );
            return;
        }

        int32_t oldestAge = -1;
        int32_t oldestIndex = -1;
        for ( auto idx = 0; idx < m_cacheSize; idx++ )
        {
            if ( m_age[idx] > oldestAge )
            {
                oldestAge   = m_age[idx];
                oldestIndex = idx;
            }

            // age as we go
            m_age[idx] ++;
        }
        assert( oldestIndex >= 0 );

        m_cache[oldestIndex] = riffPtr;
        m_age[oldestIndex] = 0;

        riff_verbose_log("store-added-new");
    }

#if RIFF_CACHE_VERBOSE_DEBUG
    inline void debugLog(const std::string& context)
    {
        for ( auto idx = 0; idx < m_used; idx++ )
        {
            blog::app( "[R$] [{:30}] {} = {}, {}", context, idx, m_age[idx], (m_cache[idx] == nullptr) ? "NONE" : (m_cache[idx]->m_riffData.riff.couchID) );
        }
    }
#endif // RIFF_CACHE_VERBOSE_DEBUG

    std::size_t                              m_cacheSize;
    std::vector< endlesss::live::RiffPtr >   m_cache;
    std::vector< int32_t >                   m_age;
    int32_t                                  m_used = 0;
};

#undef riff_verbose_log

} // namespace live
} // namespace endlesss
