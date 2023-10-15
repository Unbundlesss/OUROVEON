//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//

#include "pch.h"

#include "mix/common.h"
#include "buffer/mix.h"

namespace mix {

void RiffMixerBase::mixChannelsWriteSilence(
    const uint32_t offset, 
    const uint32_t samplesToWrite)
{
    for ( auto chIndex = 0U; chIndex < 8; chIndex++ )
    {
        for ( auto sI = offset; sI < offset + samplesToWrite; sI++ )
        {
            m_mixChannelLeft[chIndex][sI] = 0;
            m_mixChannelRight[chIndex][sI] = 0;
        }
    }
}

void RiffMixerBase::mixChannelsToOutput( 
    const app::module::Audio::OutputBuffer& outputBuffer,
    const app::module::Audio::OutputSignal& outputSignal,
    const uint32_t samplesToWrite )
{
    buffer::downmix_8channel_stereo(
        outputSignal.m_linearGain,
        samplesToWrite,
        m_mixChannelLeft[0],
        m_mixChannelLeft[1],
        m_mixChannelLeft[2],
        m_mixChannelLeft[3],
        m_mixChannelLeft[4],
        m_mixChannelLeft[5],
        m_mixChannelLeft[6],
        m_mixChannelLeft[7],
        m_mixChannelRight[0],
        m_mixChannelRight[1],
        m_mixChannelRight[2],
        m_mixChannelRight[3],
        m_mixChannelRight[4],
        m_mixChannelRight[5],
        m_mixChannelRight[6],
        m_mixChannelRight[7],
        outputBuffer.m_workingLR[0],
        outputBuffer.m_workingLR[1]
    );
}

}
