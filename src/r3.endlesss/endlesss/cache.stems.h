//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#pragma once

#include "base/construction.h"
#include "endlesss/core.types.h"
#include "endlesss/live.stem.h"

namespace endlesss {

namespace api  { struct NetConfiguration; }

namespace cache {

// ---------------------------------------------------------------------------------------------------------------------
// 
struct Stems
{
    DECLARE_NO_COPY_NO_MOVE( Stems );

    enum class CacheVersion
    {
        Version1,       // pre 0.7.7; stems were stored in a single root directory, partitioned by the initial stem ID letter
        Version2        // stems are now organised per-jam-ID, still partitioned by initial stem ID inside each sub-folder
    };
    // get path root relative to the ouroveon cache/common path
    ouro_nodiscard static fs::path getCachePathRoot( CacheVersion cv );

    ouro_nodiscard static fs::path getCachePathForStemData(
        const fs::path& cacheRoot,
        const endlesss::types::JamCouchID& jamCID,
        const endlesss::types::StemCouchID& stemCID
    );


    Stems();

    absl::Status initialise( 
        const fs::path& cachePath,          // the root path of where to build the stored stems
        const uint32_t targetSampleRate     // the chosen sample rate, stems will be resampled to this if they don't match
    );

    ouro_nodiscard endlesss::live::StemPtr request( const endlesss::types::Stem& stemData );

    // tot up all live stems' approximate memory usage; not const as it locks the mutex, dont call it every frame
    ouro_nodiscard std::size_t estimateMemoryUsageBytes();

    ouro_nodiscard fs::path getCacheRootPath() const { return m_cacheStemRoot; }

    // synchronously lock & garbage collect the cache
    void lockAndPrune( const bool verbose, const uint32_t generationsToKeep = 64 );

    // given stem data, return a suitable path to write the cached data to
    ouro_nodiscard fs::path getCachePathForStem( const endlesss::types::Stem& stemData ) const;

    // return the single shared instance of read-only stem processing state
    // used by riff resolving code after fetching audio data in
    const endlesss::live::Stem::Processing& getStemProcessing() const
    {
        ABSL_ASSERT( m_processing != nullptr );
        return *m_processing.get();
    }

private:

    using StemProcessing    = endlesss::live::Stem::Processing::UPtr;
    using StemDictionary    = absl::flat_hash_map< endlesss::types::StemCouchID, endlesss::live::StemPtr >;
    using StemUsage         = absl::flat_hash_map< endlesss::types::StemCouchID, uint32_t >;
    
    fs::path            m_cacheStemRoot;

    StemProcessing      m_processing;

    StemDictionary      m_stems;
    StemUsage           m_usages;

    uint32_t            m_targetSampleRate = 0;
    uint32_t            m_stemGeneration = 0;
    std::mutex          m_pruneLock;
};

} // namespace cache
} // namespace endlesss
