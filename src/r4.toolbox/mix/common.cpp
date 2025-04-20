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

// ---------------------------------------------------------------------------------------------------------------------
RiffMixerBase::RiffMixerBase( const int32_t maxBufferSize, const int32_t sampleRate, base::EventBusClient& eventBusClient )
    : m_audioMaxBufferSize( maxBufferSize )
    , m_audioSampleRate( sampleRate )
    , m_eventBusClient( eventBusClient )
{
    memset( &m_timeInfo, 0, sizeof( m_timeInfo ) );

    for ( size_t mI = 0; mI < 8; mI++ )
    {
        m_mixChannelLeft[mI] = mem::alloc16To<float>( m_audioMaxBufferSize, 0.0f );
        m_mixChannelRight[mI] = mem::alloc16To<float>( m_audioMaxBufferSize, 0.0f );
    }

    m_audioSampleRateRecp = 1.0 / (double)m_audioSampleRate;

    m_permutationSampleGainDelta.fill( 0 );

    stemAmalgamReset( sampleRate );
}

// ---------------------------------------------------------------------------------------------------------------------
RiffMixerBase::~RiffMixerBase()
{
    for ( size_t mI = 0; mI < 8; mI++ )
    {
        mem::free16( m_mixChannelLeft[mI] );
        mem::free16( m_mixChannelRight[mI] );
    }
    m_mixChannelLeft.fill( nullptr );
    m_mixChannelRight.fill( nullptr );
}

// ---------------------------------------------------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------------------------------------------------
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


// ---------------------------------------------------------------------------------------------------------------------
void RiffMixerBase::flushPendingPermutations()
{
    // see if we have permutations incoming
    PermutationOperation permutationOp;
    while ( m_permutationQueue.try_dequeue( permutationOp ) )
    {
        m_permutationTarget = permutationOp.m_value;

        // .. signal that we consumed it, if so
        m_eventBusClient.Send< ::events::OperationComplete >( permutationOp.m_operation );
    }
}

// ---------------------------------------------------------------------------------------------------------------------
void RiffMixerBase::updatePermutations( const uint32_t samplesToWrite, const double barLengthInSec )
{
    if ( m_permutationChangeRate == PermutationChangeRate::Instant )
    {
        for ( auto layer = 0U; layer < 8; layer++ )
        {
            m_permutationSampleGainDelta[layer] = 0;
            m_permutationCurrent.m_layerGainMultiplier[layer] = m_permutationTarget.m_layerGainMultiplier[layer];
        }
    }
    else
    {
        // get the time slice for this update() call in seconds
        const double elapsedRealTime = (double)samplesToWrite * m_audioSampleRateRecp;


        double changeRate = 60.0f;
        switch ( m_permutationChangeRate )
        {
            default: break;
            case PermutationChangeRate::Faster:  changeRate = 90.0; break;
            case PermutationChangeRate::Fast:    changeRate = 20.0; break;
            case PermutationChangeRate::Slow:    changeRate = 1.0;  break;
            case PermutationChangeRate::Glacial: changeRate = 0.5;  break;
        }
        changeRate /= barLengthInSec;   // modify to fit the current riff bar length

        const double changeLimitPerSecond = ( changeRate * elapsedRealTime );

        const double sampleCountRecp = 1.0 / (double)samplesToWrite;

        // update current/target chasing gain values
        for ( auto layer = 0U; layer < 8; layer++ )
        {
            double delta = ( m_permutationTarget.m_layerGainMultiplier[layer] - 
                             m_permutationCurrent.m_layerGainMultiplier[layer] );

            delta = std::clamp( delta, -changeLimitPerSecond, changeLimitPerSecond );

            // delta is small enough that we just snap to the target value instantly
            if ( delta <=  FLT_EPSILON && 
                 delta >= -FLT_EPSILON )
            {
                m_permutationSampleGainDelta[layer] = 0;
                m_permutationCurrent.m_layerGainMultiplier[layer] = m_permutationTarget.m_layerGainMultiplier[layer];
            }
            // otherwise set the per-sample change to be applied during render() calls
            else
            {
                m_permutationSampleGainDelta[layer] = (float)(delta * sampleCountRecp);
            }
        }
    }
}


// ---------------------------------------------------------------------------------------------------------------------
void RiffMixerBase::stemAmalgamReset( const int32_t sampleRate )
{
    // work out the timing for resetting amalgams; roughly broadcast every 1/N seconds
    m_stemDataAmalgam.reset();
    m_stemDataAmalgamSamplesBeforeReset = (uint32_t)std::round( (double)sampleRate * (1.0 / 60.0) );
    m_stemDataAmalgamSamplesUsed = 0;
}

// ---------------------------------------------------------------------------------------------------------------------
void RiffMixerBase::stemAmalgamUpdate()
{
    // burned through enough samples to send out latest stem data block. chuck the current data on the bus
    // NB/TODO doesn't deal with crossing this boundary inside the update (eg. if samplesToWrite is big, bigger than SamplesBeforeReset..)
    // could do this at a higher level, break the render() into smaller samplesToWrite blocks to fit, probably overkill
    if ( m_stemDataAmalgamSamplesUsed >= m_stemDataAmalgamSamplesBeforeReset )
    {
        m_eventBusClient.Send< ::events::StemDataAmalgamGenerated >( m_stemDataAmalgam );

        m_stemDataAmalgam.reset();
        m_stemDataAmalgamSamplesUsed = 0;
    }
}

}
