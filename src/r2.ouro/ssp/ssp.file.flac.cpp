//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#include "pch.h"

#include "ssp/ssp.file.flac.h"
#include "ssp/async.processor.h"

#include "base/construction.h"
#include "base/utils.h"
#include "base/instrumentation.h"

#include "spacetime/moment.h"

#include "buffer/mix.h"

#include "FLAC++/encoder.h"


namespace ssp {

// represents the live FLAC output stream with our own pre-buffering / float-to-int conversion stage infront
struct FLACWriter::StreamInstance : public FLAC::Encoder::File,
                                    public AsyncBufferProcessorIQ24
{
    // FLAC::Encoder::File
    virtual void progress_callback(
        FLAC__uint64 bytes_written,
        FLAC__uint64 samples_written,
        uint32_t frames_written,
        uint32_t total_frames_estimate ) override
    {
        m_flacFileBytesWritten = bytes_written;
    }

    StreamInstance( const uint32_t bufferSizeInSamples )
        : FLAC::Encoder::File()
        , AsyncBufferProcessorIQ24( bufferSizeInSamples, "FLAC" )
    {
        launchProcessorThread();
    }

    ~StreamInstance() override
    {
        // slam the breaks on the thread, allowing any active processing to finish
        terminateProcessorThread();

        // if the active page has straggling samples, append those too before we finalise
        auto* activeBuffer = getActiveBuffer();
        if ( activeBuffer &&
             activeBuffer->m_currentSamples > 0 )
        {
            ABSL_ASSERT( activeBuffer->m_committed == false ); // the buffer should not have been marked as committed
                                                               // otherwise that means it has already been through processBufferedSamplesFromThread
            m_commitsBeforeFlush = 0;

            // finalise the remainders and write it out
            activeBuffer->quantise();
            processBufferedSamplesFromThread( *activeBuffer );
        }

        // close stream, finish FLAC
        if ( is_valid() )
            finish();

        if ( m_flacFileHandle != nullptr )
        {
            fclose( m_flacFileHandle );
            m_flacFileHandle = nullptr;
        }
    }

    // buffer will already be quantised ready for reading
    void processBufferedSamplesFromThread( const base::IQ24Buffer& buffer ) override
    {
        // push data to FLAC processing
        bool flacOk = process_interleaved( buffer.m_interleavedQuant, buffer.m_currentSamples );
        if ( !flacOk )
        {
            blog::error::core( "FLAC processing failed on buffer commit!" );
            // todo ; probably abort this file at this point
        }

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

    // called from audio thread
    void appendStereo( float* buffer0, float* buffer1, const uint32_t sampleCount )
    {
        appendStereoSamples( buffer0, buffer1, sampleCount );
    }


    FILE*                   m_flacFileHandle            = nullptr;
    FLAC__uint64            m_flacFileBytesWritten      = 0;            // updated in overridden component of the stream encoder (post process_interleaved)

    uint32_t                m_commitsBeforeFlush        = 0;
};

// ---------------------------------------------------------------------------------------------------------------------
std::shared_ptr<FLACWriter> FLACWriter::Create(
    const fs::path&     outputFile,
    const uint32_t      sampleRate,
    const float         writeBufferInSeconds )
{
    // produce a 8 and 16-bit encoded version of the filename, supporting utf8 characters in the input
    const std::u16string outputFileU16 = outputFile.u16string();
    const std::string outputFileU8 = utf8::utf16to8( outputFileU16 );

    const uint32_t writeBufferInSamples = (uint32_t)std::ceil( (float)sampleRate * std::max( 0.25f, writeBufferInSeconds ) );

    std::unique_ptr< FLACWriter::StreamInstance > newState = std::make_unique< FLACWriter::StreamInstance >( writeBufferInSamples );

    bool flacConfig = true;
    flacConfig &= newState->set_verify( true );
    flacConfig &= newState->set_compression_level( 4 );
    flacConfig &= newState->set_channels( 2 );
    flacConfig &= newState->set_bits_per_sample( 24 );
    flacConfig &= newState->set_sample_rate( sampleRate );
    if ( !flacConfig )
    {
        blog::error::core( "FLAC failed to configure encoder for [{}]", outputFileU8 );
        return nullptr;
    }

    // open a FILE* to pass to the encoder
#if OURO_PLATFORM_WIN
    // wchar_t is 2 bytes on Windows and expects utf16, so pass it that
    FILE* fpFLAC = _wfopen( reinterpret_cast<const wchar_t*>(outputFileU16.c_str()), L"w+b" );
#else
    // elsewhere ... the OS is hopefully on board by default, just pass the utf8-capable 8-bit string
    // https://stackoverflow.com/questions/396567/is-there-a-standard-way-to-do-an-fopen-with-a-unicode-string-file-path
    FILE* fpFLAC = fopen( outputFileU8.c_str(), "w+b" );
#endif

    newState->m_flacFileHandle = fpFLAC;
    if ( newState->m_flacFileHandle == nullptr )
    {
        blog::error::core( "FLAC could not open [{}] for writing ({})\n", outputFileU8, std::strerror(errno) );
        return nullptr;
    }

    FLAC__StreamEncoderInitStatus flacInit = newState->init( newState->m_flacFileHandle );
    if ( flacInit != FLAC__STREAM_ENCODER_INIT_STATUS_OK ) 
    {
        blog::error::core( "FLAC unable to begin stream ({}) for file [{}]", FLAC__StreamEncoderInitStatusString[flacInit], outputFileU8 );
        return nullptr;
    }

    return base::protected_make_shared<FLACWriter>( ISampleStreamProcessor::allocateNewInstanceID(), newState );
}

// ---------------------------------------------------------------------------------------------------------------------
void FLACWriter::appendSamples( float* buffer0, float* buffer1, const uint32_t sampleCount )
{
    m_state->appendStereo( buffer0, buffer1, sampleCount );
}

// ---------------------------------------------------------------------------------------------------------------------
uint64_t FLACWriter::getStorageUsageInBytes() const
{
    return m_state->m_flacFileBytesWritten;
}

// ---------------------------------------------------------------------------------------------------------------------
FLACWriter::FLACWriter( const StreamProcessorInstanceID instanceID, std::unique_ptr< StreamInstance >& state )
    : ISampleStreamProcessor( instanceID )
    , m_state( std::move( state ) )
{
}

// ---------------------------------------------------------------------------------------------------------------------
FLACWriter::~FLACWriter()
{
    m_state.reset();
}

} // namespace ssp
