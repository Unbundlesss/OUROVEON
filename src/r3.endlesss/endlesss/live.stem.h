//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "endlesss/api.h"
#include "core.types.h"

namespace config { namespace endlesss { struct API; } }

namespace endlesss {
namespace live {

// ---------------------------------------------------------------------------------------------------------------------
struct Stem
{
    enum class State
    {
        Empty,
        WorkEnqueued,
        Complete,

        Failed_Http,
        Failed_DataUnderflow,
        Failed_DataOverflow,
        Failed_Vorbis,
    };

    Stem( const types::Stem& stemData, const uint32_t targetSampleRate );
    ~Stem();

    void fetch( const api::NetConfiguration& ncfg, const fs::path& cachePath );

    void fft();

    // stem needs a copy of the analysis task future to ensure that in the unlikely case
    // of destruction arriving before the task is done, we wait to avoid the analysis working with a deleted object
    inline void keepFuture( std::shared_future<void>& analysisFuture )
    {
        assert( !m_analysisFuture.valid() );
        m_analysisFuture = analysisFuture;
    }

    inline bool isAnalysisComplete() const
    {
        return m_hasValidAnalysis;
    }

    constexpr bool hasFailed() const 
    {
        return ( m_state == State::Failed_Http          ||
                 m_state == State::Failed_DataUnderflow ||
                 m_state == State::Failed_DataOverflow  ||
                 m_state == State::Failed_Vorbis );
    }

private:

    struct RawAudioMemory
    {
        RawAudioMemory( size_t size );
        ~RawAudioMemory();

        void allocate( size_t newSize );

        size_t                          m_rawLength;
        size_t                          m_rawReceived;
        uint8_t*                        m_rawAudio;
    };

    // make an attempt to download the stem from the Endlesss CDN; this may fail and that may be because the CDN
    // hasn't actually got the data yet - so we can call this function repeatedly to see if success is possible 
    // after a little delay
    // returns false if something broke; sets the m_state appropriately in that case
    bool attemptRemoteFetch( const api::NetConfiguration& ncfg, const uint32_t attemptUID, RawAudioMemory& audioMemory );


    size_t computeMemoryUsage() const;


    std::shared_future<void>        m_analysisFuture;
    std::atomic_bool                m_hasValidAnalysis; // set in async analysis if analysis data is to be trusted

public:
    const types::Stem               m_data;
    ImU32                           m_colourU32;        // converted from m_data and cached
    State                           m_state;

    uint32_t                        m_sampleRate;
    int32_t                         m_sampleCount;
    std::array<float*, 2>           m_channel;

    std::vector< double >           m_detectedBeatTimes;
    std::vector< float >            m_sampleEnergy;
    std::vector< uint64_t >         m_sampleBeat;
};

using StemPtr = std::shared_ptr<Stem>;

} // namespace live
} // namespace endlesss
