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

#include "mix/common.h"
#include "mix/stem.amalgam.h"

#include "app/module.audio.h"

namespace app { struct StoragePaths; }
namespace ableton { class Link; }

namespace mix {

// ---------------------------------------------------------------------------------------------------------------------
// a mostly simple mixer but with some tricks up its sleeve; can sequence riff changes on particular beats, does some
// sample averaging while blending to reduce clicks and pops on harsh riff state changes
//
struct Preview final : public app::module::MixerInterface,
                       public rec::IRecordable,
                       public RiffMixerBase
{
    static constexpr base::OperationVariant OV_EnqueueRiff { 0xAA };

    using AudioBuffer           = app::module::Audio::OutputBuffer;
    using AudioSignal           = app::module::Audio::OutputSignal;

    Preview( const int32_t maxBufferSize, const int32_t sampleRate, const std::chrono::microseconds outputLatency, base::EventBusClient& eventBusClient );
    ~Preview();

    // app::module::Audio::MixerInterface
    virtual void update(
        const AudioBuffer&  outputBuffer,
        const AudioSignal&  outputSignal,
        const uint32_t      samplesToWrite,
        const uint64_t      samplePosition ) override;

    virtual void imgui() override;

    const app::AudioPlaybackTimeInfo* getPlaybackTimeInfo() const override { return getTimeInfoPtr(); }


    // main thread requests for a new riff, added to queue for processing by mixer thread
    base::OperationID enqueueRiff( endlesss::live::RiffPtr& nextRiff )
    {
        // only accept fully synchronised and ready-to-play riffs; go do this on your own time please
        if ( nextRiff->getSyncState() != endlesss::live::Riff::SyncState::Success )
            return base::OperationID::invalid();

        const auto operationID = base::Operations::newID( OV_EnqueueRiff );

        m_riffQueue.emplace( operationID, nextRiff );
        return operationID;
    }

    // request to stop playing anything and remove everything from the request queue
    void stop()
    {
        m_drainQueueAndStop = true;
    }


    void enableAbletonLink( bool bEnabled );

    void setLockTransitionToNextBar( bool onOff ) { m_lockTransitionToNextBar = onOff; }
    bool getLockTransitionToNextBar() const       { return m_lockTransitionToNextBar; }

protected:

    struct AbletonLinkControl;

    static constexpr size_t     txBlendBufferSize = 128;
    using TxBlendInterpArray    = std::array< float, txBlendBufferSize >;

    void imguiDefault();
    void imguiTuning();


    void renderCurrentRiff(
        const uint32_t      outputOffset,
        const uint32_t      samplesToWrite );

    void applyBlendBuffer(
        const uint32_t      outputOffset,
        const uint32_t      samplesToWrite );


    void processCommandQueue();


#define _TX_BARS(_action)   \
        _action(Eighth)     \
        _action(Quarter)    \
        _action(Half)       \
        _action(Once)       \
        _action(Many)
    REFLECT_ENUM( TransitionBarCount, uint32_t, _TX_BARS );
#undef _TX_BARS

#define _MTO(_action)       \
        _action(WAV)        \
        _action(FLAC)
    REFLECT_ENUM( MultiTrackOutputFormat, uint32_t, _MTO );
#undef _MTO

    RiffOperationQueue              m_riffQueue;                        // lf interface between main and audio threads for new riff requests
    endlesss::live::RiffPtr         m_riffCurrent;                      // what we're playing; this is managed by the audio thread
    int64_t                         m_riffPlaybackSample        = 0;    //

    int32_t                         m_riffPlaybackNudge         = 0;

    AbletonLinkControl*             m_abletonLinkControl        = nullptr;

    std::array< float, 8 >          m_txBlendCacheLeft;
    std::array< float, 8 >          m_txBlendCacheRight;
    std::array< float, 8 >          m_txBlendActiveLeft;
    std::array< float, 8 >          m_txBlendActiveRight;
    TxBlendInterpArray              m_txBlendInterp;
    uint32_t                        m_txBlendSamplesRemaining   = 0;

    endlesss::live::RiffProgression m_playbackProgression;

    int32_t                         m_lockTransitionOnBeat      = 0;
    TransitionBarCount::Enum        m_lockTransitionBarCount    = TransitionBarCount::Once;
    std::atomic_int32_t             m_lockTransitionBarMultiple = 2;    // when ::Multiple in use, how many repeats
    std::atomic_int32_t             m_lockTransitionBarCounter  = 2;
    std::atomic_bool                m_lockTransitionToNextBar   = false;

    std::atomic_bool                m_riffTransitionedInMix     = false;
    float                           m_riffTransitionedUI        = 0.0f;

    std::atomic_bool                m_drainQueueAndStop         = false;


// ---------------------------------------------------------------------------------------------------------------------

    enum class EngineCommand
    {
        Invalid,
        BeginRecording,
        StopRecording
    };
    struct EngineCommandData final : public base::BasicCommandType<EngineCommand> { using BasicCommandType::BasicCommandType; };
    using CommandQueue = mcc::ReaderWriterQueue<EngineCommandData>;

    CommandQueue                    m_commandQueue;

// ---------------------------------------------------------------------------------------------------------------------
// rec::IRecordable

    using MultiTrackStreams = std::array < std::shared_ptr<ssp::ISampleStreamProcessor>, 8 >;

private:

    std::atomic_bool                m_multiTrackInFlux          = false;
    bool                            m_multiTrackRecording       = false;
    MultiTrackOutputFormat::Enum    m_multiTrackOutputFormat    = MultiTrackOutputFormat::FLAC;
    MultiTrackStreams               m_multiTrackOutputs;                        // currently live recorders
    MultiTrackStreams               m_multiTrackOutputsToDestroyOnMainThread;   // recorders ready to decommission on main thread

public:

    rec::IRecordable* getRecordable() override { return this; }

    bool beginRecording( const fs::path& outputPath, const std::string& filePrefix ) override;
    void stopRecording() override;
    bool isRecording() const override;
    uint64_t getRecordingDataUsage() const override;

    inline std::string_view getRecorderName() const override { return " 8-Track (Pre FX) "; }
    inline const char* getFluxState() const override
    {
        if ( m_multiTrackInFlux )
            return " Working ...";

        return nullptr;
    }
};

} // namespace mix
