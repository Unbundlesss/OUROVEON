//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "base/id.hash.h"
#include "base/spacetime.h"

#include "core.types.h"
#include "ssp/isamplestreamprocessor.h"

#include "endlesss/api.h"

namespace config { namespace endlesss { struct API; } }

namespace endlesss {
namespace cache { struct Stems; }
namespace live {

struct Stem; using StemPtr = std::shared_ptr<Stem>;

// ---------------------------------------------------------------------------------------------------------------------
struct Riff
{
    // unique type for hash values of riff Couch IDs (for quick and dirty equality checks and the like)
    struct _cid_hash {};
    using RiffCIDHash = base::id::HashWrapper<_cid_hash>;

    static RiffCIDHash computeHashForRiffCID( const endlesss::types::RiffCouchID& riffCID );

    Riff( const endlesss::types::RiffComplete& riffData, const int32_t targetSampleRate );
    ~Riff();

    void fetch( const api::NetConfiguration& ncfg, endlesss::cache::Stems& stemCache, tf::Executor& taskExecutor );

    void exportToDisk( const std::function< ssp::SampleStreamProcessorInstance( const uint32_t stemIndex, const endlesss::live::Stem& stemData ) >& diskWriterForStem );

    struct RiffTimingDetails
    {
        inline void ComputeProgressionAtSample( const uint64_t sampleIndex, double& percentage, int32_t& currentBar, int32_t& currentBarSegment ) const
        {
            const double         sampleTime = (double)sampleIndex * m_rcpSampleRate;
            const double    riffWrappedTime = std::fmod( sampleTime, m_lengthInSec );
            const double     riffPercentage = (riffWrappedTime / m_lengthInSec);
            const double segmentWrappedTime = std::fmod( sampleTime, m_lengthInSecPerBar );
            const double  segmentPercentage = (segmentWrappedTime / m_lengthInSecPerBar);

            percentage = riffPercentage;
            currentBar = (uint32_t)std::floor( riffPercentage * m_barCount );
            currentBarSegment = (int32_t)(segmentPercentage * (double)m_quarterBeats);
        }

        int32_t         m_quarterBeats;     // X / 4 time signature
        int32_t         m_barCount;         // how many bars that represent a riff; note that this can be both <=> 8 given
                                            // that endlesss supports having longer stems from (say) a 16/4 riff present in a 4/4 and
                                            // they appear to play in their entirety. It also has a maximum length of 60 sec (i think) 
                                            // for stems, so sometimes we have to clamp to < 8
        float           m_bps;
        float           m_bpm;
        
        // length in seconds
        double          m_lengthInSec;
        double          m_lengthInSecPerBar;

        // length in samples
        uint32_t        m_lengthInSamples;
        uint32_t        m_lengthInSamplesPerBar;

        int32_t         m_longestStemInBars;

        double          m_rcpSampleRate;
    };

    inline const RiffTimingDetails& getTimingDetails() const { return m_timingDetails; }


    enum class SyncState
    {
        Waiting,
        Working,
        Failed,
        Success
    };
    inline SyncState getSyncState() const { return m_syncState; }


    inline RiffCIDHash getCIDHash() const { return m_computedRiffCouchHash; }

public:

    endlesss::types::RiffComplete           m_riffData;
    uint32_t                                m_targetSampleRate;
    SyncState                               m_syncState;
    RiffTimingDetails                       m_timingDetails;

    std::vector<endlesss::live::StemPtr>    m_stemOwnership;
    std::vector<endlesss::live::Stem*>      m_stemPtrs;
    std::vector<float>                      m_stemGains;
    std::vector<float>                      m_stemLengthInSec;
    std::vector<float>                      m_stemTimeScales;
    std::vector<int32_t>                    m_stemRepetitions;
    std::vector<uint32_t>                   m_stemLengthInSamples;

    base::spacetime::InSeconds              m_stTimestamp;

    // set of handy preformatted strings for use in UI rendering
    std::string                             m_uiTimestamp;              // prettified riff submission timestamp
    std::string                             m_uiJamUppercase;           // THE JAM NAME
    std::string                             m_uiIdentity;               // riff owner username
    std::string                             m_uiDetails;                // compact list of riff stats (BPM, etc)

protected:
    RiffCIDHash                             m_computedRiffCouchHash;
};

using RiffPtr = std::shared_ptr<Riff>;

} // namespace live
} // namespace endlesss
