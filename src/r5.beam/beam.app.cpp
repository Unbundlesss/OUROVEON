#include "pch.h"

#include "base/utils.h"
#include "math/rng.h"
#include "buffer/mix.h"
#include "mix/common.h"

#include "spacetime/moment.h"

#include "app/module.audio.h"
#include "app/module.frontend.h"
#include "app/module.frontend.fonts.h"

#include "app/ouro.h"

#include "ux/diskrecorder.h"
#include "ux/jams.browser.h"
#include "ux/stem.beats.h"
#include "ux/riff.tagline.h"

#include "ssp/ssp.file.flac.h"

#include "discord/discord.bot.ui.h"

#include "endlesss/all.h"

#include "data/databus.h"
#include "effect/effect.stack.h"
#include "net/bond.riffpush.h"

#include <ableton/Link.hpp>
#include <ableton/link/HostTimeFilter.hpp>


#define OUROVEON_BEAM           "BEAM"
#define OUROVEON_BEAM_VERSION   OURO_FRAMEWORK_VERSION "-beta"

using namespace std::chrono_literals;

// ---------------------------------------------------------------------------------------------------------------------
struct BeamAbletonLinkControl
{
    BeamAbletonLinkControl()
        : m_link( 120.0 )
    {
    }
    ~BeamAbletonLinkControl()
    {
        m_link.enable( false );
    }

    using LinkHostTime = ableton::link::HostTimeFilter<ableton::link::platform::Clock>;

    void Transaction_StopPlaying()
    {
        // if the current link state is "playing", snag and change the session to stop it
        if ( m_linkIsPlaying )
        {
            auto linkSessionState = m_link.captureAudioSessionState();
            {
                linkSessionState.setIsPlaying(
                    false,
                    m_hostTimeFilter.sampleTimeToHostTime( m_sampleTime ) );

                m_linkIsPlaying = false;
            }
            m_link.commitAudioSessionState( linkSessionState );
        }
    }


    ableton::Link               m_link;
    LinkHostTime                m_hostTimeFilter;
    std::chrono::microseconds   m_outputLatency;
    double                      m_sampleTime = 0;
    int32_t                     m_authorativeInterval = -1;
    bool                        m_linkIsPlaying = false;
};

// ---------------------------------------------------------------------------------------------------------------------
struct MixEngine final : public app::module::MixerInterface,
                         public rec::IRecordable,
                         public mix::RiffMixerBase
{
    using AudioBuffer = app::module::Audio::OutputBuffer;
    using AudioSignal = app::module::Audio::OutputSignal;

    static constexpr uint32_t cSampleCountMax = std::numeric_limits<uint32_t>::max();

    // 
    struct ProgressionConfiguration
    {
        ProgressionConfiguration()
            : m_triggerPoint( TriggerPoint::AnyBarStart )
            , m_blendTime( BlendTime::TwoBars )
            , m_greedyMode( false )
        {}

        bool operator==( ProgressionConfiguration const& ) const = default;

        // choose when to begin blending to the next riff
        enum class TriggerPoint
        {
            Arbitrary,                      // start blending whenever a new riff arrives, yolo
            NextRiffStart,                  //             .. when a riff loops around to the beginning (ie. once per riff loop)
            AnyBarStart,                    //             .. when a bar segment is crossed (or near enough) (ie. usually 8 opportunities per riff loop)
            AnyEvenBarStart,                //             .. when an even-numbered bar is crossed
        }               m_triggerPoint;

        static constexpr size_t cTriggerPointCount = 4;
        static constexpr std::array< const char*, cTriggerPointCount > cTriggerPointNames {{
            "At Any Point",
            "At Riff Start",
            "At Any Bar Start",
            "At Even Bar Start"
        }};
        inline static const char* getTriggerPointName( const TriggerPoint tp )
        {
            return cTriggerPointNames[(size_t)tp];
        }
        inline static bool triggerPointGetter( void* data, int idx, const char** out_text )
        {
            *out_text = getTriggerPointName( (TriggerPoint)idx );
            return true;
        }

        // how long the blend should take
        enum class BlendTime
        {
            Zero,
            OneBar,
            TwoBars,
            FourBars,
            EightBars
        }               m_blendTime;

        static constexpr size_t cBlendTimeCount = 5;
        static constexpr std::array< const char*, cBlendTimeCount > cBlendTimeNames {{
            "Instant",
            "One Bar",
            "Two Bars",
            "Four Bars",
            "Eight Bars"
        }};
        inline static const char* getBlendTimeName( const BlendTime bt )
        {
            return cBlendTimeNames[(size_t)bt];
        }
        inline static bool blendTimeGetter( void* data, int idx, const char** out_text )
        {
            *out_text = getBlendTimeName( (BlendTime)idx );
            return true;
        }
        inline float getBlendTimeMultiplier() const
        {
            switch ( m_blendTime )
            {
            default:
            case ProgressionConfiguration::BlendTime::OneBar:    return 1.0f;
            case ProgressionConfiguration::BlendTime::TwoBars:   return 2.0f;
            case ProgressionConfiguration::BlendTime::FourBars:  return 4.0f;
            case ProgressionConfiguration::BlendTime::EightBars: return 8.0f;
            }
        }

        bool            m_greedyMode;       // if on and there are multiple 'next riffs' in the queue when it comes time to begin
                                            // blending, empty the list and only blend to the most recent one. if off, we will
                                            // work our way through each enqueued change in turn
    };

    struct RepComConfiguration
    {
        bool            m_enable = false;   // if multi-track is started, use repcom logic
    };

    enum class EngineCommand
    {
        Invalid,
        BeginRecording,
        StopRecording,
        UpdateProgressionConfiguration,     // pass ProgressionConfiguration*
        UpdateRepComConfiguration,          // pass RepComConfiguration*
        ClearCurrentlyPlaying,
        ClearAllScheduledTransitions
    };
    struct EngineCommandData : public base::BasicCommandType<EngineCommand> { using BasicCommandType::BasicCommandType; };



    using CommandQueue  = mcc::ReaderWriterQueue<EngineCommandData>;
    using RiffQueue     = mcc::ReaderWriterQueue<endlesss::live::RiffAndPermutation>;


    uint64_t                    m_samplePosition;


    endlesss::live::RiffAndPermutation
                                m_riffCurrent;
    uint32_t                    m_riffLengthInSamples;

    endlesss::live::RiffProgression
                                m_playbackProgression;

    RiffQueue                   m_riffQueue;
    CommandQueue                m_commandQueue;

    endlesss::live::RiffAndPermutation
                                m_riffNext;
    float                       m_transitionValue;

    float                       m_stemBeatRate;
    double                      m_transitionRate;

    ProgressionConfiguration    m_progression;

    BeamAbletonLinkControl      m_abletonLinkControl;


    MixEngine( const int32_t maxBufferSize, const int32_t sampleRate, const std::chrono::microseconds outputLatency, base::EventBusClient& eventBusClient )
        : RiffMixerBase( maxBufferSize, sampleRate, eventBusClient )
        , m_samplePosition( 0 )
        , m_transitionValue( 0 )
        , m_stemBeatRate( 4.0f )
        , m_transitionRate( 0.25 )
        , m_multiTrackInFlux( false )
        , m_multiTrackWaitingToRecordOnRiffEdge( false )
        , m_multiTrackRecording( false )
        , m_repcomRepeatBar( 0 )
        , m_repcomPausedOnBar( -1 )
        , m_repcomRepeatLimit( 0 )
        , m_repcomSampleStart( 0 )
        , m_repcomSampleEnd( cSampleCountMax )
        , m_repcomState( RepComState::Unpaused )
    {
        m_abletonLinkControl.m_outputLatency = outputLatency;
    }

    virtual ~MixEngine()
    {
    }

    const app::AudioPlaybackTimeInfo* getPlaybackTimeInfo() const override { return getTimeInfoPtr(); }


    inline void addNextRiff( const endlesss::live::RiffPtr& nextRiff )
    {
        m_riffQueue.emplace( nextRiff );
    }

    // add new riff with an optional permutation packet
    inline void addNextRiff( const endlesss::live::RiffPtr& nextRiff, const endlesss::types::RiffPlaybackPermutationOpt& permOpt )
    {
        m_riffQueue.emplace( nextRiff, permOpt );
    }


    inline void updateProgressionConfiguration( const ProgressionConfiguration* pConfig )
    {
        m_commandQueue.emplace( EngineCommand::UpdateProgressionConfiguration, (void*) pConfig );
    }

    void clearAllScheduledTransitions()
    {
        m_commandQueue.emplace( EngineCommand::ClearAllScheduledTransitions );
    }

    inline void updateRepComConfiguration( const RepComConfiguration* pConfig )
    {
        m_commandQueue.emplace( EngineCommand::UpdateRepComConfiguration, (void*)pConfig );
    }

    inline void clearCurrentPlayback()
    {
        m_commandQueue.emplace( EngineCommand::ClearCurrentlyPlaying );
    }

    void enableAbletonLink( bool bEnabled )
    {
        m_abletonLinkControl.m_link.enable( bEnabled );
    }


    inline uint32_t getBarRepetitions() const { return m_repcomRepeatBar; }

    inline bool isRepComEnabled() const { return m_repcom.m_enable; }
    inline bool isRepComPaused() const { return isRepComEnabled() && ( m_repcomState != RepComState::Unpaused ); }
    inline int32_t getBarPausedOn() const { return m_repcomPausedOnBar; }


    void update(
        const AudioBuffer&  outputBuffer,
        const AudioSignal&  outputSignal,
        const uint32_t      samplesToWrite,
        const uint64_t      samplePosition ) override;

    void commit(
        const AudioBuffer&  outputBuffer,
        const AudioSignal&  outputSignal,
        const uint32_t      samplesToWrite )
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
            outputBuffer.m_workingLR[1]);

        if ( m_multiTrackRecording )
        {
            const bool repComEnabled = isRepComEnabled();

            // repetition compression is being activated or suspended, meaning we need to take just a chunk of the 
            // presented samples rather than all of it
            if ( repComEnabled && ( m_repcomState == RepComState::SampleFragmentAndPause ||
                                    m_repcomState == RepComState::SampleFragmentAndResume ) )
            {
                const auto fragmentSampleCount = std::min( (uint32_t)m_repcomSampleEnd, samplesToWrite ) - m_repcomSampleStart;

                blog::mix( "[ REPCOM ] Fragmenting ({})  [ {} ] -> [ {} ]  ({} samples)",
                    ( m_repcomState == RepComState::SampleFragmentAndPause ) ? "Pausing" : "Resuming",
                    m_repcomSampleStart,
                    m_repcomSampleStart + fragmentSampleCount,
                    fragmentSampleCount );

                for ( auto i = 0; i < 8; i++ )
                {
                    m_multiTrackOutputs[i]->appendSamples(
                        m_mixChannelLeft[i] + m_repcomSampleStart,
                        m_mixChannelRight[i] + m_repcomSampleStart,
                        fragmentSampleCount );
                }
            }
            else if ( !repComEnabled || m_repcomState == RepComState::Unpaused )
            {
                for ( auto i = 0; i < 8; i++ )
                {
                    m_multiTrackOutputs[i]->appendSamples( m_mixChannelLeft[i], m_mixChannelRight[i], samplesToWrite );
                }
            }
        }
        {
            if ( m_repcomState == RepComState::SampleFragmentAndPause )
                 m_repcomState  = RepComState::Paused;
            if ( m_repcomState == RepComState::SampleFragmentAndResume )
            {
                m_repcomState       = RepComState::Unpaused;
                m_repcomPausedOnBar = -1;
            }

            m_repcomSampleStart = 0;
            m_repcomSampleEnd   = cSampleCountMax;
        }
    }

    void mainThreadUpdate( const float dT, endlesss::toolkit::Exchange& beatEx )
    {
        for ( auto layer = 0U; layer < 8; layer++ )
            m_multiTrackOutputsToDestroyOnMainThread[layer].reset();
    }


// ---------------------------------------------------------------------------------------------------------------------
// rec::IRecordable

public:

    inline bool beginRecording( const fs::path& outputPath, const std::string& filePrefix ) override
    {
        // should not be calling this if we're already in the process of streaming out
        assert( !isRecording() );
        if ( isRecording() )
            return false;

        math::RNG32 writeBufferShuffleRNG;

        // set up 8 WAV output streams, one for each Endlesss layer
        for ( auto i = 0; i < 8; i++ )
        {
            auto recordFile = outputPath / fmt::format( "{}beam_channel{}.flac", filePrefix, i );
            m_multiTrackOutputs[i] = ssp::FLACWriter::Create(
                recordFile.string(),
                m_audioSampleRate,
                writeBufferShuffleRNG.genFloat( 0.75f, 1.75f ) );   // randomise the write buffer sizes to avoid all
                                                                    // outputs flushing outputs simultaneously
        }

        // tell the worker thread to begin writing to our streams
        m_commandQueue.enqueue( EngineCommand::BeginRecording );
        m_multiTrackInFlux = true;

        return true;
    }

    inline void stopRecording() override
    {
        // can't stop what hasn't started
        assert( isRecording() );
        if ( !isRecording() )
            return;

        m_commandQueue.enqueue( EngineCommand::StopRecording );
        m_multiTrackInFlux = true;
    }

    // either we're fully engaged with writing out the stream or the request to do (or to stop) is still in-flight
    inline bool isRecording() const override
    {
        return m_multiTrackRecording || 
               m_multiTrackInFlux;
    }

    inline uint64_t getRecordingDataUsage() const override
    {
        if ( !isRecording() )
            return 0;

        uint64_t usage = 0;
        for ( auto i = 0; i < 8; i++ )
            usage += m_multiTrackOutputs[i]->getStorageUsageInBytes();

        return usage;
    }

    inline std::string_view getRecorderName() const override { return " Multitrack "; }
    inline const char* getFluxState() const override
    {
        if ( m_repcomState != RepComState::Unpaused )
            return "[PAUSED] ";
        if ( m_multiTrackInFlux )
            return " Awaiting Loop Start";

        return nullptr;
    }


private:

    using MultiTrackStreams = std::array < std::shared_ptr<ssp::FLACWriter>, 8 >;

    bool                m_multiTrackInFlux;
    bool                m_multiTrackWaitingToRecordOnRiffEdge;
    bool                m_multiTrackRecording;
    MultiTrackStreams   m_multiTrackOutputs;                        // currently live recorders
    MultiTrackStreams   m_multiTrackOutputsToDestroyOnMainThread;   // recorders ready to decommission on main thread


    // multitrack "repetition compression" (RepCom) 

    // repcom config written to via engine command
    RepComConfiguration m_repcom;

    // repcom internal state
    int32_t             m_repcomRepeatBar;
    int32_t             m_repcomPausedOnBar;
    int32_t             m_repcomRepeatLimit;
    uint32_t            m_repcomSampleStart;
    uint32_t            m_repcomSampleEnd;
    enum class RepComState
    {
        Unpaused,
        SampleFragmentAndPause,
        Paused,
        SampleFragmentAndResume
    }                   m_repcomState;
};

// ---------------------------------------------------------------------------------------------------------------------
void MixEngine::update(
    const AudioBuffer&  outputBuffer,
    const AudioSignal&  outputSignal,
    const uint32_t      samplesToWrite,
    const uint64_t      samplePosition )
{
    m_samplePosition = samplePosition;
    m_timeInfo.samplePos = (double)samplePosition;

    stemAmalgamUpdate();

    const double linearTimeStep = (double)samplesToWrite / (double)m_audioSampleRate;

    const auto notifyRepComOfActivity = [&]( uint32_t newStartSample )
    {
        assert( m_repcomState != RepComState::SampleFragmentAndResume );
        if ( m_repcomState == RepComState::Paused )
        {
            blog::mix( "[ REPCOM ] UNPAUSE on new activity @ {} -> {}", newStartSample, samplesToWrite );

            m_repcomSampleStart = newStartSample;
            m_repcomSampleEnd   = cSampleCountMax;
            m_repcomState       = RepComState::SampleFragmentAndResume;
        }
    };

    // flag to indicate that the local data buffers need updating with m_riffCurrent data
    // .. this is reset mid-cycle in the case we have to handle a transition event where the
    // riff might change underneith us (via exchangeLiveRiff)
    bool riffUnpackRequired = true;

    // swap in the Next riff, mark transition/unpack flags as appropriate
    const auto exchangeLiveRiff = [&]
    {
        m_riffCurrent           = m_riffNext;
        m_riffNext              = {};
        m_transitionValue       = 0.0f;
        m_repcomRepeatBar       = 0;

        m_abletonLinkControl.m_authorativeInterval = 1;

        riffUnpackRequired      = true;
    };

    // process commands from the main thread
    {
        EngineCommandData engineCmd;
        if ( m_commandQueue.try_dequeue( engineCmd ) )
        {
            switch ( engineCmd.getCommand() )
            {
                // start multitrack on the edge of a riff; the actual recording begins 
                // when that is detected during the sample loop below
                case EngineCommand::BeginRecording:
                {
                    m_multiTrackWaitingToRecordOnRiffEdge = true;
                }
                break;

                // cleanly disengage recording from the mix thread
                case EngineCommand::StopRecording:
                {
                    m_multiTrackWaitingToRecordOnRiffEdge = false;
                    m_multiTrackRecording                 = false;
                    m_multiTrackInFlux                    = false;

                    // move recorders over to destroy on the main thread, avoid any stalls
                    // from whatever may be required to tie off recording
                    for ( auto i = 0; i < 8; i++ )
                    {
                        assert( m_multiTrackOutputsToDestroyOnMainThread[i] == nullptr );

                        m_multiTrackOutputsToDestroyOnMainThread[i] = m_multiTrackOutputs[i];
                        m_multiTrackOutputs[i].reset();
                    }

                    blog::mix( "[ Multitrack ] ... Stopped" );
                }
                break;

                case EngineCommand::UpdateProgressionConfiguration:
                {
                    assert( engineCmd.getPtr() != nullptr );
                    m_progression = *engineCmd.getPtrAs< ProgressionConfiguration >();
                }
                break;

                case EngineCommand::UpdateRepComConfiguration:
                {
                    assert( !isRecording() );   // should not be allowed to update if recording is already underway
                    assert( engineCmd.getPtr() != nullptr );
                    m_repcom = *engineCmd.getPtrAs< RepComConfiguration >();
                }
                break;

                case EngineCommand::ClearCurrentlyPlaying:
                {
                    m_riffNext = {};
                    exchangeLiveRiff();
                }
                break;

                case EngineCommand::ClearAllScheduledTransitions:
                {
                    // purge the queue
                    endlesss::live::RiffAndPermutation dumpRiff;
                    while ( m_riffQueue.try_dequeue( dumpRiff ) )
                    { }
                }
                break;

                default:
                case EngineCommand::Invalid:
                    blog::error::mix( "Unknown or invalid command received" );
                    break;
            }
        }
    }

    const auto checkForAndDequeueNextRiff = [&]
    {
        if ( m_riffNext.isEmpty() )
        {
            if ( m_riffQueue.peek() != nullptr &&
                 m_transitionValue == 0 )
            {
                if ( !m_riffQueue.try_dequeue( m_riffNext ) )
                {
                    assert( false );
                    return false;
                }

                blog::mix( "[ BLEND ] DEQUEUED new riff" );

                if ( m_riffNext.m_riffPtr->getSyncState() != endlesss::live::Riff::SyncState::Success )
                {
                    blog::error::mix( "[ BLEND ] .. new riff invalid, ignoring it" );
                    m_riffNext = {};
                }

                // hard cut
                if ( m_progression.m_blendTime == ProgressionConfiguration::BlendTime::Zero )
                {
                    blog::mix( "[ BLEND ] hard cut" );
                    exchangeLiveRiff();
                }
                // pick blend rate based on how many bars to take
                else
                {
                    ABSL_ASSERT( m_riffNext.isNotEmpty() );
                    m_transitionRate = 1.0 / ( m_riffNext.m_riffPtr->m_timingDetails.m_lengthInSecPerBar * m_progression.getBlendTimeMultiplier() );
                }
            }
        }

        return ( m_riffNext.isNotEmpty() );
    };

    // in Arbitrary mode, check all the time to see if we could be switching
    if ( m_progression.m_triggerPoint == ProgressionConfiguration::TriggerPoint::Arbitrary )
    {
        checkForAndDequeueNextRiff();
    }

    // update the transition, if one is active; and swap in the next riff if one has completed
    if ( m_riffNext.isNotEmpty() )
    {
        m_transitionValue += (float)(linearTimeStep * m_transitionRate);

        if ( m_transitionValue >= 1.0f )
        {
            blog::mix( "[ BLEND ] completed" );
            exchangeLiveRiff();
        }
    }

    // early out if we have nothing to play or the active riff is 0-length
    if ( m_riffCurrent.isEmpty() ||
         m_riffCurrent.m_riffPtr->m_timingDetails.m_lengthInSamples == 0 )
    {
        outputBuffer.applySilence();

        // reset computed riff playback variables
        m_playbackProgression.reset();

        // check if there's something on the horizon
        if ( m_riffQueue.peek() != nullptr )
        {
            if ( checkForAndDequeueNextRiff() )
            {
                blog::mix( "[ BLEND ] hard cut to first riff" );
                exchangeLiveRiff();
                notifyRepComOfActivity( 0 );
            }
        }

        // deal with LINK session update with nothing playing
        {
            m_abletonLinkControl.Transaction_StopPlaying();
            m_abletonLinkControl.m_sampleTime += static_cast<double>(samplesToWrite);
        }

        return;
    }



    const endlesss::live::Riff* currentRiff = m_riffCurrent.m_riffPtr.get();
    const endlesss::types::RiffPlaybackPermutation& currentPermutation = m_riffCurrent.m_permutation;

    // compute where we are (roughly) for the UI
    currentRiff->getTimingDetails().ComputeProgressionAtSample( m_samplePosition, m_playbackProgression );



    // update vst time structure with latest state
    m_timeInfo.tempo              = currentRiff->m_timingDetails.m_bpm;
    m_timeInfo.timeSigNumerator   = currentRiff->m_timingDetails.m_quarterBeats;
    m_timeInfo.timeSigDenominator = 4;


    std::array< bool,  16 >         stemHasBeat;
    std::array< float, 16 >         stemEnergy;
    std::array< float, 16 >         stemTimeStretch;
    std::array< float, 16 >         stemGains;
    std::array< endlesss::live::Stem*, 16 >   stemPtr;

    // keep note of where we are mixing in terms of the 0..N sample count of the current riff
    std::array< uint32_t, 2 >       riffLengthInSamples;
    std::array< uint32_t, 2 >       riffWrappedSampleStart;
    
    stemHasBeat.fill( false );
    stemEnergy.fill( 0.0f );
    stemTimeStretch.fill( 0.0f );
    stemGains.fill( 0.0f );
    stemPtr.fill( nullptr );

    riffLengthInSamples.fill( 0 );
    riffWrappedSampleStart.fill( 0 );

    // unpack our primary foreground riff into the local data buffers used during sample filling
    const auto decodeForegroundRiffData = [&]
    {
        riffLengthInSamples[0]      = currentRiff->m_timingDetails.m_lengthInSamples;
        riffWrappedSampleStart[0]   = samplePosition % riffLengthInSamples[0];

        for (auto stemI = 0U; stemI < 8; stemI++)
        {
            stemTimeStretch[stemI]  = currentRiff->m_stemTimeScales[stemI];
            stemGains[stemI]        = currentRiff->m_stemGains[stemI] * currentPermutation.m_layerGainMultiplier[stemI];
            stemPtr[stemI]          = currentRiff->m_stemPtrs[stemI];
        }
    };
    decodeForegroundRiffData();

    // unpack transitional riff? can be called mid-loop if we pull a new riff off the pile
    const auto decodeTransitionalRiffData = [&]
    {
        if ( m_transitionValue > 0 )
        {
            const auto* nextRiff        = m_riffNext.m_riffPtr.get();
            const auto& nextPermutation = m_riffNext.m_permutation;

            riffLengthInSamples[1]      = nextRiff->m_timingDetails.m_lengthInSamples;
            riffWrappedSampleStart[1]   = samplePosition % riffLengthInSamples[1];

            for ( auto stemI = 0U; stemI < 8; stemI++ )
            {
                stemTimeStretch[ 8 + stemI ]  = nextRiff->m_stemTimeScales[stemI];
                stemGains[ 8 + stemI ]        = nextRiff->m_stemGains[stemI] * nextPermutation.m_layerGainMultiplier[stemI];
                stemPtr[ 8 + stemI ]          = nextRiff->m_stemPtrs[stemI];
            }
        }
    };
    decodeTransitionalRiffData();


    const auto segmentLengthInSamples   = currentRiff->m_timingDetails.m_lengthInSamplesPerBar;
          auto segmentSampleStart       = samplePosition % segmentLengthInSamples;

    for ( auto sI = 0U; sI < samplesToWrite; sI++ )
    {
        const uint64_t sample = samplePosition + sI;

        // get sample position in context of the riff
        uint64_t riffSample   = riffWrappedSampleStart[0] + sI;
        if ( riffSample >= riffLengthInSamples[0] )
            riffSample -= riffLengthInSamples[0];

        while ( segmentSampleStart >= segmentLengthInSamples )
        {
            segmentSampleStart -= segmentLengthInSamples;

            m_playbackProgression.m_playbackBar++;
            if ( m_playbackProgression.m_playbackBar >= currentRiff->m_timingDetails.m_barCount )
                m_playbackProgression.m_playbackBar = 0;
        }

        const uint64_t segmentSample = segmentSampleStart++;


        if ( segmentSample == 0 )
        {
//            blog::mix( "[ Edge ] Bar {}", m_riffPlaybackBar + 1 );

            const bool isEvenBarNumber = (m_playbackProgression.m_playbackBar & 1 ) == 0;
            const bool shouldTriggerTransition =
                // any bar
                ( m_progression.m_triggerPoint == ProgressionConfiguration::TriggerPoint::AnyBarStart ) ||
                // 0, 2, 4, .. 
                ( m_progression.m_triggerPoint == ProgressionConfiguration::TriggerPoint::AnyEvenBarStart && isEvenBarNumber ) ||
                // next riff is bar 0
                ( m_progression.m_triggerPoint == ProgressionConfiguration::TriggerPoint::NextRiffStart && m_playbackProgression.m_playbackBar == 0 );

            if ( shouldTriggerTransition )
            {
                bool allowedToTrigger = true;

                if ( isRepComPaused() && m_repcomPausedOnBar != m_playbackProgression.m_playbackBar )
                    allowedToTrigger = false;

                if ( allowedToTrigger && checkForAndDequeueNextRiff() )
                {
                    if ( riffUnpackRequired )
                        decodeForegroundRiffData();
                    decodeTransitionalRiffData();
                    notifyRepComOfActivity( sI );
                }
            }

            if ( m_multiTrackRecording )
            {
                m_repcomRepeatBar++;

                const bool repComTriggerPause = isRepComEnabled() &&
                                                m_repcomRepeatBar >= currentRiff->m_timingDetails.m_longestStemInBars;

                if (    repComTriggerPause
                     && m_repcomState           == RepComState::Unpaused
                     && m_riffNext.m_riffPtr    == nullptr
                     && m_transitionValue       == 0 )
                {
                    blog::mix( "[ REPCOM ] Pausing @ bar {}, sample offset {}", m_playbackProgression.m_playbackBar, sI );

                    m_repcomPausedOnBar = m_playbackProgression.m_playbackBar;

                    m_repcomSampleStart = 0;
                    m_repcomSampleEnd   = sI;
                    m_repcomState       = RepComState::SampleFragmentAndPause;
                }
            }
        }
        if ( riffSample == 0 )
        {
            blog::mix( "[ Edge ] Riff" );

            // multitrack recording waits till the start of a riff to begin
            if ( m_multiTrackWaitingToRecordOnRiffEdge )
            {
                // begin writing out data
                m_multiTrackRecording                 = true;
                m_multiTrackWaitingToRecordOnRiffEdge = false;
                m_multiTrackInFlux                    = false;

                // reset repcom stats
                m_repcomRepeatBar                     = 0;

                blog::mix( "[ Multitrack ] Recording ..." );
            }
        }


        for ( auto stemI = 0U; stemI < 8; stemI++ )
        {
            const endlesss::live::Stem* stemInst = stemPtr[stemI];
            const float       stemGain = stemGains[stemI];

            if ( stemInst == nullptr || stemInst->hasFailed() )
            {
                m_mixChannelLeft[stemI][sI] = 0;
                m_mixChannelRight[stemI][sI] = 0;

                m_stemDataAmalgam.m_wave[stemI] = 0;
                m_stemDataAmalgam.m_beat[stemI] = 0;
                m_stemDataAmalgam.m_low[stemI]  = 0;
                m_stemDataAmalgam.m_high[stemI] = 0;
                continue;
            }

            const auto sampleCount = stemInst->m_sampleCount;
            uint64_t finalSampleIdx = riffSample;

            if (stemTimeStretch[stemI] != 1.0f)
            {
                double scaleSample = (double)finalSampleIdx * stemTimeStretch[stemI];
                finalSampleIdx = (uint64_t)scaleSample;
            }
            finalSampleIdx %= sampleCount;


            if ( stemInst->getAnalysisState() == endlesss::live::Stem::AnalysisState::AnalysisValid )
            {
                const float permGain = currentPermutation.m_layerGainMultiplier[stemI];

                const auto& stemAnalysis = stemInst->getAnalysisData();

                const float stemWave = stemAnalysis.getWaveF( finalSampleIdx ) * permGain;
                const float stemBeat = stemAnalysis.getBeatF( finalSampleIdx ) * permGain;
                const float stemLow  = stemAnalysis.getLowFreqF( finalSampleIdx ) * permGain;
                const float stemHigh = stemAnalysis.getHighFreqF( finalSampleIdx ) * permGain;

                m_stemDataAmalgam.m_wave[stemI] = std::max( m_stemDataAmalgam.m_wave[stemI], stemWave );
                m_stemDataAmalgam.m_beat[stemI] = std::max( m_stemDataAmalgam.m_beat[stemI], stemBeat );
                m_stemDataAmalgam.m_low[stemI]  = std::max( m_stemDataAmalgam.m_low[stemI],  stemLow  );
                m_stemDataAmalgam.m_high[stemI] = std::max( m_stemDataAmalgam.m_high[stemI], stemHigh );
            }

            m_mixChannelLeft[stemI][sI]  = stemInst->m_channel[0][finalSampleIdx] * stemGain;
            m_mixChannelRight[stemI][sI] = stemInst->m_channel[1][finalSampleIdx] * stemGain;
        }

        if ( m_transitionValue > 0 )
        {
            riffSample = riffWrappedSampleStart[1] + sI;
            if ( riffSample >= riffLengthInSamples[1] )
                riffSample -= riffLengthInSamples[1];

            for ( auto stemI = 0U; stemI < 8; stemI++ )
            {
                const endlesss::live::Stem* stemInst = stemPtr[ 8 + stemI ];
                const float       stemGain = stemGains[ 8 + stemI ];
        
                // when transitioning, a missing/muted stem means we need to transition down to silence, not just skip entirely
                if ( stemInst == nullptr || stemInst->hasFailed() )
                {
                    m_mixChannelLeft[stemI][sI]     = base::lerp( m_mixChannelLeft[stemI][sI],  0.0f, m_transitionValue );
                    m_mixChannelRight[stemI][sI]    = base::lerp( m_mixChannelRight[stemI][sI], 0.0f, m_transitionValue );

                    continue;
                }
        
                const auto sampleCount = stemInst->m_sampleCount;
                uint64_t finalSampleIdx = riffSample;
        
                if ( stemTimeStretch[ 8 + stemI ] != 1.0f )
                {
                    double scaleSample = (double)finalSampleIdx * stemTimeStretch[ 8 + stemI ];
                    finalSampleIdx = (uint64_t)scaleSample;
                }
                finalSampleIdx %= sampleCount;

                m_mixChannelLeft[stemI][sI]         = base::lerp( m_mixChannelLeft[stemI][sI],  stemInst->m_channel[0][finalSampleIdx] * stemGain, m_transitionValue );
                m_mixChannelRight[stemI][sI]        = base::lerp( m_mixChannelRight[stemI][sI], stemInst->m_channel[1][finalSampleIdx] * stemGain, m_transitionValue );
            }
        }
    }

    m_stemDataAmalgamSamplesUsed += samplesToWrite;

    // LINK logic EXTREMELY WIP HACK
    if ( currentRiff != nullptr )
    {
        const auto& timingData = currentRiff->getTimingDetails();

        // compute timing state when update() began
        endlesss::live::RiffProgression progressionAtEndOfUpdate;
        currentRiff->getTimingDetails().ComputeProgressionAtSample(
            m_samplePosition + samplesToWrite,
            progressionAtEndOfUpdate );

        const double timingQuantum = static_cast<double>(timingData.m_quarterBeats);

        const auto hostTime = m_abletonLinkControl.m_hostTimeFilter.sampleTimeToHostTime( m_abletonLinkControl.m_sampleTime );
        m_abletonLinkControl.m_sampleTime += static_cast<double>(samplesToWrite);

        const auto bufferBeginAtOutput = hostTime + m_abletonLinkControl.m_outputLatency;

        auto linkSessionState = m_abletonLinkControl.m_link.captureAudioSessionState();

        // always write our tempo. we are the tempo.
        linkSessionState.setTempo( timingData.m_bpm, bufferBeginAtOutput );

        // on the arrival of a new riff when nothing was playing, tag IsPlaying in the session state and force
        // a new 0-beat to match
        if ( !m_abletonLinkControl.m_linkIsPlaying )
        {
            linkSessionState.setIsPlaying( true, bufferBeginAtOutput );
            linkSessionState.forceBeatAtTime( 0, bufferBeginAtOutput, timingQuantum );
            m_abletonLinkControl.m_linkIsPlaying = true;
        }

        if ( progressionAtEndOfUpdate.m_playbackBarSegment != m_playbackProgression.m_playbackBarSegment )
        {
            std::chrono::microseconds zeroBeatUsOffset( std::llround( progressionAtEndOfUpdate.m_playbackQuarterTimeSec * 1.0e6 ) );

            if ( progressionAtEndOfUpdate.m_playbackBarSegment == 0 || m_abletonLinkControl.m_authorativeInterval >= 0 )
                linkSessionState.forceBeatAtTime( progressionAtEndOfUpdate.m_playbackBarSegment, bufferBeginAtOutput + zeroBeatUsOffset, timingQuantum );
            else
                linkSessionState.requestBeatAtTime( progressionAtEndOfUpdate.m_playbackBarSegment, bufferBeginAtOutput + zeroBeatUsOffset, timingQuantum );

            if ( m_abletonLinkControl.m_authorativeInterval >= 0 )
            {
                m_abletonLinkControl.m_authorativeInterval--;
                blog::mix( FMTX( "authorativeInterval:{}" ), m_abletonLinkControl.m_authorativeInterval );
            }
        }

        m_abletonLinkControl.m_link.commitAudioSessionState( linkSessionState );
    }

    commit( outputBuffer, outputSignal, samplesToWrite );
}



// ---------------------------------------------------------------------------------------------------------------------
struct BeamApp : public app::OuroApp,
                 public ux::TagLineToolProvider
{
    BeamApp()
        : app::OuroApp()
    {
        m_discordBotUI = std::make_unique<discord::BotWithUI>( *this );
    }

    const char* GetAppName() const override { return OUROVEON_BEAM; }
    const char* GetAppNameWithVersion() const override { return (OUROVEON_BEAM " " OUROVEON_BEAM_VERSION); }
    const char* GetAppCacheName() const override { return "beam"; }

    int EntrypointOuro() override;

protected:

    // ux::TagLineToolProvider
    // turn off all the tools, they don't really make sense to use inside BEAM (beyond the copy-to-clipboard)
    uint8_t getToolCount() const override
    {
        return 0;
    }

protected:

    enum class StreamSource : int32_t
    {
        EndlesssJam = 0,
        BOND = 1
    };

    StreamSource                            m_streamMode = 
#if OURO_HAS_NDLS_ONLINE
        StreamSource::EndlesssJam;
#else
        StreamSource::BOND;
#endif // OURO_HAS_NDLS_ONLINE

protected:

    using BondServerInstance = std::unique_ptr< net::bond::RiffPushServer >;

    static constexpr base::OperationVariant OV_RiffPlayback{ 0xBB };

    BondServerInstance                      m_bondServer;
    absl::Status                            m_bondServerStatus;

    uint32_t                                m_bondMessagesHandled = 0;

protected:

    endlesss::types::JamCouchID             m_trackedJamCouchID;

    MixEngine::ProgressionConfiguration     m_mixProgressionConfig;
    MixEngine::ProgressionConfiguration     m_mixProgressionConfigCommitted;

    MixEngine::RepComConfiguration          m_repComConfig;

    std::unique_ptr< ux::TagLine >          m_uxTagLine;

#if OURO_FEATURE_NST24
    std::unique_ptr< effect::EffectStack >  m_effectStack;
#endif // OURO_FEATURE_NST24

    std::unique_ptr< discord::BotWithUI >   m_discordBotUI;
};


// ---------------------------------------------------------------------------------------------------------------------
int BeamApp::EntrypointOuro()
{
    // create a lifetime-tracked provider object to pass to systems that want access to Riff-fetching abilities we provide
    endlesss::services::RiffFetchInstance riffFetchService( this );
    endlesss::services::RiffFetchProvider riffFetchProvider = riffFetchService.makeBound();


    // create and install the mixer engine
    MixEngine mixEngine(
        m_mdAudio->getMaximumBufferSize(),
        m_mdAudio->getSampleRate(),
        m_mdAudio->getOutputLatencyMs(),
        m_appEventBusClient.value() );
    m_mdAudio->blockUntil( m_mdAudio->installMixer( &mixEngine ) );

    // LINK controls for the mixer
    registerMainMenuEntry( 20, "LINK", [&mixEngine]()
        {
            static bool LinkEnableFlag = false;
            if ( ImGui::MenuItem( "Ableton Link", nullptr, &LinkEnableFlag ) )
            {
                blog::app( FMTX( "Requesting LINK state : {}" ), LinkEnableFlag ? "enabled" : "disabled" );
                mixEngine.enableAbletonLink( LinkEnableFlag );
            }
        });

#if OURO_FEATURE_NST24
    // VSTs for audio engine
    m_effectStack = std::make_unique<effect::EffectStack>( m_mdAudio.get(), mixEngine.getTimeInfoPtr(), "beam" );
    m_effectStack->load( m_appConfigPath );
#endif // OURO_FEATURE_NST24

    m_uxTagLine = std::make_unique<ux::TagLine>( getEventBusClient() );

    // == SNOOP ========================================================================================================

    bool        riffSyncInProgress = false;

    endlesss::toolkit::Sentinel jamSentinel( riffFetchProvider, [&]( endlesss::live::RiffPtr& riffPtr )
    {
        // enqueue for mixer
        mixEngine.addNextRiff( riffPtr );
    });

    ux::UniversalJamBrowserBehaviour jamBrowserBehaviour;
    jamBrowserBehaviour.fnOnSelected = [this]( const endlesss::types::JamCouchID& newJamCID )
    {
        m_trackedJamCouchID = newJamCID;
    };

    endlesss::toolkit::Pipeline riffPipeline(
        m_appEventBus,
        riffFetchProvider,
        32,
        [this]( const endlesss::types::RiffIdentity& request, endlesss::types::RiffComplete& result) -> bool
        {
            // most requests can be serviced direct from the DB
            if ( m_warehouse->fetchSingleRiffByID( request.getRiffID(), result ) )
            {
                ABSL_ASSERT( result.jam.couchID == request.getJamID() );

                endlesss::toolkit::Pipeline::applyCustomIdentityData( request, result );
                return true;
            }

            return endlesss::toolkit::Pipeline::defaultNetworkResolver( *m_networkConfiguration, request, result );
        },
        [&mixEngine]( const endlesss::types::RiffIdentity& request, endlesss::live::RiffPtr& loadedRiff, const endlesss::types::RiffPlaybackPermutationOpt& playbackPermutationOpt )
        {
            if ( loadedRiff )
            {
                mixEngine.addNextRiff( loadedRiff, playbackPermutationOpt );
            }
        },
        []()
        {
        } );



    // == MAIN LOOP ====================================================================================================

    while ( beginInterfaceLayout( (app::CoreGUI::ViewportFlags)(
        app::CoreGUI::VF_WithDocking   |
        app::CoreGUI::VF_WithMainMenu  |
        app::CoreGUI::VF_WithStatusBar ) ) )
    {
        mixEngine.mainThreadUpdate( GImGui->IO.DeltaTime, m_endlesssExchange );

        // run modal jam browser window if it's open
        bool modalDisplayJamBrowser = false;
        const char* modalJamBrowserTitle = "Jam Browser";
        ux::modalUniversalJamBrowser( modalJamBrowserTitle, m_jamLibrary, jamBrowserBehaviour, *this );


        // #HDD just do this on tracked choice change, doesn't need to be every frame
        endlesss::cache::Jams::Data trackedJamData;
        if ( m_trackedJamCouchID.empty() )
        {
            trackedJamData.m_displayName = "[ none selected ]";
        }
        else
        {
            std::string resolvedName;
            const auto lookupResult = lookupJamName( m_trackedJamCouchID, resolvedName );

            if ( lookupResult != endlesss::services::IJamNameResolveService::LookupResult::NotFound )
            {
                trackedJamData.m_displayName = resolvedName;
            }
            else
            {
                trackedJamData.m_displayName = "[ name unknown ]";
            }
        }

        auto currentRiffPerm = mixEngine.m_riffCurrent;

        {
            // process and blank out Exchange data ready to re-write it
            emitAndClearExchangeData();

            encodeExchangeData(
                currentRiffPerm.m_riffPtr,
                trackedJamData.m_displayName,
                (uint64_t)mixEngine.getTimeInfoPtr()->samplePos,
                nullptr );
        }


#if OURO_FEATURE_NST24
        {
            ImGui::Begin( "Effects" );
            m_effectStack->imgui( *this );
            ImGui::End();
        }
#endif // OURO_FEATURE_NST24

        m_discordBotUI->imgui( *this );

        {
            ImGui::Begin( "System" );

            const auto panelRegionAvailable = ImGui::GetContentRegionAvail();

            // expose gain control
            {
                float gainF = m_mdAudio->getOutputSignalGain();
                if ( ImGui::KnobFloat(
                    "##mix_gain",
                    24.0f,
                    &gainF,
                    0.0f,
                    1.0f,
                    2000.0f,
                    0.5f,
                    // custom tooltip showing dB instead of 0..1
                    []( const float percentage01, const float value ) -> std::string
                    {
                        if ( percentage01 <= std::numeric_limits<float>::min() )
                            return "-INF";

                        const auto dB = cycfi::q::lin_to_db( percentage01 );
                        return fmt::format( FMTX( "{:.2f} dB" ), dB.rep );
                    }))
                {
                    m_mdAudio->setOutputSignalGain( gainF );
                }
            }
            ImGui::SameLine( 0, 8.0f );
            // button to toggle end-chain mute on audio engine (but leave processing, WAV output etc intact)
            const bool isMuted = m_mdAudio->isMuted();
            {
                const char* muteIcon = isMuted ? ICON_FA_VOLUME_OFF : ICON_FA_VOLUME_HIGH;

                {
                    ImGui::Scoped::ToggleButton bypassOn( isMuted );
                    if ( ImGui::Button( muteIcon, ImVec2( 48.0f, 48.0f ) ) )
                        m_mdAudio->toggleMute();
                }
                ImGui::CompactTooltip( "Mute final audio output\nThis does not affect streaming or disk-recording" );
            }

            ImGui::SameLine( 0, 8.0f );
            if ( ImGui::BeginChild( "disk-recorders", ImVec2( 210.0f, 48.0f ) ) )
            {
                ux::widget::DiskRecorder( *m_mdAudio, m_storagePaths->outputApp );
                ux::widget::DiskRecorder( mixEngine, m_storagePaths->outputApp );
            }
            ImGui::EndChild();
            
            ImGui::Spacing();
            ImGui::SeparatorBreak();
            ImGui::Spacing();

            {
                const bool bIsSourceSwitchEnabled = ( m_bondServer == nullptr ) && ( jamSentinel.isTrackerRunning() == false );

                ImGui::Scoped::Enabled se( bIsSourceSwitchEnabled );
                ImGui::TextUnformatted( "Select Stream Source :" );
                ImGui::Indent( 16.0f );
#if !OURO_HAS_NDLS_ONLINE
                {
                    ImGui::Scoped::Disabled sd( true );
#endif // !OURO_HAS_NDLS_ONLINE
                    ImGui::RadioButton( " Live Endlesss Jam", (int32_t*) &m_streamMode, static_cast<int32_t>( StreamSource::EndlesssJam ) );
#if !OURO_HAS_NDLS_ONLINE
                }
#endif // !OURO_HAS_NDLS_ONLINE
                ImGui::RadioButton( " BOND Transmission", (int32_t*) &m_streamMode, static_cast<int32_t>( StreamSource::BOND ) );
                ImGui::Unindent( 16.0f );
                ImGui::Spacing();
                ImGui::Spacing();
                {
                    ImGui::PushStyleColor( ImGuiCol_Text, colour::shades::callout.light() );
                    ImGui::TextWrapped( "Once a stream source has been selected and used, you must fully disconnect it or restart BEAM to switch to another." );
                    ImGui::PopStyleColor();
                }
                ImGui::Spacing();
            }

            ImGui::Spacing();
            ImGui::SeparatorBreak();
            ImGui::Spacing();

            const float chunkyButtonHeight = 40.0f;
            ImGui::PushItemWidth( panelRegionAvailable.x );

            if ( m_streamMode == StreamSource::EndlesssJam )
            {
                const bool hasSelectedJam       = !m_trackedJamCouchID.empty();
                const bool isTracking           = jamSentinel.isTrackerRunning();
                const bool isTrackerBroken      = jamSentinel.isTrackerBroken();

                ImGui::TextUnformatted( "Selected Jam :" );

                {
                    ImGui::TextColored( 
                        isTracking ? ImGui::GetStyleColorVec4( ImGuiCol_PlotHistogram ) : ImGui::GetStyleColorVec4( ImGuiCol_SliderGrabActive ),
                        "%s",
                        trackedJamData.m_displayName.c_str() );

                    ImGui::Spacing();
                }
                {
                    ImGui::BeginDisabledControls( isTracking );

                    if ( ImGui::Button( ICON_FA_TABLE_LIST " Browse ...", ImVec2( panelRegionAvailable.x, chunkyButtonHeight ) ) )
                        modalDisplayJamBrowser = true;

                    ImGui::EndDisabledControls( isTracking );
                }

                ImGui::BeginDisabledControls( !hasSelectedJam );
                if ( isTracking )
                {
                    if ( isTrackerBroken )
                    {
                        if ( ImGui::Button( ICON_FA_TRIANGLE_EXCLAMATION " Tracker Failed", ImVec2( panelRegionAvailable.x, chunkyButtonHeight ) ) )
                        {
                            jamSentinel.startTracking( { m_trackedJamCouchID, trackedJamData.m_displayName } );
                        }
                    }
                    else
                    {
                        ImGui::Scoped::ToggleButton toggled( true, true );
                        if ( ImGui::Button( ICON_FA_CIRCLE_STOP " Tracking Changes", ImVec2( panelRegionAvailable.x, chunkyButtonHeight ) ) )
                        {
                            jamSentinel.stopTracking();
                        }
                    }
                }
                else
                {
                    if ( ImGui::Button( ICON_FA_CIRCLE_PLAY " Begin Tracking ", ImVec2( panelRegionAvailable.x, chunkyButtonHeight ) ) )
                    {
                        jamSentinel.startTracking( { m_trackedJamCouchID, trackedJamData.m_displayName } );
                    }
                }
                ImGui::EndDisabledControls( !hasSelectedJam );
            }
            if ( m_streamMode == StreamSource::BOND )
            {
                if ( m_bondServer == nullptr )
                {
                    if ( ImGui::Button( ICON_FA_CIRCLE_NODES " Start BOND Server ", ImVec2( panelRegionAvailable.x, chunkyButtonHeight ) ) )
                    {
                        m_trackedJamCouchID = {};
                        mixEngine.clearCurrentPlayback();

                        // create a BOND server instance; plug the riff-push handler to push all new requests
                        // straight into the riff resolver pipeline; these will then get enqueued into the mixer when they're available
                        m_bondServer = std::make_unique< net::bond::RiffPushServer >();
                        m_bondServer->setRiffPushedCallback( [&](
                            const endlesss::types::JamCouchID& jamID,
                            const endlesss::types::RiffCouchID& riffID,
                            const endlesss::types::RiffPlaybackPermutationOpt& permutationOpt )
                            {
                                const auto operationID = base::Operations::newID( OV_RiffPlayback );

                                riffPipeline.requestRiff( { { jamID, riffID }, permutationOpt, operationID } );

                                m_bondMessagesHandled++;
                            });

                        m_bondMessagesHandled = 0;
                        m_bondServerStatus = m_bondServer->start();
                    }
                }
                else
                {
                    if ( m_bondServerStatus.ok() )
                    {
                        ImGui::TextColored( colour::shades::green.light(), ICON_FA_CIRCLE_NODES " BOND Server Running" );
                        ImGui::Spacing();
                        ImGui::Text( "Messages Handled : %u", m_bondMessagesHandled );
                    }
                    else
                    {
                        ImGui::TextColored( colour::shades::errors.light(), "Server Error :" );
                        ImGui::TextWrapped( "%s", m_bondServerStatus.ToString().c_str() );
                    }
                    
                    ImGui::Spacing();
                    ImGui::Spacing();

                    if ( ImGui::Button( ICON_FA_CIRCLE_STOP " Stop BOND Server ", ImVec2( panelRegionAvailable.x, chunkyButtonHeight ) ) )
                    {
                        std::ignore = m_bondServer->stop();
                        m_bondServer.reset();
                    }
                }
            }

            ImGui::PopItemWidth();
            ImGui::End();
        }
        {
            ImGui::Begin( "Progression Settings" );
            const auto panelRegionAvailable = ImGui::GetContentRegionAvail();

            const bool splitIntoColumns = ( panelRegionAvailable.x > 500.0f );

            if ( splitIntoColumns )
            {
                ImGui::Columns( 2, nullptr, true );
                ImGui::SetColumnWidth( 0, panelRegionAvailable.x * 0.5f );
            }
            {
                ImGui::TextUnformatted( ICON_FA_SHUFFLE " Active Riff Blending" );
                ImGui::Spacing();
                ImGui::Spacing();

                const auto currentLineHeight = ImGui::GetTextLineHeight();
                const auto progressBarHeight = ImVec2( -1.0f, currentLineHeight * 1.25f );

                {
                    const auto* progTrigger = MixEngine::ProgressionConfiguration::getTriggerPointName( mixEngine.m_progression.m_triggerPoint );
                    const auto* progBlend = MixEngine::ProgressionConfiguration::getBlendTimeName( mixEngine.m_progression.m_blendTime );

                    ImGui::Text( "Trigger %s, %s %s",
                        progTrigger,
                        progBlend,
                        mixEngine.m_progression.m_greedyMode ? "(Leap)" : "(Sequential)" );
                }

                const bool itemsInRiffQueue = (mixEngine.m_riffQueue.size_approx() > 0);
                {
                    ImGui::PushStyleColor( ImGuiCol_PlotHistogram, ImGui::GetStyleColorVec4( ImGuiCol_NavHighlight ) );

                    if ( itemsInRiffQueue )
                        ImGui::ProgressBar( mixEngine.m_transitionValue, progressBarHeight, " Transition Scheduled ... " );
                    else if ( mixEngine.m_transitionValue > 0 )
                        ImGui::ProgressBar( mixEngine.m_transitionValue, progressBarHeight, " Transitioning ... " );
                    else
                        ImGui::ProgressBar( mixEngine.m_transitionValue, progressBarHeight, "" );

                    ImGui::PopStyleColor();
                }
                {
                    ImGui::Scoped::Enabled se( itemsInRiffQueue );
                    if ( ImGui::Button( ICON_FA_TRASH_CAN " Clear All Scheduled Transitions ", { -1.0f, 32.0f } ) )
                    {
                        mixEngine.clearAllScheduledTransitions();
                    }
                }


                ImGui::Spinner( "##syncing", riffSyncInProgress, currentLineHeight * 0.4f, 3.0f, 0.0f, ImGui::GetColorU32( ImGuiCol_Text ) );
                ImGui::SameLine();
                if ( riffSyncInProgress )
                    ImGui::TextUnformatted( " Fetching Stems ..." );
                else
                    ImGui::TextUnformatted( "" );
            }
            {
                ImGui::TextUnformatted( ICON_FA_SHUFFLE " Riff Blending" );
                ImGui::Spacing();
                ImGui::Spacing();

                ImGui::PushItemWidth( 180.0f );

                ImGui::Combo( " Trigger Point", (int32_t*)&m_mixProgressionConfig.m_triggerPoint, &MixEngine::ProgressionConfiguration::triggerPointGetter, nullptr, MixEngine::ProgressionConfiguration::cTriggerPointCount );
                ImGui::Combo( " Transition Time",  (int32_t*)&m_mixProgressionConfig.m_blendTime, &MixEngine::ProgressionConfiguration::blendTimeGetter,    nullptr, MixEngine::ProgressionConfiguration::cBlendTimeCount );
                //ImGui::Checkbox( "Empty Riff Queue On Transition", &m_mixProgressionConfig.m_greedyMode );

                const bool progressionIsUpToDate = ( m_mixProgressionConfigCommitted == m_mixProgressionConfig );
                ImGui::BeginDisabledControls( progressionIsUpToDate );
                {
                    ImGui::Scoped::ToggleButton highlightButton( !progressionIsUpToDate, true );
                    if ( ImGui::Button( " Activate " ) )
                    {
                        mixEngine.updateProgressionConfiguration( &m_mixProgressionConfig );
                        m_mixProgressionConfigCommitted = m_mixProgressionConfig;
                    }
                }
                ImGui::EndDisabledControls( progressionIsUpToDate );
            }
            if ( splitIntoColumns )
                ImGui::NextColumn();
            else
                ImGui::SeparatorBreak();
            {
                ImGui::TextUnformatted( ICON_FA_BARS_STAGGERED " Repetition Compression" );
                ImGui::Spacing();
                ImGui::Spacing();

                const bool recordingFreezeRepCom = mixEngine.isRecording();
                ImGui::BeginDisabledControls( recordingFreezeRepCom );
                {
                    if ( ImGui::Checkbox( "Enabled", &m_repComConfig.m_enable ) )
                    {
                        mixEngine.updateRepComConfiguration( &m_repComConfig );
                    }
                }
                ImGui::EndDisabledControls( recordingFreezeRepCom );

                if ( mixEngine.isRepComEnabled() )
                {
                    ImGui::Spacing();
                    if ( recordingFreezeRepCom )
                        ImGui::TextWrapped( "Cannot activate or deactivate while recording is underway" );
                    else
                        ImGui::TextWrapped( "Multitrack recording will pause when bars repeat too many times, unpausing when new transitions occur" );
                }
            }
            ImGui::Columns( 1 );

            ImGui::End();
        }
        {
            ImGui::Begin( "Current Riff" );
            const auto panelRegionAvailable = ImGui::GetContentRegionAvail();

            const auto* currentRiff = currentRiffPerm.m_riffPtr.get();

            ImGui::TextUnformatted( "Current Riff Source" );
            if ( m_bondServer != nullptr )
            {
                // empty title
                ImGui::PushStyleColor( ImGuiCol_Text, colour::shades::sea_green.neutralU32() );
                ImGui::PushFont( m_mdFrontEnd->getFont( app::module::Frontend::FontChoice::MediumTitle ) );
                ImGui::TextUnformatted( "BOND : " );
                ImGui::PopStyleColor();

                ImGui::SameLine(0, 0);
                if ( currentRiff )
                {
                    ImGui::TextColored( colour::shades::sea_green.light(), currentRiff->m_uiJamUppercase.c_str() );
                }
                else
                {
                    switch ( m_bondServer->getState() )
                    {
                        case net::bond::BondState::Disconnected:
                            ImGui::TextColored( colour::shades::green.dark(), "STOPPED" );
                            break;
                        case net::bond::BondState::InFlux:
                            ImGui::TextColored( colour::shades::green.neutral(), "STARTING ..." );
                            break;
                        case net::bond::BondState::Connected:
                            ImGui::TextColored( colour::shades::green.light(), "LIVE" );
                            break;
                    }
                }
                ImGui::PopFont();
            }
            else
            {
                if ( currentRiff )
                {
                    // jam title
                    ImGui::PushStyleColor( ImGuiCol_Text, colour::shades::sea_green.neutralU32() );
                    ImGui::PushFont( m_mdFrontEnd->getFont( app::module::Frontend::FontChoice::MediumTitle ) );
                    ImGui::TextUnformatted( currentRiff->m_uiJamUppercase.c_str() );
                    ImGui::PopFont();
                    ImGui::PopStyleColor();
                }
                else
                {
                    // empty title
                    ImGui::PushStyleColor( ImGuiCol_Text, colour::shades::blue_gray.neutralU32() );
                    ImGui::PushFont( m_mdFrontEnd->getFont( app::module::Frontend::FontChoice::MediumTitle ) );
                    ImGui::TextUnformatted( "NO CONNECTION" );
                    ImGui::PopFont();
                    ImGui::PopStyleColor();
                }
            }



            if ( currentRiffPerm.isNotEmpty() )
            {
                const ImVec2 verticalSpacer( 0.0f, 16.0f );

                ImGui::Dummy( verticalSpacer );
                if ( ImGui::BeginChild( "beat-box", ImVec2(0, 32.0f) ) )
                {
                    ux::StemBeats( "##stem_beat", m_endlesssExchange, 18.0f, false );
                }
                ImGui::EndChild();
                ImGui::Dummy( verticalSpacer );
                {
                    ImGui::BeatSegments( "##beats", m_endlesssExchange.m_riffBeatSegmentCount, m_endlesssExchange.m_riffBeatSegmentActive );
                    ImGui::Spacing();
                }
                {
                    ImGui::ProgressBar( (float)mixEngine.m_playbackProgression.m_playbackPercentage, ImVec2( -1, 3.0f ), "" );
                    ImGui::BeatSegments(
                        "##bars_play",
                        currentRiff->m_timingDetails.m_barCount,
                        mixEngine.m_playbackProgression.m_playbackBar,
                        -1,
                        3.0f,
                        ImGui::GetColorU32( ImGuiCol_PlotHistogram ) );

                    if ( mixEngine.isRepComEnabled() &&
                         mixEngine.isRecording() )
                    {
                        ImGui::Spacing();
                        ImGui::BeatSegments(
                            "##bars_sync",
                            currentRiff->m_timingDetails.m_barCount,
                            mixEngine.getBarPausedOn(),
                            -1,
                            3.0f,
                            ImGui::GetColorU32( ImGuiCol_HeaderActive )
                        );
                        ImGui::Spacing();

                        ImGui::Text( "Repetition Counter : %u | Last Pause Bar : %i", mixEngine.getBarRepetitions(), mixEngine.getBarPausedOn() );
                    }
                    ImGui::Spacing();
                }
                ImGui::Dummy( verticalSpacer );
                {
                    m_uxTagLine->imgui( currentRiffPerm.m_riffPtr, nullptr, this );
                }

                ImGui::Dummy( verticalSpacer );
                if ( ImGui::BeginTable( "##stem_stack", 11, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg ) )
                {
                    ImGui::PushStyleColor( ImGuiCol_Text, ImGui::GetStyleColorVec4( ImGuiCol_ResizeGripHovered ) );

                    ImGui::TableSetupColumn( "",        ImGuiTableColumnFlags_WidthFixed, 10.0f  );
                    ImGui::TableSetupColumn( "User",    ImGuiTableColumnFlags_WidthFixed, 140.0f );
                    ImGui::TableSetupColumn( "Instr",   ImGuiTableColumnFlags_WidthFixed, 60.0f  );
                    ImGui::TableSetupColumn( "Preset",  ImGuiTableColumnFlags_WidthFixed, 140.0f );
                    ImGui::TableSetupColumn( "Format",  ImGuiTableColumnFlags_WidthFixed, 70.0f  );
                    ImGui::TableSetupColumn( "Gain",    ImGuiTableColumnFlags_WidthFixed, 45.0f  );
                    ImGui::TableSetupColumn( "Speed",   ImGuiTableColumnFlags_WidthFixed, 45.0f  );
                    ImGui::TableSetupColumn( "Rep",     ImGuiTableColumnFlags_WidthFixed, 30.0f  );
                    ImGui::TableSetupColumn( "Length",  ImGuiTableColumnFlags_WidthFixed, 65.0f  );
                    ImGui::TableSetupColumn( "Rate",    ImGuiTableColumnFlags_WidthFixed, 65.0f  );
                    ImGui::TableSetupColumn( "Size/KB", ImGuiTableColumnFlags_WidthFixed, 65.0f  );
                    ImGui::TableHeadersRow();
                    ImGui::PopStyleColor();

                    for ( size_t sI=0; sI<8; sI++ )
                    {
                        const endlesss::live::Stem* stem = currentRiff->m_stemPtrs[sI];

                        if ( stem == nullptr )
                        {
                            ImGui::TableNextColumn(); ImGui::TextUnformatted( "" );
                            ImGui::TableNextColumn(); ImGui::TextUnformatted( "" );
                            ImGui::TableNextColumn(); ImGui::TextUnformatted( "" );
                            ImGui::TableNextColumn(); ImGui::TextUnformatted( "" );
                            ImGui::TableNextColumn(); ImGui::TextUnformatted( "" );
                            ImGui::TableNextColumn(); ImGui::TextUnformatted( "" );
                            ImGui::TableNextColumn(); ImGui::TextUnformatted( "" );
                            ImGui::TableNextColumn(); ImGui::TextUnformatted( "" );
                            ImGui::TableNextColumn(); ImGui::TextUnformatted( "" );
                            ImGui::TableNextColumn(); ImGui::TextUnformatted( "" );
                            ImGui::TableNextColumn(); ImGui::TextUnformatted( "" );
                        }
                        else
                        {
                            const auto& riffDocument = currentRiff->m_riffData;

                            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::VerticalProgress( "##gainBar", currentRiffPerm.m_permutation.m_layerGainMultiplier[sI] );
                            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted( stem->m_data.user.c_str() );
                            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted( stem->m_data.getInstrumentName() );
                            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted( stem->m_data.preset.c_str() );
                            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding();
                            switch ( stem->getCompressionFormat() )
                            {
                                case endlesss::live::Stem::Compression::Unknown:
                                    break;
                                case endlesss::live::Stem::Compression::OggVorbis:
                                    ImGui::PushStyleColor( ImGuiCol_Text, colour::shades::blue_gray.neutralU32() );
                                    ImGui::TextUnformatted( ICON_FA_CIRCLE " OggV" );
                                    ImGui::PopStyleColor();
                                    break;
                                case endlesss::live::Stem::Compression::FLAC:
                                    ImGui::PushStyleColor( ImGuiCol_Text, colour::shades::sea_green.darkU32() );
                                    ImGui::TextUnformatted( ICON_FA_CIRCLE_UP " FLAC" );
                                    ImGui::PopStyleColor();
                                    break;
                            }
                            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::Text( "%.2f",  m_endlesssExchange.m_stemGain[sI] );
                            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::Text( "%.2f",  currentRiff->m_stemTimeScales[sI] );
                            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::Text( "%ix",   currentRiff->m_stemRepetitions[sI] );
                            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::Text( "%.2fs", currentRiff->m_stemLengthInSec[sI] );
                            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::Text( "%i",    stem->m_data.sampleRate );
                            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::Text( "%i",    stem->m_data.fileLengthBytes / 1024 );
                        }
                    }

                    ImGui::EndTable();
                }
            }
            ImGui::End();
        }

        if ( modalDisplayJamBrowser )
            ImGui::OpenPopup( modalJamBrowserTitle );

        maintainStemCacheAsync();

        finishInterfaceLayoutAndRender();
    }

    m_uxTagLine.reset();

    m_discordBotUI.reset();

    m_mdAudio->blockUntil( m_mdAudio->installMixer( nullptr ) );
    m_mdAudio->blockUntil( m_mdAudio->effectClearAll() );

#if OURO_FEATURE_NST24
    m_effectStack->save( m_appConfigPath );
#endif // OURO_FEATURE_NST24

    return 0;
}

// ---------------------------------------------------------------------------------------------------------------------
int main( int argc, char** argv )
{
    BeamApp beam;
    const int result = beam.Run();
    if ( result != 0 )
        app::Core::waitForConsoleKey();

    return result;
}
