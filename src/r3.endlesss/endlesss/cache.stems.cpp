//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"

#include "app/module.frontend.h"

#include "filesys/fsutil.h"
#include "spacetime/moment.h"

#include "endlesss/cache.stems.h"
#include "endlesss/live.stem.h"

using recursive_directory_iterator = std::filesystem::recursive_directory_iterator;

namespace endlesss {
namespace cache {

// ---------------------------------------------------------------------------------------------------------------------
Stems::Stems()
{
    m_stems.reserve( 2048 );
    m_usages.reserve( 2048 );
}

// ---------------------------------------------------------------------------------------------------------------------
absl::Status Stems::initialise( const fs::path& cachePath, const uint32_t targetSampleRate )
{
    static const fs::path stemSubdir( "stem" );

    m_cacheStemRoot     = cachePath / stemSubdir;
    m_targetSampleRate  = targetSampleRate;

    const auto stemRootStatus = filesys::ensureDirectoryExists( m_cacheStemRoot );
    if ( !stemRootStatus.ok() )
    {
        return absl::PermissionDeniedError( fmt::format(
            "Failed to create directory inside [{}], {}", m_cacheStemRoot.string(), stemRootStatus.ToString() ) );
    }

    m_stemGeneration = 0;

//     spacetime::Moment pruneTimer;
//     for ( const auto& dirEntry : recursive_directory_iterator( m_cacheStemRoot ) )
//     {
//         if ( dirEntry.is_directory() )
//             continue;
//         const auto stat = dirEntry.path().stem();
//     }
//     blog::stem( "stem cache examination took {}", pruneTimer.deltaMs() );

    return absl::OkStatus();
}

// ---------------------------------------------------------------------------------------------------------------------
endlesss::live::StemPtr Stems::request( const endlesss::types::Stem& stemData )
{
    ABSL_ASSERT( m_targetSampleRate > 0 );  // ensure initialise() has been run
    {
        std::scoped_lock<std::mutex> lock( m_pruneLock );

        m_stemGeneration++;

        const auto& stemDocumentID = stemData.couchID;

        auto stemIter = m_stems.find( stemDocumentID );
        if ( stemIter == m_stems.end() )
        {
            auto newStem = std::make_shared<endlesss::live::Stem>( stemData, m_targetSampleRate );

            m_usages.emplace( stemDocumentID, m_stemGeneration );
            m_stems.emplace( stemDocumentID, newStem );
            return newStem;
        }
        else
        {
            ABSL_ASSERT( m_usages.find( stemDocumentID ) != m_usages.end() );
            m_usages[stemDocumentID] = m_stemGeneration;

            return stemIter->second;
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
ouro_nodiscard std::size_t Stems::estimateMemoryUsageBytes()
{
    std::size_t total = 0;
    {
        std::scoped_lock<std::mutex> lock( m_pruneLock );
        for ( const auto& stem : m_stems )
        {
            total += stem.second->estimateMemoryUsageBytes();
        }
    }
    return total;
}

// ---------------------------------------------------------------------------------------------------------------------
void Stems::lockAndPrune( const bool verbose, const uint32_t generationsToKeep )
{
    spacetime::Moment pruneTimer;

    {
        std::scoped_lock<std::mutex> lock( m_pruneLock );

        if ( m_stemGeneration < generationsToKeep )
        {
            if ( verbose )
                blog::stem( "stem cache pruning aborted, generation index is too low : {} (need at least {})", m_stemGeneration, generationsToKeep );

            return;
        }

        if ( verbose )
            blog::stem( "stem cache prune : generation {}", m_stemGeneration );

        StemDictionary keptStems;
        StemUsage      keptUsages;

        const uint32_t pruneGenerationsTo = m_stemGeneration - generationsToKeep;

        for ( const auto& currentUsage : m_usages )
        {
            bool keepThisStem = true;

            auto stemIter = m_stems.find( currentUsage.first );
            ABSL_ASSERT( stemIter != m_stems.end() );

            if ( currentUsage.second < pruneGenerationsTo )
            {
                keepThisStem = false;

                // check that the usage has fallen to 1; otherwise something somewhere is holding onto this
                const auto stemUsage = stemIter->second.use_count();
                if ( stemUsage > 1 )
                    keepThisStem = true;
            }

            if ( keepThisStem )
            {
                keptUsages.emplace( currentUsage );
                keptStems.emplace( currentUsage.first, stemIter->second );
            }
        }

        const std::size_t beforeSize = m_stems.size();
        if ( verbose )
            blog::stem( "stem cache prune : had {} stems ...", beforeSize );

        m_stems = std::move( keptStems );
        m_usages = std::move( keptUsages );

        const std::size_t afterSize = m_stems.size();
        if ( verbose )
            blog::stem( "stem cache prune : ... now has {}", afterSize );

        blog::stem( "stem cache prune trimmed {} entries, took {}", (beforeSize - afterSize), pruneTimer.deltaMs() );
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// produce a path to store the stem 
// 
// NB changing this may invalidate existing stem caches
//
fs::path Stems::getCachePathForStem( const endlesss::types::StemCouchID& stemDocumentID ) const
{
    // pluck the first character from the couch ID to partition the cache folder
    const std::string stemRoot = stemDocumentID.value().substr(0, 1);

    // append to the base cache directory
    return (m_cacheStemRoot / stemRoot);
}

} // namespace cache
} // namespace endlesss
