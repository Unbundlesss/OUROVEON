//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#pragma once

#include "base/id.hash.h"
#include "spacetime/chronicle.h"

#include "core.types.h"
#include "ssp/isamplestreamprocessor.h"

#include "endlesss/core.services.h"
#include "endlesss/api.h"

namespace config { namespace endlesss { struct rAPI; } }

namespace endlesss {
namespace cache { struct Stems; }
namespace live {

struct Stem; using StemPtr = std::shared_ptr<Stem>;

// ---------------------------------------------------------------------------------------------------------------------
struct RiffProgression
{
    RiffProgression()
    {
        reset();
    }

    constexpr void reset()
    {
        m_playbackPercentage        = 0;
        m_playbackSegmentPercentage = 0;
        m_playbackQuarterPercentage = 0;
        m_playbackBar               = 0;
        m_playbackBarSegment        = 0;
    }

    double      m_playbackPercentage;
    double      m_playbackSegmentPercentage;
    double      m_playbackQuarterPercentage;
    int32_t     m_playbackBar;
    int32_t     m_playbackBarSegment;
    double      m_playbackQuarterTimeSec;
};

// ---------------------------------------------------------------------------------------------------------------------
struct Riff
{
    // unique type for hash values of riff Couch IDs (for quick and dirty equality checks and the like)
    struct _cid_hash {};
    using RiffCIDHash = base::id::HashWrapper<_cid_hash>;

    static RiffCIDHash computeHashForRiffCID( const endlesss::types::RiffCouchID& riffCID );

    Riff( const endlesss::types::RiffComplete& riffData );
    ~Riff();

    void fetch( services::RiffFetchProvider& services );

    using streamProcessorFactoryFn = std::function< ssp::SampleStreamProcessorInstance( const uint32_t stemIndex, const endlesss::live::Stem& stemData ) >;
    void exportToDisk( const streamProcessorFactoryFn& diskWriterForStem, const int32_t sampleOffset );

    struct RiffTimingDetails
    {
        inline void ComputeProgressionAtSample( const uint64_t sampleIndex, RiffProgression& progression ) const
        {
            const double                 sampleTime = (double)sampleIndex * m_rcpSampleRate;
            const double            riffWrappedTime = std::fmod( sampleTime, m_lengthInSec );
            const double             riffPercentage = (riffWrappedTime / m_lengthInSec);
            const double         segmentWrappedTime = std::fmod( sampleTime, m_lengthInSecPerBar );
            const double          segmentPercentage = (segmentWrappedTime / m_lengthInSecPerBar);

            const double        quarterTime = m_lengthInSecPerBar / static_cast<double>(m_quarterBeats);
            const double         quarterWrappedTime = std::fmod( sampleTime, quarterTime );
            const double          quarterPercentage = (segmentWrappedTime / quarterTime);

            progression.m_playbackPercentage        = riffPercentage;
            progression.m_playbackSegmentPercentage = segmentPercentage;
            progression.m_playbackQuarterPercentage = quarterPercentage;
            progression.m_playbackBar               = (uint32_t)std::floor( riffPercentage * m_barCount );
            progression.m_playbackBarSegment        = (int32_t)( segmentPercentage * (double)m_quarterBeats );

            progression.m_playbackQuarterTimeSec    = ( progression.m_playbackQuarterPercentage - static_cast<double>(progression.m_playbackBarSegment) ) * quarterTime;
        }

        int32_t     m_quarterBeats = 0;     // X / 4 time signature
        int32_t     m_barCount = 0;         // how many bars that represent a riff; note that this can be both <=> 8 given
                                            // that endlesss supports having longer stems from (say) a 16/4 riff present in a 4/4 and
                                            // they appear to play in their entirety. It also has a maximum length of 60 sec (i think) 
                                            // for stems, so sometimes we have to clamp to < 8
        float       m_bps = 0;
        float       m_bpm = 0;
        
        // length in seconds
        double      m_lengthInSec = 0;
        double      m_lengthInSecPerBar = 0;

        // length in samples
        uint32_t    m_lengthInSamples = 0;
        uint32_t    m_lengthInSamplesPerBar = 0;

        int32_t     m_longestStemInBars = 0;

        double      m_rcpSampleRate = 0;
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


    // export the riff metadata and anything else of vague (debug) interest into a string for dumping out somewhere
    std::string generateMetadataReport() const;

    // .. via this cereal fn; it includes a bunch of derived and more debug-specific data that won't be interesting to 
    // most. "m_riffData" is likely the only bit that others might actually care about
    template<class Archive>
    inline void metadata( Archive& archive )
    {
        archive( CEREAL_NVP( m_riffData )
               , CEREAL_NVP( m_timingDetails.m_lengthInSamples )
               , CEREAL_NVP( m_timingDetails.m_lengthInSamplesPerBar )
               , CEREAL_NVP( m_timingDetails.m_lengthInSec )
               , CEREAL_NVP( m_timingDetails.m_lengthInSecPerBar )
               , CEREAL_NVP( m_timingDetails.m_longestStemInBars )
               , CEREAL_NVP( m_stemGains )
               , CEREAL_NVP( m_stemLengthInSec )
               , CEREAL_NVP( m_stemTimeScales )
        );
    }

public:

    endlesss::types::RiffComplete           m_riffData;
    SyncState                               m_syncState;
    RiffTimingDetails                       m_timingDetails;

    uint32_t                                m_stemSampleRate;
    std::array<endlesss::live::StemPtr, 8>  m_stemOwnership;
    std::array<endlesss::live::Stem*, 8>    m_stemPtrs;
    std::array<float, 8>                    m_stemGains;
    std::array<float, 8>                    m_stemLengthInSec;
    std::array<float, 8>                    m_stemTimeScales;
    std::array<int32_t, 8>                  m_stemRepetitions;
    std::array<uint32_t, 8>                 m_stemLengthInSamples;

    spacetime::InSeconds                    m_stTimestamp;

    // set of handy pre-formatted strings for use in UI rendering
    std::string                             m_uiTimestamp;              // prettified riff submission timestamp
    std::string                             m_uiJamUppercase;           // THE JAM NAME
    std::string                             m_uiDetails;                // compact list of riff stats (author, BPM, ..)
    std::string                             m_uiDetailsDebug;           // debug stuff for alternative display
    std::string                             m_uiPlaybackDebug;          // some playback debugging info (stem repeats, length in sec, ..)

protected:
    RiffCIDHash                             m_computedRiffCouchHash;
};

using RiffPtr = std::shared_ptr<Riff>;

} // namespace live
} // namespace endlesss
