//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#include "pch.h"

#include "ssp/ssp.stream.opus.h"
#include "ssp/async.processor.h"

#include "base/construction.h"
#include "base/utils.h"

#include <opus.h>


namespace ssp {

// ---------------------------------------------------------------------------------------------------------------------
ouro_nodiscard constexpr const char* getOpusErrorString( const int32_t err )
{
    switch ( err )
    {
        case OPUS_OK:               return "OK";
        case OPUS_BAD_ARG:          return "BAD_ARG";
        case OPUS_BUFFER_TOO_SMALL: return "BUFFER_TOO_SMALL";
        case OPUS_INTERNAL_ERROR:   return "INTERNAL_ERROR";
        case OPUS_INVALID_PACKET:   return "INVALID_PACKET";
        case OPUS_UNIMPLEMENTED:    return "UNIMPLEMENTED";
        case OPUS_INVALID_STATE:    return "INVALID_STATE";
        case OPUS_ALLOC_FAIL:       return "ALLOC_FAIL";
    }
    return "UNKNOWN";
}


// ---------------------------------------------------------------------------------------------------------------------
OpusPacketData::OpusPacketData( const uint32_t packetCount )
    : m_opusDataBufferSize( packetCount * OpusStream::cFrameSize )
{
    m_opusData = mem::alloc16To< uint8_t >( m_opusDataBufferSize, 0 );
    m_opusPacketSizes.reserve( packetCount );
}

OpusPacketData::~OpusPacketData()
{
    if ( m_opusData != nullptr )
        mem::free16( m_opusData );
    m_opusData = nullptr;
}


// ---------------------------------------------------------------------------------------------------------------------
// 
struct OpusStream::StreamInstance final : public AsyncBufferProcessorIQ16
{
    StreamInstance()
        : AsyncBufferProcessorIQ16( OpusStream::cFrameSize * OpusStream::cBufferedFrames, "OPUS" )
    {
        launchProcessorThread();
    }

    ~StreamInstance()
    {
        // stop processing thread
        terminateProcessorThread();

        if ( m_opusEncoder != nullptr )
        {
            opus_encoder_destroy( m_opusEncoder );
            m_opusEncoder = nullptr;
        }
        if ( m_opusRepacketizer != nullptr )
        {
            opus_repacketizer_destroy( m_opusRepacketizer );
            m_opusRepacketizer = nullptr;
        }
    }

    absl::Status initialiseEncoder( const uint32_t sampleRate )
    {
        int32_t opusError = 0;
        m_opusEncoder = opus_encoder_create( sampleRate, 2, OPUS_APPLICATION_AUDIO, &opusError );
        if ( opusError )
        {
            return absl::UnknownError( fmt::format(
                FMTX( "opus_encoder_create failed with error {} ({})" ), opusError, getOpusErrorString( opusError ) ) );
        }

        opus_encoder_ctl( m_opusEncoder, OPUS_SET_COMPLEXITY( 10 ) );
        opus_encoder_ctl( m_opusEncoder, OPUS_SET_SIGNAL( OPUS_SIGNAL_MUSIC ) );

        opus_encoder_ctl( m_opusEncoder, OPUS_SET_BITRATE( 64000 ) );
        opus_encoder_ctl( m_opusEncoder, OPUS_GET_BITRATE( &m_compressionSetup.m_bitrate ) );

        opus_encoder_ctl( m_opusEncoder, OPUS_SET_PACKET_LOSS_PERC( 0 ) );
        opus_encoder_ctl( m_opusEncoder, OPUS_GET_PACKET_LOSS_PERC( &m_compressionSetup.m_expectedPacketLossPercent ) );

        m_opusRepacketizer = opus_repacketizer_create();

        return absl::OkStatus();
    }


    static constexpr size_t cEncodeBufferSize = 65536;
    uint8_t threadEncoderBuffer[cEncodeBufferSize] = { 0 };

    void processBufferedSamplesFromThread( const base::IQ16Buffer& buffer ) override
    {
        const uint32_t packetsInStream = buffer.m_currentSamples / OpusStream::cFrameSize;
        auto packetDataInstance = std::make_unique< OpusPacketData >( packetsInStream );

        uint32_t totalPackets = 0;
        uint32_t totalPacketSizes = 0;

        int64_t remainingSamples = buffer.m_currentSamples;
        
        float* fltInput = buffer.m_interleavedFloat;
        opus_int16* pcmInput = (opus_int16*)(buffer.m_interleavedQuant);
        uint8_t* opusOut = packetDataInstance->m_opusData;

        memset( threadEncoderBuffer, 0, cEncodeBufferSize );

        for ( uint32_t pk = 0; pk < packetsInStream; pk ++ )
        {
            int ret = opus_encode( m_opusEncoder, pcmInput, OpusStream::cFrameSize, threadEncoderBuffer, cEncodeBufferSize );

            if ( ret > 0 )
            {
                m_opusRepacketizer = opus_repacketizer_init( m_opusRepacketizer );

                int retval = opus_repacketizer_cat( m_opusRepacketizer, threadEncoderBuffer, ret );
                if ( retval != OPUS_OK ) 
                {
                    blog::error::core( "opus_repacketizer_cat(): {}", opus_strerror( retval ) );
                    break;
                }

                retval = opus_repacketizer_out( m_opusRepacketizer, opusOut, cEncodeBufferSize );
                if ( retval < 0 )
                {
                    blog::error::core( "opus_repacketizer_cat(): {}", opus_strerror( retval ) );
                    break;
                }

                opusOut += retval;
                packetDataInstance->m_opusPacketSizes.push_back( retval );

                totalPackets++;
                totalPacketSizes += retval;
            }
            else
            {
                blog::error::core( "opus_encode(): {}", opus_strerror( ret ) );
                break;
            }

            pcmInput += OpusStream::cFrameSize * 2;
            fltInput += OpusStream::cFrameSize * 2;

            remainingSamples -= OpusStream::cFrameSize;
        }
        ABSL_ASSERT( remainingSamples == 0 );


        packetDataInstance->m_averagePacketSize = ( totalPacketSizes / totalPackets );

        m_newDataCallback( std::move( packetDataInstance ) );
    }

    CompressionSetup                m_compressionSetup;

    OpusEncoder*                    m_opusEncoder           = nullptr;
    OpusRepacketizer*               m_opusRepacketizer      = nullptr;


    NewDataCallback                 m_newDataCallback       = nullptr;
};

// ---------------------------------------------------------------------------------------------------------------------
OpusStream::PtrOrStatus OpusStream::Create(
    const NewDataCallback&  newDataCallback,
    const uint32_t          sampleRate )
{
    std::unique_ptr< OpusStream::StreamInstance > newState = std::make_unique< OpusStream::StreamInstance >();

    const auto encoderStatus = newState->initialiseEncoder( sampleRate );
    if ( !encoderStatus.ok() )
    {
        return encoderStatus;
    }

    newState->m_newDataCallback = newDataCallback;

    return PtrOrStatus( base::protected_make_shared<OpusStream>( ISampleStreamProcessor::allocateNewInstanceID(), newState ) );
}

// ---------------------------------------------------------------------------------------------------------------------
void OpusStream::appendSamples( float* buffer0, float* buffer1, const uint32_t sampleCount )
{
    m_state->appendStereoSamples( buffer0, buffer1, sampleCount );
}

// ---------------------------------------------------------------------------------------------------------------------
uint64_t OpusStream::getStorageUsageInBytes() const
{
    return OpusStream::cFrameSize * OpusStream::cBufferedFrames * 2;
}

// ---------------------------------------------------------------------------------------------------------------------
OpusStream::CompressionSetup OpusStream::getCurrentCompressionSetup() const
{
    return m_state->m_compressionSetup;
}

// ---------------------------------------------------------------------------------------------------------------------
void OpusStream::setCompressionSetup( const OpusStream::CompressionSetup& setup )
{
    if ( m_state->m_compressionSetup.m_bitrate != setup.m_bitrate )
    {
        opus_encoder_ctl( m_state->m_opusEncoder, OPUS_SET_BITRATE( setup.m_bitrate ) );
        blog::core( "OPUS_SET_BITRATE({})", setup.m_bitrate );
    }
    if ( m_state->m_compressionSetup.m_expectedPacketLossPercent != setup.m_expectedPacketLossPercent )
    {
        opus_encoder_ctl( m_state->m_opusEncoder, OPUS_SET_INBAND_FEC( setup.m_expectedPacketLossPercent > 0 ? 1 : 0 ) );
        opus_encoder_ctl( m_state->m_opusEncoder, OPUS_SET_PACKET_LOSS_PERC( setup.m_expectedPacketLossPercent ) );
        blog::core( "OPUS_SET_PACKET_LOSS_PERC({})", setup.m_expectedPacketLossPercent );
    }

    m_state->m_compressionSetup = setup;
}

// ---------------------------------------------------------------------------------------------------------------------
OpusStream::OpusStream( const StreamProcessorInstanceID instanceID, std::unique_ptr< StreamInstance >& state )
    : ISampleStreamProcessor( instanceID )
    , m_state( std::move( state ) )
{
}

// ---------------------------------------------------------------------------------------------------------------------
OpusStream::~OpusStream()
{
    m_state.reset();
}

} // namespace ssp
