//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//

#include "base/utils.h"

#include "mix/common.h"
#include "app/module.audio.h"

namespace app { struct StoragePaths; }

namespace mix {

// ---------------------------------------------------------------------------------------------------------------------
// a very simple playback engine that accepts fully-loaded riffs and plays them immediately (within reason).
// for when you just want to hear a riff, like, right now.
//
struct Preview : public app::module::MixerInterface,
                 public RiffMixerBase
{
    using AudioBuffer        = app::module::Audio::OutputBuffer;
    using RiffChangeCallback = std::function<void( endlesss::live::RiffPtr& nowPlayingRiff ) >;

    Preview( const int32_t maxBufferSize, const int32_t sampleRate, const RiffChangeCallback& riffChangeCallback );
    ~Preview();


    // app::module::Audio::MixerInterface
    virtual void update(
        const AudioBuffer&  outputBuffer,
        const float         outputVolume,
        const uint32_t      samplesToWrite,
        const uint64_t      samplePosition ) override;
    virtual void imgui( const app::StoragePaths& storagePaths ) override;

    void exportRiff( const app::StoragePaths& storagePaths, const endlesss::live::RiffPtr& riffPtr );

    // main thread requests for a new riff; this queue is not deep - the preview mixer reacts immediately to jump
    // to new riffs as requested during the audio thread update(), the queue is just a lockfree buffer
    inline bool play( endlesss::live::RiffPtr& nextRiff )
    {
        // only accept fully synchronised and ready-to-play riffs; go do this on your own time please
        if ( nextRiff->getSyncState() != endlesss::live::Riff::SyncState::Success )
            return false;

        m_riffQueue.emplace( nextRiff );
        return true;
    }

    inline void stop()
    {
        m_drainQueueAndStop = true;
    }

    inline void setLockTransitionToNextBar( bool onOff ) { m_lockTransitionToNextBar = onOff; }
    inline bool getLockTransitionToNextBar() const       { return m_lockTransitionToNextBar; }

protected:

    void renderCurrentRiff(
        const uint32_t      outputOffset,
        const uint32_t      samplesToWrite,
        const uint64_t      samplePosition );


    RiffQueue                   m_riffQueue;                // lf interface between main and audio threads for new riff requests
    endlesss::live::RiffPtr     m_riffCurrent;              // what we're playing; this is managed by the audio thread
    int64_t                     m_riffPlaybackSample;       // 

    double                      m_riffPlaybackPercentage;
    int32_t                     m_riffPlaybackBar;
    int32_t                     m_riffPlaybackBarSegment;

    int32_t                     m_lockTransitionOnBeat      = 0;
    int32_t                     m_lockTransitionBarCount    = 1;
    bool                        m_lockTransitionToNextBar   = false;

    std::atomic_bool            m_drainQueueAndStop = false;

    RiffChangeCallback          m_riffChangeCallback;
};

} // namespace mix