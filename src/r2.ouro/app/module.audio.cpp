//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  Frontend controls initialistion of the rendering canvas and IMGUI 
//  instance, along with the update and display hooks
//

#include "pch.h"
#include "app/core.h"
#include "app/module.audio.h"

#include "config/audio.h"

#include "base/mathematics.h"
#include "dsp/scope.h"

#include "effect/vst2/host.h"
#include "win32/errors.h"
#include "win32/utils.h"

#include "ssp/ssp.file.wav.h"
#include "ssp/ssp.file.flac.h"

#include "portaudio.h"


namespace app {
namespace module {

// ---------------------------------------------------------------------------------------------------------------------
Audio::Audio() 
{
}

// ---------------------------------------------------------------------------------------------------------------------
Audio::~Audio()
{
}

// ---------------------------------------------------------------------------------------------------------------------
absl::Status Audio::create( const app::Core* appCore )
{
    const auto baseStatus = Module::create( appCore );
    if ( !baseStatus.ok() )
        return baseStatus;

    // bring up PA, check we have things to play with
    const auto paErrorInit = Pa_Initialize();
    if ( paErrorInit != paNoError )
    {
        return absl::UnavailableError( 
            fmt::format( "PortAudio failed to initalise, {}", Pa_GetErrorText( paErrorInit ) ) );
    }

    const auto paDeviceCount = Pa_GetDeviceCount();
    if ( paDeviceCount <= 0 )
    {
        return absl::UnavailableError( "PortAudio was unable to iterate or find any audio devices" );
    }

    blog::core( "Initialised PortAudio [ {} ]", Pa_GetVersionText() );
    blog::core( "                      [ .. found {} audio endpoints ]", paDeviceCount );

    const auto hostAPIcount     = Pa_GetHostApiCount();
    const auto hostAPIdefault   = Pa_GetDefaultHostApi();
    blog::core( "                      [ .. {} host APIs ]", hostAPIcount );
    for ( auto hApi = 0; hApi < hostAPIcount; hApi++ )
    {
        const auto* apiInfo = Pa_GetHostApiInfo( hApi );
        blog::core( "                      [ {} {}]", apiInfo->name, ( hostAPIdefault == hApi ) ? "(default) " : "" );
    }

    return absl::OkStatus();
}

// ---------------------------------------------------------------------------------------------------------------------
void Audio::destroy()
{
    if ( m_paStream != nullptr )
        termOutput();

    // graceful audio shutdown
    const auto paErrorTerm = Pa_Terminate();
    if ( paErrorTerm != paNoError )
    {
        blog::error::core( "PortAudio failed to terminate cleanly, {}", Pa_GetErrorText( paErrorTerm ) );
    }

    Module::destroy();
}

// ---------------------------------------------------------------------------------------------------------------------
absl::Status Audio::initOutput( const config::Audio& outputDevice, const config::Spectrum& scopeSpectrumConfig )
{
    blog::core( "Establishing audio output using [{}] @ {}", outputDevice.lastDevice, outputDevice.sampleRate );

    // cache sample rate
    m_sampleRate = outputDevice.sampleRate;

    // create the rolling spectrum scope
    m_scope = std::make_unique< dsp::Scope8 >( 1.0f / 60.0f, m_sampleRate, scopeSpectrumConfig );

    PaStreamParameters outputParameters;
    if ( AudioDeviceQuery::generateStreamParametersFromDeviceConfig( outputDevice, outputParameters ) < 0 )
    {
        return absl::UnavailableError( "Unable to resolve stream parameters" );
    }

    PaError err = Pa_OpenStream(
                        &m_paStream,
                        nullptr,
                        &outputParameters,
                        outputDevice.sampleRate,
                        outputDevice.bufferSize,
                        paNoFlag,
                        PortAudioCallback,
                        (void *)this );

    if ( err != paNoError )
    {
        return absl::UnavailableError( fmt::format( FMTX( "Pa_OpenStream failed [{}]" ), Pa_GetErrorText( err ) ) );
    }

    m_mixerBuffers = new OutputBuffer( getMaximumBufferSize() );

    err = Pa_StartStream( m_paStream );
    if ( err != paNoError )
    {
        termOutput();

        return absl::UnavailableError( fmt::format( FMTX( "Pa_StartStream failed [{}]" ), Pa_GetErrorText( err ) ) );
    }

    return absl::OkStatus();
}

// ---------------------------------------------------------------------------------------------------------------------
void Audio::termOutput()
{
    if ( m_paStream )
    {
        Pa_StopStream( m_paStream );
        Pa_CloseStream( m_paStream );
        m_paStream = nullptr;
    }

    if ( m_mixerBuffers != nullptr )
    {
        delete m_mixerBuffers;
        m_mixerBuffers = nullptr;
    }

    m_sampleRate = 0;
}


// ---------------------------------------------------------------------------------------------------------------------
double Audio::getAudioEngineCPULoadPercent() const
{
    if ( m_paStream == nullptr )
        return 0;

    return Pa_GetStreamCpuLoad( m_paStream ) * 100.0;
}

// ---------------------------------------------------------------------------------------------------------------------
AsyncCommandCounter Audio::toggleMute()
{
    const uint32_t commandCounter = m_mixThreadCommandsIssued++;
    m_mixThreadCommandQueue.emplace( MixThreadCommand::ToggleMute );
    return AsyncCommandCounter{ commandCounter };
}

// ---------------------------------------------------------------------------------------------------------------------
int Audio::PortAudioCallback(
    const void* inputBuffer,
    void* outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData )
{
    app::module::Audio* audioModule = static_cast<app::module::Audio*>(userData);

    if ( !audioModule->m_threadInitOnce )
    {
        ouroveonThreadEntry( OURO_THREAD_PREFIX "AudioMix" );
        audioModule->m_threadInitOnce = true;
    }

    return audioModule->PortAudioCallbackInternal( outputBuffer, framesPerBuffer, timeInfo );
}

// ---------------------------------------------------------------------------------------------------------------------
void Audio::ProcessMixCommandsOnMixThread()
{
    MixThreadCommandData mixCmdData;
    while ( m_mixThreadCommandQueue.try_dequeue( mixCmdData ) )
    {
        const int64_t commandI64 = mixCmdData.getI64();

        switch ( mixCmdData.getCommand() )
        {
            case MixThreadCommand::Invalid:
            default:
                assert( 0 );
                break;

            case MixThreadCommand::SetMixerFunction:
                m_mixerInterface = mixCmdData.getPtrAs<MixerInterface>();
                break;
            case MixThreadCommand::InstallVST:
#if OURO_FEATURE_VST24
                m_vstiStack.emplace_back( mixCmdData.getPtrAs<vst::Instance>() );
#endif // OURO_FEATURE_VST24
                break;
            case MixThreadCommand::ClearAllVSTs:
#if OURO_FEATURE_VST24
                m_vstiStack.clear();
#endif // OURO_FEATURE_VST24
                break;
            case MixThreadCommand::ToggleVSTBypass:
#if OURO_FEATURE_VST24
                m_vstBypass = !m_vstBypass;
#endif // OURO_FEATURE_VST24
                break;
            case MixThreadCommand::ToggleMute:
                m_mute = !m_mute;
                break;
            case MixThreadCommand::AttachSampleProcessor:
                {
                    ssp::SampleStreamProcessorInstance instance;
                    if ( m_sampleProcessorsToInstall.try_dequeue( instance ) )
                    {
                        m_sampleProcessorsInstalled.emplace_back( instance );
                        blog::mix( "attaching SSP [{}]", instance->getInstanceID().get() );
                    }
                    else
                    {
                        blog::error::mix( "unable to deque new SSP on mix thread" );
                    }
                }
                break;
            case MixThreadCommand::DetatchSampleProcessor:
                {
                    ssp::StreamProcessorInstanceID idToRemove( (uint32_t)commandI64 );

                    auto new_end = std::remove_if( m_sampleProcessorsInstalled.begin(),
                                                   m_sampleProcessorsInstalled.end(),
                                                  [=]( ssp::SampleStreamProcessorInstance& ssp )
                                                  {
                                                      return ssp->getInstanceID() == idToRemove;
                                                  });
                    if ( new_end == m_sampleProcessorsInstalled.end() )
                    {
                        blog::error::mix( "unable to detach SSP [{}]", commandI64 );
                    }
                    else
                    {
                        blog::mix( "detaching SSP [{}]", commandI64 );
                    }
                    m_sampleProcessorsInstalled.erase( new_end, m_sampleProcessorsInstalled.end() );
                }
                break;
        }

        m_mixThreadCommandsComplete++;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
int Audio::PortAudioCallbackInternal( void* outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo )
{
    assert( m_mixerBuffers );
    ProcessMixCommandsOnMixThread();

    m_state.mark( ExposedState::ExecutionStage::Start );

    // pass control to the installed mixer, if we have one
    if ( m_mixerInterface != nullptr )
    {
        m_mixerInterface->update( *m_mixerBuffers, m_gain, framesPerBuffer, m_state.m_samplePos );
    }
    // otherwise, ssshhh
    else
    {
        m_mixerBuffers->applySilence();
    }

    // set up the buffer pointers for the final stage of the process
    float* inputs[8]
    {
        m_mixerBuffers->m_workingLR[0],
        m_mixerBuffers->m_workingLR[1],
        m_mixerBuffers->m_silence,
        m_mixerBuffers->m_silence,
        m_mixerBuffers->m_silence,
        m_mixerBuffers->m_silence,
        m_mixerBuffers->m_silence,
        m_mixerBuffers->m_silence
    };
    float* outputs[8]
    {
        m_mixerBuffers->m_finalOutputLR[0],
        m_mixerBuffers->m_finalOutputLR[1],
        m_mixerBuffers->m_runoff,
        m_mixerBuffers->m_runoff,
        m_mixerBuffers->m_runoff,
        m_mixerBuffers->m_runoff,
        m_mixerBuffers->m_runoff,
        m_mixerBuffers->m_runoff
    };

    // track performance pre-VST 
    m_state.mark( ExposedState::ExecutionStage::Mixer );

    // if we don't use VSTs to process our samples into the output buffer, then we must eventually manually copy them raw; this tracks that
    bool outputsWritten = false;

#if OURO_FEATURE_VST24
    if ( m_vstBypass == false )
    {
        for ( auto vstInst : m_vstiStack )
        {
            if ( vstInst &&
                 vstInst->availableForUse() )
            {
                vstInst->process( inputs, outputs, framesPerBuffer );
                inputs[0] = outputs[0];
                inputs[1] = outputs[1];
                outputsWritten = true;
            }
        }
    }
#endif // OURO_FEATURE_VST24

    m_state.mark( ExposedState::ExecutionStage::VSTs );

    // by default we'll assume the results are still in the working buffers
    float* resultChannelLeft  = inputs[0];
    float* resultChannelRight = inputs[1];

    // .. but if VSTs ran, we will use the resulting output buffers
    if ( outputsWritten )
    {
        resultChannelLeft  = outputs[0];
        resultChannelRight = outputs[1];
    }

    m_scope->append( resultChannelLeft, resultChannelRight, framesPerBuffer );

    m_state.mark( ExposedState::ExecutionStage::Scope );

    {
        float* out = (float*)outputBuffer;
        for ( auto i = 0U; i < framesPerBuffer; i++ )
        {
            out[0] = resultChannelLeft[i];
            out[1] = resultChannelRight[i];
            out += 2;
        }
    }

    // update state block
    {
        uint32_t fpb32 = (uint32_t)framesPerBuffer;
        m_state.m_maxBufferFillSize = std::max( m_state.m_maxBufferFillSize, fpb32 );
        m_state.m_minBufferFillSize = std::min( m_state.m_minBufferFillSize, fpb32 );
        m_state.m_samplePos += fpb32;
    }

    m_state.mark( ExposedState::ExecutionStage::Interleave );

    for ( auto& ssp : m_sampleProcessorsInstalled )
    {
        ssp->appendSamples( resultChannelLeft, resultChannelRight, framesPerBuffer );
    }

    m_state.mark( ExposedState::ExecutionStage::SampleProcessing );

    // mute only the final output if requested - preserve all the processed work and WAV-output
    if ( m_mute )
        memset( outputBuffer, 0, sizeof( float ) * framesPerBuffer * 2 );

    return 0;
}

// ---------------------------------------------------------------------------------------------------------------------
bool Audio::beginRecording( const fs::path& outputPath, const std::string& filePrefix )
{
    assert( !isRecording() );

    auto outputFilename = fs::absolute( outputPath / fmt::format( "{}_finalmix.flac", filePrefix ) ).string();
    blog::core( "Opening FLAC final-mix output '{}'", outputFilename );

    assert( m_currentRecorderProcessor == nullptr );
    m_currentRecorderProcessor = ssp::FLACWriter::Create( outputFilename, getSampleRate(), 1.0f );
    if ( m_currentRecorderProcessor )
    {
        attachSampleProcessor( m_currentRecorderProcessor );
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------------------------------------------------
void Audio::stopRecording()
{
    assert( isRecording() );
    blockUntil( 
        detachSampleProcessor( m_currentRecorderProcessor->getInstanceID() ) );
    m_currentRecorderProcessor.reset();
}

// ---------------------------------------------------------------------------------------------------------------------
bool Audio::isRecording() const
{
    return ( m_currentRecorderProcessor != nullptr );
}

// ---------------------------------------------------------------------------------------------------------------------
uint64_t Audio::getRecordingDataUsage() const
{
    if ( isRecording() )
        return m_currentRecorderProcessor->getStorageUsageInBytes();

    return 0;
}

// ---------------------------------------------------------------------------------------------------------------------
AsyncCommandCounter Audio::toggleEffectBypass()
{
    const uint32_t commandCounter = m_mixThreadCommandsIssued++;
    m_mixThreadCommandQueue.emplace( MixThreadCommand::ToggleVSTBypass );
    return AsyncCommandCounter{ commandCounter };
}

// ---------------------------------------------------------------------------------------------------------------------
bool Audio::isEffectBypassEnabled() const
{
#if OURO_FEATURE_VST24
    return m_vstBypass;
#else
    return false;
#endif 
}

// ---------------------------------------------------------------------------------------------------------------------
AsyncCommandCounter Audio::effectAppend( vst::Instance* vst )
{
    const uint32_t commandCounter = m_mixThreadCommandsIssued++;
    m_mixThreadCommandQueue.emplace( MixThreadCommand::InstallVST, vst );
    return AsyncCommandCounter{ commandCounter };
}

// ---------------------------------------------------------------------------------------------------------------------
AsyncCommandCounter Audio::effectClearAll()
{
    const uint32_t commandCounter = m_mixThreadCommandsIssued++;
    m_mixThreadCommandQueue.emplace( MixThreadCommand::ClearAllVSTs );
    return AsyncCommandCounter{ commandCounter };
}

// ---------------------------------------------------------------------------------------------------------------------
AsyncCommandCounter Audio::installMixer( MixerInterface* mixIf )
{
    const uint32_t commandCounter = m_mixThreadCommandsIssued++;
    m_mixThreadCommandQueue.emplace( MixThreadCommand::SetMixerFunction, mixIf );
    return AsyncCommandCounter{ commandCounter };
}


// ---------------------------------------------------------------------------------------------------------------------
AsyncCommandCounter Audio::attachSampleProcessor( ssp::SampleStreamProcessorInstance sspInstance )
{
    const uint32_t commandCounter = m_mixThreadCommandsIssued++;
    m_sampleProcessorsToInstall.enqueue( sspInstance );
    m_mixThreadCommandQueue.emplace( MixThreadCommand::AttachSampleProcessor, commandCounter );
    return AsyncCommandCounter{ commandCounter };
}

// ---------------------------------------------------------------------------------------------------------------------
AsyncCommandCounter Audio::detachSampleProcessor( ssp::StreamProcessorInstanceID sspID )
{
    const uint32_t commandCounter = m_mixThreadCommandsIssued++;
    m_mixThreadCommandQueue.emplace( MixThreadCommand::DetatchSampleProcessor, sspID.get() );
    return AsyncCommandCounter{ commandCounter };
}

// ---------------------------------------------------------------------------------------------------------------------
void Audio::blockUntil( AsyncCommandCounter counter )
{
    for ( ;; )
    {
        const uint32_t currentCounter = m_mixThreadCommandsComplete.load();
        if ( currentCounter > counter.get() )
            break;

        std::this_thread::yield();
    }
}

} // namespace module

// ---------------------------------------------------------------------------------------------------------------------
void AudioDeviceQuery::findSuitable( const config::Audio& audioConfig )
{
    m_deviceNames.clear();
    m_deviceNamesUnpacked.clear();
    m_deviceLatencies.clear();
    m_devicePaIndex.clear();

    const auto paDeviceCount = Pa_GetDeviceCount();
    if ( paDeviceCount <= 0 )
        return;

    std::unordered_set< std::string > uniqueDeviceNames;
    uniqueDeviceNames.reserve( paDeviceCount );

    PaStreamParameters outputParameters;
    memset( &outputParameters, 0, sizeof( outputParameters ) );
    outputParameters.channelCount              = 2;
    outputParameters.sampleFormat              = paFloat32;

    const PaDeviceInfo* paDeviceInfo;
    for ( auto paDi = 0; paDi < paDeviceCount; paDi++ )
    {
        paDeviceInfo = Pa_GetDeviceInfo( paDi );
        outputParameters.device                    = paDi;
        outputParameters.suggestedLatency          = audioConfig.lowLatency ? paDeviceInfo->defaultLowOutputLatency :
                                                                              paDeviceInfo->defaultHighOutputLatency;

        if ( Pa_IsFormatSupported( nullptr, &outputParameters, audioConfig.sampleRate ) == paFormatIsSupported  )
        {
            const auto paDeviceName = std::string( paDeviceInfo->name );

            if ( uniqueDeviceNames.find( paDeviceName ) == uniqueDeviceNames.end() )
            {
                uniqueDeviceNames.emplace( paDeviceName );

                m_deviceNames.emplace_back( std::move( paDeviceName ) );
                m_deviceLatencies.push_back( outputParameters.suggestedLatency );
                m_devicePaIndex.push_back( paDi );
            }
        }
    }

    for ( const auto& eachName : m_deviceNames )
        m_deviceNamesUnpacked.emplace_back( eachName.c_str() );
}

// ---------------------------------------------------------------------------------------------------------------------
int32_t AudioDeviceQuery::generateStreamParametersFromDeviceConfig( const config::Audio& audioConfig, PaStreamParameters& streamParams )
{
    memset( &streamParams, 0, sizeof( PaStreamParameters ) );
    streamParams.channelCount              = 2;
    streamParams.sampleFormat              = paFloat32;

    const auto paDeviceCount = Pa_GetDeviceCount();
    if ( paDeviceCount <= 0 )
        return -1;

    const PaDeviceInfo* paDeviceInfo;
    for ( auto paDi = 0; paDi < paDeviceCount; paDi++ )
    {
        paDeviceInfo = Pa_GetDeviceInfo( paDi );

        streamParams.device                    = paDi;
        streamParams.suggestedLatency          = audioConfig.lowLatency ? paDeviceInfo->defaultLowOutputLatency :
                                                                          paDeviceInfo->defaultHighOutputLatency;

        if ( Pa_IsFormatSupported( nullptr, &streamParams, audioConfig.sampleRate ) == paFormatIsSupported )
        {
            const auto paDeviceName = std::string( paDeviceInfo->name );
            if ( paDeviceName == audioConfig.lastDevice )
            {
                blog::core( "Selecting device #{} [{}]", paDi, paDeviceName );

                return paDi;
            }
        }
    }
    return -1;
}

} // namespace app
