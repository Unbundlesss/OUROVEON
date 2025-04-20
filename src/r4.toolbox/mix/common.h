//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//

#pragma once

#include "base/utils.h"
#include "base/operations.h"
#include "base/metaenum.h"

#include "app/module.audio.h"

#include "mix/stem.amalgam.h"

#include "endlesss/live.riff.h"


// ---------------------------------------------------------------------------------------------------------------------
CREATE_EVENT_BEGIN( MixerRiffChange )

MixerRiffChange( endlesss::live::RiffPtr& riff )
    : m_riff( riff )
{}

MixerRiffChange( endlesss::live::RiffPtr& riff, bool bWasCancelled )
    : m_riff( riff )
    , m_wasCancelled( bWasCancelled )
{}

endlesss::live::RiffPtr     m_riff;
bool                        m_wasCancelled = false;     // true if the mixer discarded this change (ie if panic-stopping)

CREATE_EVENT_END()


// ---------------------------------------------------------------------------------------------------------------------
namespace mix {

// spsc queue of riff instances
using RiffQueue          = mcc::ReaderWriterQueue< endlesss::live::RiffPtr >;

// spsc queue of riff instances with attached operation-ids
using RiffPtrOperation   = base::ValueWithOperation< endlesss::live::RiffPtr >;
using RiffOperationQueue = mcc::ReaderWriterQueue< RiffPtrOperation >;

#define _PCR(_action)       \
        _action(Instant)    \
        _action(Faster)     \
        _action(Fast)       \
        _action(Slow)       \
        _action(Glacial)
REFLECT_ENUM( PermutationChangeRate, uint32_t, _PCR );
#undef _PCR


// ---------------------------------------------------------------------------------------------------------------------
struct RiffMixerBase
{
    // support playback permutations with a queue + operation callback system
    using Permutation           = endlesss::types::RiffPlaybackPermutation;
    using PermutationOperation  = base::ValueWithOperation< Permutation >;
    using PermutationQueue      = mcc::ReaderWriterQueue< PermutationOperation >;

    // operation tag for permutation tasks
    static constexpr base::OperationVariant OV_Permutation{ 0xAB };



    RiffMixerBase( const int32_t maxBufferSize, const int32_t sampleRate, base::EventBusClient& eventBusClient );
    virtual ~RiffMixerBase();


    ouro_nodiscard constexpr const app::AudioPlaybackTimeInfo* getTimeInfoPtr() const { return &m_timeInfo; }


    base::OperationID enqueuePermutation( const Permutation& newPerm )
    {
        const auto operationID = base::Operations::newID( OV_Permutation );

        m_permutationQueue.emplace( operationID, newPerm );
        return operationID;
    }

    Permutation getCurrentPermutation() const
    {
        return m_permutationCurrent;
    }

protected:

    void mixChannelsWriteSilence(
        const uint32_t offset,
        const uint32_t samplesToWrite );

    // mash all 8 channels down into a stereo output buffer
    void mixChannelsToOutput(
        const app::module::Audio::OutputBuffer& outputBuffer,
        const app::module::Audio::OutputSignal& outputSignal,
        const uint32_t samplesToWrite );

    // permutation support
    void flushPendingPermutations();
    void updatePermutations( const uint32_t samplesToWrite, const double barLengthInSec );

    // amalgam support
    void stemAmalgamReset( const int32_t sampleRate );
    void stemAmalgamUpdate();


    int32_t                         m_audioMaxBufferSize    = 0;
    int32_t                         m_audioSampleRate       = 0;
    double                          m_audioSampleRateRecp   = 0;        // ( 1.0 / m_audioSampleRate )

    app::AudioPlaybackTimeInfo      m_timeInfo;

    // per-layer aligned memory buffers used for mixing
    std::array< float*, 8 >         m_mixChannelLeft;
    std::array< float*, 8 >         m_mixChannelRight;

    // per-layer gain permutation controls
    Permutation                     m_permutationCurrent;
    Permutation                     m_permutationTarget;
    PermutationQueue                m_permutationQueue;
    std::array< float, 8 >          m_permutationSampleGainDelta;
    PermutationChangeRate::Enum     m_permutationChangeRate = PermutationChangeRate::Instant;

    // amalgamated stem data
    StemDataAmalgam                 m_stemDataAmalgam;
    uint32_t                        m_stemDataAmalgamSamplesBeforeReset;
    uint32_t                        m_stemDataAmalgamSamplesUsed;

    base::EventBusClient            m_eventBusClient;
};

} // namespace mix
