//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "base/construction.h"
#include "endlesss/core.types.h"

namespace endlesss {

namespace live { struct Stem; using StemPtr = std::shared_ptr<Stem>; }
namespace api  { struct NetConfiguration; }

namespace cache {

// ---------------------------------------------------------------------------------------------------------------------
// 
struct Stems
{
    DECLARE_NO_COPY_NO_MOVE( Stems );


    Stems();

    absl::Status initialise( 
        const fs::path& cachePath,          // the root path of where to build the stored stems
        const uint32_t targetSampleRate     // the chosen sample rate, stems will be resampled to this if they don't match
    );

    ouro_nodiscard endlesss::live::StemPtr request( const endlesss::types::Stem& stemData );

    // tot up all live stems' approximate memory usage; not const as it locks the mutex, dont call it every frame
    ouro_nodiscard std::size_t estimateMemoryUsageBytes();

    // synchronously lock & garbage collect the cache
    void lockAndPrune( const bool verbose, const uint32_t generationsToKeep = 64 );

    // given a stem ID, return a suitable path to write the cached data to
    ouro_nodiscard fs::path getCachePathForStem( const endlesss::types::StemCouchID& stemDocumentID ) const;


private:
    
    fs::path            m_cacheStemRoot;

    using StemDictionary = absl::flat_hash_map< endlesss::types::StemCouchID, endlesss::live::StemPtr >;
    using StemUsage      = absl::flat_hash_map< endlesss::types::StemCouchID, uint32_t >;

    StemDictionary      m_stems;
    StemUsage           m_usages;

    uint32_t            m_targetSampleRate = 0;
    uint32_t            m_stemGeneration = 0;
    std::mutex          m_pruneLock;
};

} // namespace cache
} // namespace endlesss
