//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//

#include "base/utils.h"
#include "app/module.audio.h"
#include "endlesss/live.riff.h"

namespace mix {

using RiffQueue = mcc::ReaderWriterQueue<endlesss::live::RiffPtr>;

struct RiffMixerBase
{
    RiffMixerBase( const int32_t maxBufferSize, const int32_t sampleRate )
        : m_audioMaxBufferSize( maxBufferSize )
        , m_audioSampleRate( sampleRate )
    {
        memset( &m_timeInfo, 0, sizeof( m_timeInfo ) );

        for ( size_t mI = 0; mI < 8; mI++ )
        {
            m_mixChannelLeft[mI]  = mem::malloc16AsSet<float>( m_audioMaxBufferSize, 0.0f );
            m_mixChannelRight[mI] = mem::malloc16AsSet<float>( m_audioMaxBufferSize, 0.0f );
        }
    }

    ~RiffMixerBase()
    {
        for ( size_t mI = 0; mI < 8; mI++ )
        {
            mem::free16( m_mixChannelLeft[mI] );
            mem::free16( m_mixChannelRight[mI] );
        }
    }

    inline const app::AudioPlaybackTimeInfo* getTimeInfoPtr() const { return &m_timeInfo; }

protected: 

    // mash all 8+8 channels down into a stereo output buffer
    void mixChannelsToOutput(
        const app::module::Audio::OutputBuffer& outputBuffer,
        const float globalMultiplier,
        const uint32_t samplesToWrite );

    int32_t                         m_audioMaxBufferSize = 0;
    int32_t                         m_audioSampleRate = 0;

    std::array< float*, 8 >         m_mixChannelLeft;
    std::array< float*, 8 >         m_mixChannelRight;

    app::AudioPlaybackTimeInfo      m_timeInfo;
};

} // namespace mix