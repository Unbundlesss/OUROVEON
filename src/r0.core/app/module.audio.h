//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once
#include "app/module.h"

#include "base/utils.h"
#include "base/id.simple.h"
#include "base/instrumentation.h"

#include "spacetime/moment.h"

#include "rec/irecordable.h"
#include "ssp/isamplestreamprocessor.h"
#include "effect/container.h"


struct PaStreamCallbackTimeInfo;
struct PaStreamParameters;

namespace config { struct Audio; }
namespace vst { class Instance; }
namespace ssp { class WAVWriter; class FLACWriter; class OpusStream; }

namespace app {

struct StoragePaths;

namespace module {

struct MixerInterface;

struct Audio : public Module
             , public rec::IRecordable
             , public effect::IContainer
{
    Audio();
    ~Audio();

    using SampleProcessorInstances  = std::vector< ssp::SampleStreamProcessorInstance >;
    using SampleProcessorQueue      = mcc::ReaderWriterQueue< ssp::SampleStreamProcessorInstance >;


    // -----------------------------------------------------------------------------------------------------------------
    // fp32 working buffers for the mix process, covering somewhere to build results, flip buffers for VST processing, 
    // spares for padding VST inputs, etc. 
    //
    struct OutputBuffer
    {
        OutputBuffer( const uint32_t maxSamples )
            : m_maxSamples( maxSamples )
        {
            m_workingLR[0]      = mem::malloc16AsSet<float>( m_maxSamples, 0.0f );
            m_workingLR[1]      = mem::malloc16AsSet<float>( m_maxSamples, 0.0f );
            m_finalOutputLR[0]  = mem::malloc16AsSet<float>( m_maxSamples, 0.0f );
            m_finalOutputLR[1]  = mem::malloc16AsSet<float>( m_maxSamples, 0.0f );
            m_silence           = mem::malloc16AsSet<float>( m_maxSamples, 0.0f );
            m_runoff            = mem::malloc16AsSet<float>( m_maxSamples, 0.0f );
        }

        ~OutputBuffer()
        {
            mem::free16( m_runoff );
            mem::free16( m_silence );
            mem::free16( m_workingLR[1] );
            mem::free16( m_workingLR[0] );
        }

        // blit silence into all output channels
        inline void applySilence() const
        {
            memcpy( m_workingLR[0],   m_silence, sizeof( float ) * m_maxSamples );
            memcpy( m_workingLR[1],   m_silence, sizeof( float ) * m_maxSamples );
        }

        // .. apply silence from a sample offset to end of buffer
        inline void applySilenceFrom( const uint32_t sampleOffset ) const
        {
            if ( sampleOffset >= m_maxSamples )
                return;

            memcpy( m_workingLR[0] + sampleOffset, m_silence, sizeof( float ) * (m_maxSamples - sampleOffset) );
            memcpy( m_workingLR[1] + sampleOffset, m_silence, sizeof( float ) * (m_maxSamples - sampleOffset) );
        }


        uint32_t                m_maxSamples;
        std::array<float*, 2>   m_workingLR;        // data is written here by the mixer function
        std::array<float*, 2>   m_finalOutputLR;    // we use these as flip-flop working buffers for VST processing
        float*                  m_silence;          // empty input channel available for use as VST input (for example)
        float*                  m_runoff;           // empty output channel to pad out for VSTs using >2 outputs
    };


    // an arbitrary choice, as some things need an estimate upfront - and even once a portaudio stream is open the sample count
    // can potentially change. Choose something big to give us breathing room. 
    inline int32_t getMaximumBufferSize() const { return 1024 * 4; }


    // Module
    bool create( const app::Core& appCore ) override;
    void destroy() override;

    // initialise chosen audio device
    bool initOutput( const config::Audio& outputDevice );
    void termOutput();

    inline int32_t getSampleRate() const { assert( m_sampleRate > 0 ); return m_sampleRate; }


    struct ExposedState
    {
        // we profile different chunks of the mixer execution to track what specific stages might be sluggish
        // no timing or averaging is done in here, we leave that to the UI thread
        enum class ExecutionStage
        {
            Start,
            Mixer,                  // mixer logic
            VSTs,                   // external VST processing
            Interleave,             // move results into PA buffers
            SampleProcessing,       // pushing new samples out to any attached processors, like record-to-disk or discord-transmit
            Count
        };
        static constexpr std::array< const char*, (size_t)ExecutionStage::Count > ExecutionStageEventName =
        {
            "Start",
            "Mixer",
            "VSTs",
            "Interleave",
            "SampleProcessing"
            "Invalid"
        };

        static constexpr size_t     cNumExecutionStages         = (size_t)ExecutionStage::Count;
        static constexpr size_t     cPerfTrackSlots             = 15;
        static constexpr double     cPerfSumRcp                 = 1.0 / (double)cPerfTrackSlots;

        using CounterArray      = std::array< uint64_t, cPerfTrackSlots >;
        using StageCounters     = std::array< CounterArray, cNumExecutionStages >;


        ExposedState()
        {
            m_samplePos             = 0;
            m_perfTraceIndex        = 0;

            m_maxBufferFillSize     = 0;
            m_minBufferFillSize     = std::numeric_limits<uint32_t>::max();

            for ( auto& ca : m_perfCounters )
                ca.fill( 0 );
        }

        // called after a block of work has been done to log performance metrics for that stage
        inline void mark( const ExecutionStage es )
        {
            const size_t stageIndex = (size_t)es;

            // [Start] is an empty block, so there will be no event running
            if ( stageIndex > 0 )
                base::instr::eventEnd();

            m_perfCounters[stageIndex][m_perfTraceIndex] = m_timingMoment.deltaUs().count();
            m_timingMoment.restart();

            // on last phase, cycle trace index
            if ( stageIndex == cNumExecutionStages - 1 )
            {
                m_perfTraceIndex++;
                if ( m_perfTraceIndex >= cPerfTrackSlots )
                    m_perfTraceIndex = 0;
            }
            else
            {
                base::instr::eventBegin( "AUDIOPROC", ExecutionStageEventName[stageIndex + 1] );
            }
        }

        spacetime::Moment   m_timingMoment;

        uint64_t            m_samplePos;                // incremented by audio callback, tracks total sample count
        uint32_t            m_maxBufferFillSize;        // tracking range of min/max audio buffer fill request sizes
        uint32_t            m_minBufferFillSize;        //

        // perf counter snapshots at start/mid/end of mixer process
        StageCounters       m_perfCounters;
        uint32_t            m_perfTraceIndex;
    };

    inline const ExposedState& getState() const { return m_state; }
    double getAudioEngineCPULoadPercent() const;

    inline float getMasterGain() const { return m_gain; }
    inline void setMasterGain( const float v ) { m_gain = v; }

    AsyncCommandCounter toggleMute();
    inline bool isMuted() const { return m_mute; }

private:

    enum class MixThreadCommand
    {
        Invalid,
        SetMixerFunction,
        InstallVST,
        ClearAllVSTs,
        ToggleVSTBypass,
        ToggleMute,
        AttachSampleProcessor,
        DetatchSampleProcessor
    };
    struct MixThreadCommandData : public base::BasicCommandType<MixThreadCommand> { using BasicCommandType::BasicCommandType; };
    using MixThreadCommandQueue = mcc::ReaderWriterQueue<MixThreadCommandData>;

    void ProcessMixCommandsOnMixThread();


    static int PortAudioCallback(
        const void* inputBuffer,
        void* outputBuffer,
        unsigned long framesPerBuffer,
        const PaStreamCallbackTimeInfo* timeInfo,
        unsigned long /*PaStreamCallbackFlags*/ statusFlags,
        void* userData );

    int PortAudioCallbackInternal(
        void* outputBuffer,
        unsigned long framesPerBuffer,
        const PaStreamCallbackTimeInfo* timeInfo );

    MixThreadCommandQueue               m_mixThreadCommandQueue;
    std::atomic_uint32_t                m_mixThreadCommandsIssued   = 0;
    std::atomic_uint32_t                m_mixThreadCommandsComplete = 0;

    // active output 
    int32_t                             m_paDeviceIndex     = -1;
    void*                               m_paStream          = nullptr;
    uint32_t                            m_sampleRate        = 0;
    OutputBuffer*                       m_mixerBuffers      = nullptr;      // the aligned intermediate buffer, filled by the configurable mixer process
    bool                                m_mute              = false;

    ExposedState                        m_state;

#if OURO_FEATURE_VST24
    using VSTFxSlots = std::vector< vst::Instance* >;

    VSTFxSlots                          m_vstiStack;                        // VST slots, executed in order
    bool                                m_vstBypass = false;
#endif // OURO_FEATURE_VST24

    std::atomic<float>                  m_gain            = 0.5f;           // generic 0..1 global gain control

    MixerInterface*                     m_mixerInterface    = nullptr;


    SampleProcessorQueue                m_sampleProcessorsToInstall;
    SampleProcessorInstances            m_sampleProcessorsInstalled;

    ssp::SampleStreamProcessorInstance  m_currentRecorderProcessor;

public:

    // rec::IRecordable
    bool beginRecording( const std::string& outputPath, const std::string& filePrefix ) override;
    void stopRecording() override;
    bool isRecording() const override;
    uint64_t getRecordingDataUsage() const override;
    const char* getRecorderName() const override { return " Final Mix "; }


public: 

    // effect::IContainer
    int32_t getEffectSampleRate() const override { return getSampleRate(); }
    int32_t getEffectMaximumBufferSize() const override { return getMaximumBufferSize(); }

    AsyncCommandCounter toggleEffectBypass() override;
    bool isEffectBypassEnabled() const override;

    AsyncCommandCounter effectAppend( vst::Instance* vst ) override;
    AsyncCommandCounter effectClearAll() override;


public:

    AsyncCommandCounter installMixer( MixerInterface* mixIf );

    AsyncCommandCounter attachSampleProcessor( ssp::SampleStreamProcessorInstance sspInstance );
    AsyncCommandCounter detachSampleProcessor( ssp::StreamProcessorInstanceID sspID );

    void blockUntil( AsyncCommandCounter counter );
};

// interface class used to plug sound output generators into the Audio instance; this is how a client app interacts with the audio engine
struct MixerInterface
{
    // called from the realtime audio processing thread
    virtual void update(
        const Audio::OutputBuffer& outputBuffer,
        const float                outputVolume,
        const uint32_t             samplesToWrite,
        const uint64_t             samplePosition ) = 0;

    // called from the main UI thread
    virtual void imgui( const app::StoragePaths& storagePaths ) {}

    // optionally cast / return a rec::IRecordable interface to expose disk output functionality
    virtual rec::IRecordable* getRecordable() { return nullptr; }
};

} // namespace module

// ---------------------------------------------------------------------------------------------------------------------
struct AudioDeviceQuery
{
    void findSuitable( const config::Audio& audioConfig );

    static int32_t generateStreamParametersFromDeviceConfig( const config::Audio& audioConfig, PaStreamParameters& streamParams );

    std::vector< std::string >      m_deviceNames;
    std::vector< const char* >      m_deviceNamesUnpacked;
    std::vector< double >           m_deviceLatencies;
    std::vector< int32_t >          m_devicePaIndex;
};

using AudioModule = std::unique_ptr<module::Audio>;


// ---------------------------------------------------------------------------------------------------------------------
// a shared memory block based on the timing data required to be fed into VSTs. I'm not really sure this is a good idea!
// mixers can be handed a pointer and update it to sync data direct to all live effects. REFACTOR THIS LATER
//
struct AudioPlaybackTimeInfo
{
    double samplePos;           // current Position in audio samples (always valid)
    double sampleRate;          // current Sample Rate in Herz (always valid)
    double nanoSeconds;         // System Time in nanoseconds (10^-9 second)
    double ppqPos;              // Musical Position, in Quarter Note (1.0 equals 1 Quarter Note)
    double tempo;               // current Tempo in BPM (Beats Per Minute)
    double barStartPos;         // last Bar Start Position, in Quarter Note
    double cycleStartPos;       // Cycle Start (left locator), in Quarter Note
    double cycleEndPos;         // Cycle End (right locator), in Quarter Note
    int32_t timeSigNumerator;   // Time Signature Numerator (e.g. 3 for 3/4)
    int32_t timeSigDenominator; // Time Signature Denominator (e.g. 4 for 3/4)
};

} // namespace app
