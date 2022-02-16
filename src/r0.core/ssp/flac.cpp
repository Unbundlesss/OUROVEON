//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"

#include "ssp/flac.h"

#include "base/utils.h"
#include "buffer/mix.h"

#include "FLAC++/encoder.h"

namespace ssp {

// represents the live FLAC output stream with our own pre-buffering / float-to-int conversion stage infront
struct FLACWriter::StreamInstance : public FLAC::Encoder::File
{
    static constexpr float fScaler24 = (float)0x7fffffL;
    static constexpr int32_t fInt24Max = (  0x7fffffL );
    static constexpr int32_t fInt24Min = ( -fInt24Max - 1 );

    // FLAC::Encoder::File
    virtual void progress_callback(
        FLAC__uint64 bytes_written,
        FLAC__uint64 samples_written,
        uint32_t frames_written,
        uint32_t total_frames_estimate ) override
    {
        m_flacFileBytesWritten = bytes_written;
    }

    StreamInstance()
        : FLAC::Encoder::File()
    {
    }

    ~StreamInstance()
    {
        // finish up any stragglers
        commitBuffer();

        // toss buffer
        if ( m_sampleBuffer )
            mem::free16( m_sampleBuffer );
        m_sampleBuffer = nullptr;

        // close stream
        if ( is_valid() )
            finish();

        if ( m_flacFileHandle != nullptr )
        {
            fclose( m_flacFileHandle );
            m_flacFileHandle = nullptr;
        }
    }

    void appendStereo( float* buffer0, float* buffer1, const uint32_t sampleCount )
    {
        float* bufferLeft  = buffer0;
        float* bufferRight = buffer1;

        int64_t totalSamplesToAppend = sampleCount * 2;

        do 
        {
            int64_t sampleBufferFree = m_sampleBufferSize - m_sampleBufferUsed;
            assert( sampleBufferFree >= 0 );
            if ( sampleBufferFree == 0 )
            {
                commitBuffer();

                assert( m_sampleBufferUsed == 0 );
                sampleBufferFree = m_sampleBufferSize;
            }

            int32_t samplesCanBeWritten = (int32_t)std::min( sampleBufferFree, totalSamplesToAppend );

            FLAC__int32* bufferWritePoint = &m_sampleBuffer[m_sampleBufferUsed];

            auto samplesToWritePerChannel = samplesCanBeWritten / 2;
            buffer::interleave_float_to_int24( samplesToWritePerChannel, bufferLeft, bufferRight, bufferWritePoint );
            

            m_sampleBufferUsed += samplesCanBeWritten;
            totalSamplesToAppend -= samplesCanBeWritten;

            bufferLeft += samplesToWritePerChannel;
            bufferRight += samplesToWritePerChannel;

        } while ( totalSamplesToAppend > 0 );
    }

    void commitBuffer()
    {
        if ( m_sampleBufferUsed > 0 )
        {
            assert( m_sampleBuffer != nullptr );

            bool flacOk = process_interleaved( m_sampleBuffer, m_sampleBufferUsed / 2 );
            if ( !flacOk )
            {
                blog::error::core( "FLAC processing failed on buffer commit!" );
                // todo ; probably abort this file at this point
            }
            m_sampleBufferUsed = 0;

            if ( m_commitsBeforeFlush == 0 )
            {
                fflush( m_flacFileHandle );
                m_commitsBeforeFlush = 50;
            }
            else
            {
                m_commitsBeforeFlush--;
            }
        }
    }

    FILE*                   m_flacFileHandle            = nullptr;
    FLAC__uint64            m_flacFileBytesWritten      = 0;            // updated in overridden component of the stream encoder (post process_interleaved)

    FLAC__int32*            m_sampleBuffer              = nullptr;
    uint32_t                m_sampleBufferSize          = 0;            // max number of samples that can be written to m_sampleBuffer
    uint32_t                m_sampleBufferUsed          = 0;            // how many samples have been currently written

    uint32_t                m_commitsBeforeFlush        = 0;
};

// ---------------------------------------------------------------------------------------------------------------------
std::unique_ptr<FLACWriter> FLACWriter::Create(
    const std::string&  outputFile,
    const uint32_t      sampleRate,
    const uint32_t      writeBufferInSeconds )
{
    std::unique_ptr< FLACWriter::StreamInstance > newState = std::make_unique< FLACWriter::StreamInstance >();

    bool flacConfig = true;
    flacConfig &= newState->set_verify( true );
    flacConfig &= newState->set_compression_level( 2 );
    flacConfig &= newState->set_channels( 2 );
    flacConfig &= newState->set_bits_per_sample( 24 );
    flacConfig &= newState->set_sample_rate( sampleRate );
    if ( !flacConfig )
    {
        blog::error::core( "FLAC failed to configure encoder for [{}]", outputFile );
        return nullptr;
    }

    // open a FILE* to pass to the encoder
    newState->m_flacFileHandle = fopen( outputFile.c_str(), "w+b" );
    if ( newState->m_flacFileHandle == nullptr )
    {
        fmt::print( "FLAC could not open [{}] for writing ({})\n", outputFile, std::strerror(errno) );
        return nullptr;
    }

    FLAC__StreamEncoderInitStatus flacInit = newState->init( newState->m_flacFileHandle );
    if ( flacInit != FLAC__STREAM_ENCODER_INIT_STATUS_OK ) 
    {
        blog::error::core( "FLAC unable to begin stream ({}) for file [{}]", FLAC__StreamEncoderInitStatusString[flacInit], outputFile );
        return nullptr;
    }

    // work out a buffer big enough for N seconds of storage
    const uint32_t samplesToBufferForSecs = sampleRate * writeBufferInSeconds * 2 /* stereo */;
    const uint32_t samplesToBuffer        = ( 1 + (samplesToBufferForSecs / 4096) ) * 4096;

    newState->m_sampleBufferSize         = 4096 * 6;

    newState->m_sampleBuffer             = mem::malloc16AsSet< FLAC__int32 >( newState->m_sampleBufferSize, 0 );

    auto instance = new FLACWriter{ newState };
    return std::unique_ptr<FLACWriter>( instance );
}

// ---------------------------------------------------------------------------------------------------------------------
void FLACWriter::appendSamples( float* buffer0, float* buffer1, const uint32_t sampleCount )
{
    m_state->appendStereo( buffer0, buffer1, sampleCount );

    m_totalUncompressedSamples += sampleCount;
}

// ---------------------------------------------------------------------------------------------------------------------
uint64_t FLACWriter::getStorageUsageInBytes() const
{
    return m_state->m_flacFileBytesWritten;
}

// ---------------------------------------------------------------------------------------------------------------------
FLACWriter::FLACWriter( std::unique_ptr< StreamInstance >& state )
    : m_state( std::move( state ) )
    , m_totalUncompressedSamples( 0 )
{
}

// ---------------------------------------------------------------------------------------------------------------------
FLACWriter::~FLACWriter()
{
    m_state.reset();
}

} // namespace ssp