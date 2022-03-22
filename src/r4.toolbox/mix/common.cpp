//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//

#include "pch.h"

#include "mix/common.h"
#include "buffer/mix.h"

namespace mix {

void RiffMixerBase::mixChannelsToOutput( 
    const app::module::Audio::OutputBuffer& outputBuffer, 
    const float globalMultiplier,
    const uint32_t samplesToWrite )
{
    buffer::downmix_8channel_stereo(
        globalMultiplier,
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
        outputBuffer.m_workingLR[1] );
}

}