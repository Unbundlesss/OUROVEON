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

namespace endlesss {

namespace live { struct Stem; using StemPtr = std::shared_ptr<Stem>; }
namespace api  { struct NetConfiguration; }

namespace cache {

// ---------------------------------------------------------------------------------------------------------------------
struct Stems
{
    Stems() = default;

    // inhibit accidental copying of the whole cache
    Stems( const Stems& rhs ) = delete;
    Stems& operator=( const Stems& ) = delete;


    bool initialise( 
        const fs::path& cachePath,          // the root path of where to build the stored stems
        const uint32_t targetSampleRate     // the chosen sample rate, stems will be resampled to this if they don't match
    );

    endlesss::live::StemPtr request( const endlesss::types::Stem& stemData );

    // synchronously lock & garbage collect the cache
    void prune( const uint32_t generationsToKeep = 50 );

    // given a stem ID, return a suitable path to write the cached data to
    fs::path getCachePathForStem( const endlesss::types::StemCouchID& stemDocumentID ) const;

private:
    
    fs::path            m_cacheStemRoot;

    using StemDictionary = robin_hood::unordered_flat_map< endlesss::types::StemCouchID, endlesss::live::StemPtr, cid_hash<endlesss::types::StemCouchID> >;
    using StemUsage      = robin_hood::unordered_flat_map< endlesss::types::StemCouchID, uint32_t, cid_hash<endlesss::types::StemCouchID> >;

    StemDictionary      m_stems;
    StemUsage           m_usages;

    uint32_t            m_targetSampleRate;
    uint32_t            m_stemGeneration;
    std::mutex          m_pruneLock;
};

} // namespace cache
} // namespace endlesss
