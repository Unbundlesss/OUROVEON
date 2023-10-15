//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//

#pragma once

#include "base/utils.h"
#include "base/operations.h"

#include "app/module.audio.h"

#include "endlesss/live.riff.h"


// ---------------------------------------------------------------------------------------------------------------------
CREATE_EVENT_BEGIN( MixerRiffChange )

MixerRiffChange( endlesss::live::RiffPtr& riff )
    : m_riff( riff )
{}

endlesss::live::RiffPtr     m_riff;

CREATE_EVENT_END()


// ---------------------------------------------------------------------------------------------------------------------
namespace mix {

// spsc queue of riff instances
using RiffQueue          = mcc::ReaderWriterQueue< endlesss::live::RiffPtr >;

// spsc queue of riff instances with attached operation-ids
using RiffPtrOperation   = base::ValueWithOperation< endlesss::live::RiffPtr >;
using RiffOperationQueue = mcc::ReaderWriterQueue< RiffPtrOperation >;


// ---------------------------------------------------------------------------------------------------------------------
struct RiffMixerBase
{
    RiffMixerBase( const int32_t maxBufferSize, const int32_t sampleRate, base::EventBusClient& eventBusClient )
        : m_audioMaxBufferSize( maxBufferSize )
        , m_audioSampleRate( sampleRate )
        , m_eventBusClient( eventBusClient )
    {
        memset( &m_timeInfo, 0, sizeof( m_timeInfo ) );

        for ( size_t mI = 0; mI < 8; mI++ )
        {
            m_mixChannelLeft[mI]  = mem::alloc16To<float>( m_audioMaxBufferSize, 0.0f );
            m_mixChannelRight[mI] = mem::alloc16To<float>( m_audioMaxBufferSize, 0.0f );
        }

        m_audioSampleRateRecp = 1.0 / (double)m_audioSampleRate;
    }

    ~RiffMixerBase()
    {
        for ( size_t mI = 0; mI < 8; mI++ )
        {
            mem::free16( m_mixChannelLeft[mI] );
            mem::free16( m_mixChannelRight[mI] );
        }
    }

    ouro_nodiscard constexpr const app::AudioPlaybackTimeInfo* getTimeInfoPtr() const { return &m_timeInfo; }

protected:

    void mixChannelsWriteSilence(
        const uint32_t offset,
        const uint32_t samplesToWrite );

    // mash all 8 channels down into a stereo output buffer
    void mixChannelsToOutput(
        const app::module::Audio::OutputBuffer& outputBuffer,
        const app::module::Audio::OutputSignal& outputSignal,
        const uint32_t samplesToWrite );

    int32_t                         m_audioMaxBufferSize    = 0;
    int32_t                         m_audioSampleRate       = 0;
    double                          m_audioSampleRateRecp   = 0;        // ( 1.0 / m_audioSampleRate )

    std::array< float*, 8 >         m_mixChannelLeft;
    std::array< float*, 8 >         m_mixChannelRight;

    app::AudioPlaybackTimeInfo      m_timeInfo;

    base::EventBusClient            m_eventBusClient;
};

} // namespace mix
