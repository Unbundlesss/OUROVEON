//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"

#include "ssp/ssp.stream.opus.h"

#include "base/construction.h"
#include "base/utils.h"
#include "base/macro.h"

#include <opus.h>


namespace ssp {

// ---------------------------------------------------------------------------------------------------------------------
inline const char* getOpusErrorString( const int32_t err )
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
    m_opusData = mem::malloc16AsSet< uint8_t >( m_opusDataBufferSize, 0 );
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
struct OpusStream::StreamInstance
{
    //using MixThreadCommandQueue = mcc::ReaderWriterQueue<MixThreadCommandData>;

    struct Page
    {
        Page( const uint32_t sampleCount )
            : m_sampleCount( sampleCount )
        {
            const auto totalStereoSamples = m_sampleCount * 2;

            m_interleavedFloat  = mem::malloc16As< float >( totalStereoSamples );
            m_interleavedI16    = mem::malloc16AsSet< int16_t >( totalStereoSamples, 0 );
        }

        ~Page()
        {
            if ( m_interleavedFloat != nullptr )
                mem::free16( m_interleavedFloat );
            m_interleavedFloat = nullptr;

            if ( m_interleavedI16 != nullptr )
                mem::free16( m_interleavedI16 );
            m_interleavedI16 = nullptr;
        }

        const uint32_t  m_sampleCount;
        uint32_t        m_currentSamples    = 0;
        float*          m_interleavedFloat  = nullptr;
        int16_t*        m_interleavedI16    = nullptr;
    };



    StreamInstance()
    {
        m_activePage  = new Page( OpusStream::cFrameSize * OpusStream::cBufferedFrames );
        m_reservePage = new Page( OpusStream::cFrameSize * OpusStream::cBufferedFrames );
    }

    ~StreamInstance()
    {
        // terminate compressor thread
        if ( m_compressorThread )
        {
            {
                std::unique_lock<std::mutex> lock( m_compressorMutex );
                m_compressorThreadRun = false;
            }
            m_compressorCVar.notify_one(); // unblock
            m_compressorThread->join();
        }

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

    bool initialiseEncoder( const uint32_t sampleRate )
    {
        int32_t opusError = 0;
        m_opusEncoder = opus_encoder_create( sampleRate, 2, OPUS_APPLICATION_AUDIO, &opusError );
        if ( opusError )
        {
            blog::error::core( "opus_encoder_create failed with error {} ({})", opusError, getOpusErrorString(opusError) );
            return false;
        }

        opus_encoder_ctl( m_opusEncoder, OPUS_SET_COMPLEXITY( 10 ) );
        opus_encoder_ctl( m_opusEncoder, OPUS_SET_SIGNAL( OPUS_SIGNAL_MUSIC ) );

        opus_encoder_ctl( m_opusEncoder, OPUS_SET_BITRATE( 96000 ) );

        opus_int32 rate;
        opus_encoder_ctl( m_opusEncoder, OPUS_GET_BITRATE( &rate ) );

        blog::core( "OPUS bitrate : {}", rate );

        m_opusRepacketizer = opus_repacketizer_create();

        // launch background compressor thread
        m_compressorThreadRun   = true;
        m_compressorThread      = std::make_unique<std::thread>( &StreamInstance::compressorThreadWorker, this );

        // compression handler is adjacent to the realtime audio thread, so give it a lil priority bump
        // #HDD TODO other platforms, probably with abseil
#if OURO_PLATFORM_WIN
        ::SetThreadPriority( m_compressorThread->native_handle(), THREAD_PRIORITY_ABOVE_NORMAL );
#endif // OURO_PLATFORM_WIN

        return true;
    }

    void appendStereo( float* buffer0, float* buffer1, const uint32_t sampleCount )
    {
        if ( m_activePage == nullptr )
            return;

        size_t   readOffset         = 0;
        uint32_t samplesRemaining   = sampleCount;
        uint32_t pageRemaining      = ( m_activePage->m_sampleCount - m_activePage->m_currentSamples );
        
        // if the remaining number of samples would overrun the buffer : 
        // first take all the samples we can, then flip buffers and trigger the background worker thread
        if ( samplesRemaining > pageRemaining )
        {
            float* currentFpPos = &m_activePage->m_interleavedFloat[m_activePage->m_currentSamples * 2];
            for ( size_t idxIn = 0, idxOut = 0; idxIn < pageRemaining; idxIn++, idxOut +=2 )
            {
                currentFpPos[idxOut + 0] = buffer0[idxIn];
                currentFpPos[idxOut + 1] = buffer1[idxIn];
            }

            m_activePage->m_currentSamples += pageRemaining;

            // offset the initial values for how many samples to read and where to read from for the remaining
            // copy code outside of this block
            samplesRemaining -= pageRemaining;
            readOffset += pageRemaining;


            // buffer complete - launch compressor stage on background thread and swap to other buffer to continue work
            {
                std::unique_lock<std::mutex> lock( m_compressorMutex );

                std::swap( m_activePage, m_reservePage );
                m_activePage->m_currentSamples = 0;
            }
            m_compressorCVar.notify_one();

        }

        float* currentFpPos = &m_activePage->m_interleavedFloat[m_activePage->m_currentSamples * 2];
        for ( size_t idxIn = 0, idxOut = 0; idxIn < samplesRemaining; idxIn++, idxOut +=2 )
        {
            currentFpPos[idxOut + 0] = buffer0[readOffset + idxIn];
            currentFpPos[idxOut + 1] = buffer1[readOffset + idxIn];
        }

        m_activePage->m_currentSamples += sampleCount;
    }


    static constexpr float fScaler16    = (float)0x7fffL;
    static constexpr int32_t fInt16Max  = (0x7fffL);
    static constexpr int32_t fInt16Min  = (-fInt16Max - 1);

    static constexpr size_t cEncodeBufferSize = 65536;
    uint8_t threadEncoderBuffer[cEncodeBufferSize] = { 0 };

    // background thread that takes the currently buffered page of samples and works on them
    void compressorThreadWorker()
    {
        base::instr::setThreadName( OURO_THREAD_PREFIX "OpusPageCompressor" );

        blog::core( "Opus compressor thread launched");

        for ( ;; )
        {
            std::unique_lock<std::mutex> lock( m_compressorMutex );
            m_compressorCVar.wait( lock );

            if ( !m_compressorThreadRun )
                break;

            base::instr::ScopedEvent se( "OPUS", "encoder-thread", base::instr::PresetColour::Orange );
            processBufferedSamples();

            lock.unlock();
        }

        blog::core( "Opus compressor thread closing" );
    }

    void processBufferedSamples()
    {
        for ( size_t i = 0; i < m_reservePage->m_currentSamples * 2; i++ )
        {
            m_reservePage->m_interleavedI16[i] = (int16_t)std::clamp( (int32_t)(m_reservePage->m_interleavedFloat[i] * fScaler16), fInt16Min, fInt16Max );
        }

        const uint32_t packetsInStream = m_reservePage->m_currentSamples / OpusStream::cFrameSize;
        OpusPacketData* newPacketStream = new OpusPacketData( packetsInStream );

        uint32_t totalPackets = 0;
        uint32_t totalPacketSizes = 0;

        int64_t remainingSamples = m_reservePage->m_currentSamples;
        
        float* fltInput = m_reservePage->m_interleavedFloat;
        opus_int16* pcmInput = (opus_int16*)(m_reservePage->m_interleavedI16);
        uint8_t* opusOut = newPacketStream->m_opusData;
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
                newPacketStream->m_opusPacketSizes.push_back( retval );

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
        assert( remainingSamples == 0 );


        newPacketStream->m_averagePacketSize = ( totalPacketSizes / totalPackets );

        m_newDataCallback( OpusPacketDataInstance( newPacketStream ) );
    }


    std::unique_ptr< std::thread >  m_compressorThread;
    std::atomic_bool                m_compressorThreadRun   = false;

    // inter-thread communication to signal compression jobs ready
    std::mutex                      m_compressorMutex;
    std::condition_variable         m_compressorCVar;


    Page*                           m_activePage            = nullptr;
    Page*                           m_reservePage           = nullptr;

    OpusEncoder*                    m_opusEncoder           = nullptr;
    OpusRepacketizer*               m_opusRepacketizer      = nullptr;


    NewDataCallback                 m_newDataCallback       = nullptr;
};

// ---------------------------------------------------------------------------------------------------------------------
std::shared_ptr<OpusStream> OpusStream::Create(
    const NewDataCallback&  newDataCallback,
    const uint32_t          sampleRate )
{
    std::unique_ptr< OpusStream::StreamInstance > newState = std::make_unique< OpusStream::StreamInstance >();

    if ( !newState->initialiseEncoder( sampleRate ) )
    {
        return nullptr;
    }

    newState->m_newDataCallback = newDataCallback;

    return base::protected_make_shared<OpusStream>( ISampleStreamProcessor::allocateNewInstanceID(), newState );
}

// ---------------------------------------------------------------------------------------------------------------------
void OpusStream::appendSamples( float* buffer0, float* buffer1, const uint32_t sampleCount )
{
    m_state->appendStereo( buffer0, buffer1, sampleCount );
}

// ---------------------------------------------------------------------------------------------------------------------
uint64_t OpusStream::getStorageUsageInBytes() const
{
    return OpusStream::cFrameSize * OpusStream::cBufferedFrames * 2;
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