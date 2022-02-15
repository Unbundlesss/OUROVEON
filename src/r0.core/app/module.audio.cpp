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

#include "effect/vst2/host.h"
#include "win32/errors.h"
#include "win32/utils.h"

#include "ssp/wav.h"
#include "ssp/flac.h"

#include "portaudio.h"

#include <objbase.h>


namespace app {
namespace module {

// ---------------------------------------------------------------------------------------------------------------------
Audio::Audio() 
    : m_sampleRate( 0 )
    , m_paDeviceIndex( -1 )
    , m_paStream( nullptr )
    , m_mixerBuffers( nullptr )
    , m_mute( false )
    , m_mixerInterface( nullptr )
{
    m_mixThreadCommandsIssued = 0;
    m_mixThreadCommandsComplete = 0;
}

// ---------------------------------------------------------------------------------------------------------------------
Audio::~Audio()
{
}

// ---------------------------------------------------------------------------------------------------------------------
bool Audio::create( const app::Core& appCore )
{
    // bring up PA, check we have things to play with
    const auto paErrorInit = Pa_Initialize();
    if ( paErrorInit != paNoError )
    {
        blog::error::core( "PortAudio failed to initalise, {}", Pa_GetErrorText( paErrorInit ) );
        return false;
    }
    const auto paDeviceCount = Pa_GetDeviceCount();
    if ( paDeviceCount <= 0 )
    {
        blog::error::core( "PortAudio was unable to iterate or find any audio devices" );
        return false;
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

    
    return true;
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
}

// ---------------------------------------------------------------------------------------------------------------------
bool Audio::initOutput( const config::Audio& outputDevice )
{
    blog::core( "Establishing audio output using [{}] @ {}", outputDevice.lastDevice, outputDevice.sampleRate );

    PaStreamParameters outputParameters;
    if ( AudioDeviceQuery::generateStreamParametersFromDeviceConfig( outputDevice, outputParameters ) < 0 )
    {
        blog::error::core( "Unable to resolve stream parameters" );
        return false;
    }

    PaError err = Pa_OpenStream(
                        &m_paStream,
                        nullptr,
                        &outputParameters,
                        outputDevice.sampleRate,
                        0,
                        paNoFlag,
                        PortAudioCallback,
                        (void *)this );

    if ( err != paNoError )
    {
        blog::error::core( "Pa_OpenStream failed [{}]", Pa_GetErrorText( err ) );
        return false;
    }

    m_sampleRate   = outputDevice.sampleRate;
    m_mixerBuffers = new OutputBuffer( getMaximumBufferSize() );

    err = Pa_StartStream( m_paStream );
    if ( err != paNoError )
    {
        termOutput();

        blog::error::core( "Pa_StartStream failed [{}]", Pa_GetErrorText( err ) );
        return false;
    }

    return true;
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
#if OURO_FEATURES_VST
                m_vstiStack.emplace_back( mixCmdData.getPtrAs<vst::Instance>() );
#endif // OURO_FEATURES_VST
                break;
            case MixThreadCommand::ClearAllVSTs:
#if OURO_FEATURES_VST
                m_vstiStack.clear();
#endif // OURO_FEATURES_VST
                break;
            case MixThreadCommand::ToggleVSTBypass:
#if OURO_FEATURES_VST
                m_vstBypass = !m_vstBypass;
#endif // OURO_FEATURES_VST
                break;
            case MixThreadCommand::ToggleMute:
                m_mute = !m_mute;
                break;
            case MixThreadCommand::InstallSampleProcessor:
                m_sampleProcessorsInstalled[commandI64] = std::move( m_sampleProcessorsToInstall[commandI64] );
                break;
            case MixThreadCommand::ClearSampleProcessor:
                m_sampleProcessorsInstalled[commandI64].reset();
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
        m_mixerInterface->update( *m_mixerBuffers, m_volume, framesPerBuffer, m_state.m_samplePos );
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

#if OURO_FEATURES_VST
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
#endif // OURO_FEATURES_VST

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

    {
        // #hdd not like this is slow but could probably be ISPCd
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
        if ( ssp )
            ssp->appendSamples( resultChannelLeft, resultChannelRight, framesPerBuffer );
    }

    m_state.mark( ExposedState::ExecutionStage::RecordToDisk );

    // mute only the final output if requested - preserve all the processed work and WAV-output
    if ( m_mute )
        memset( outputBuffer, 0, sizeof( float ) * framesPerBuffer * 2 );

    return 0;
}

// ---------------------------------------------------------------------------------------------------------------------
bool Audio::beginRecording( const std::string& outputPath, const std::string& filePrefix )
{
    auto wavFilename = fs::absolute( fs::path( outputPath ) / fmt::format( "{}_finalmix.flac", filePrefix ) ).string();
    blog::core( "Opening FLAC final-mix output '{}'", wavFilename );

    auto newRecorder = ssp::FLACWriter::Create( wavFilename, getSampleRate(), 5 );
    if ( newRecorder )
    {
        m_sampleProcessorsToInstall[0] = std::move( newRecorder );

        m_mixThreadCommandQueue.emplace( MixThreadCommand::InstallSampleProcessor, 0 );
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------------------------------------------------
void Audio::stopRecording()
{
    m_mixThreadCommandQueue.emplace( MixThreadCommand::ClearSampleProcessor, 0 );
}

// ---------------------------------------------------------------------------------------------------------------------
bool Audio::isRecording() const
{
    return m_sampleProcessorsInstalled[0] != nullptr;
}

// ---------------------------------------------------------------------------------------------------------------------
uint64_t Audio::getRecordingDataUsage() const
{
    if ( isRecording() )
        return m_sampleProcessorsInstalled[0]->getStorageUsageInBytes();

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
#if OURO_FEATURES_VST
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
AsyncCommandCounter Audio::installAuxSampleProcessor( ssp::SampleStreamProcessorInstance&& sspInstance )
{
    const uint32_t commandCounter = m_mixThreadCommandsIssued++;
    m_sampleProcessorsToInstall[1] = std::move( sspInstance );
    m_mixThreadCommandQueue.emplace( MixThreadCommand::InstallSampleProcessor, 1 );
    return AsyncCommandCounter{ commandCounter };
}

// ---------------------------------------------------------------------------------------------------------------------
AsyncCommandCounter Audio::clearAuxSampleProcessor()
{
    const uint32_t commandCounter = m_mixThreadCommandsIssued++;
    m_mixThreadCommandQueue.emplace( MixThreadCommand::ClearSampleProcessor, 1 );
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
