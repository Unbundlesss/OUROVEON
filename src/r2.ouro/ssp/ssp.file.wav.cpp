//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"

#include "ssp/ssp.file.wav.h"

#include "base/construction.h"

namespace ssp {

// ---------------------------------------------------------------------------------------------------------------------
namespace data {

struct WavHeader
{
    static constexpr uint32_t bytesPerSample = sizeof( uint32_t );
    static constexpr uint32_t bitsPerSample = bytesPerSample * 8;

    WavHeader( const uint32_t totalSampleCount, const uint32_t sampleRate )
    {
        #define	MAKE_MARKER(a, b, c, d)     ((uint32_t) ((a) | ((b) << 8) | ((c) << 16) | (((uint32_t) (d)) << 24)))

        m_chunkID       = MAKE_MARKER( 'R', 'I', 'F', 'F' );
        m_format        = MAKE_MARKER( 'W', 'A', 'V', 'E' );

        m_subChunk1ID   = MAKE_MARKER( 'f', 'm', 't', ' ' );
        m_subChunk1Size = 16;
        m_audioFormat   = 3;                                // WAVE_FORMAT_IEEE_FLOAT
        m_numChannels   = 2;
        m_sampleRate    = sampleRate;
        m_byteRate      = m_sampleRate * m_numChannels * bytesPerSample;
        m_blockAlign    = m_numChannels * bytesPerSample;
        m_bitsPerSample = bitsPerSample;

        m_subChunk2ID   = MAKE_MARKER( 'd', 'a', 't', 'a' );
        m_subChunk2Size = totalSampleCount * m_numChannels * bytesPerSample;

        m_chunkSize     = 36 + m_subChunk2Size; // http://soundfile.sapp.org/doc/WaveFormat/

        #undef MAKE_MARKER
    }

    // the main chunk
    uint32_t      m_chunkID;
    uint32_t      m_chunkSize;
    uint32_t      m_format;

    // sub chunk 1 "fmt "
    uint32_t      m_subChunk1ID;
    uint32_t      m_subChunk1Size;
    uint16_t      m_audioFormat;
    uint16_t      m_numChannels;
    uint32_t      m_sampleRate;
    uint32_t      m_byteRate;
    uint16_t      m_blockAlign;
    uint16_t      m_bitsPerSample;

    // sub chunk 2 "data"
    uint32_t      m_subChunk2ID;
    uint32_t      m_subChunk2Size;
};

} // namespace data

// ---------------------------------------------------------------------------------------------------------------------
std::shared_ptr<WAVWriter> WAVWriter::Create(
    const fs::path& outputFile,
    const uint32_t sampleRate,
    const uint32_t writeBufferInSeconds /* = 10 */ )
{
    // produce a 8 and 16-bit encoded version of the filename, supporting utf8 characters in the input
    const std::u16string outputFileU16 = outputFile.u16string();
    const std::string outputFileU8 = utf8::utf16to8( outputFileU16 );

#if OURO_PLATFORM_WIN
    // wchar_t is 2 bytes on Windows and expects utf16, so pass it that
    FILE* fpWAV = _wfopen( reinterpret_cast<const wchar_t*>( outputFileU16.c_str() ), L"wb" );
#else
    // elsewhere ... the OS is hopefully on board by default, just pass the utf8-capable 8-bit string
    // https://stackoverflow.com/questions/396567/is-there-a-standard-way-to-do-an-fopen-with-a-unicode-string-file-path
    FILE* fpWAV = fopen( outputFileU8.c_str(), "wb" );
#endif

    if ( fpWAV == nullptr )
    {
        blog::error::core( "rec::WavFile could not open [{}] for writing ({})", outputFileU8, strerror(errno));
        return nullptr;
    }

    // write default header with 0 samples declared
    data::WavHeader nullHeader( 0, sampleRate );
    fwrite( &nullHeader, sizeof( data::WavHeader ), 1, fpWAV );

#if OURO_PLATFORM_WIN
    SYSTEM_INFO sinf;
    ::GetSystemInfo( &sinf );
    const uint32_t bufferChunkSize = (uint32_t)sinf.dwAllocationGranularity;
#else
    const uint32_t bufferChunkSize = 4096;
#endif

    // work out a buffer big enough for N seconds of storage, rounded to allocation granularity
    const uint32_t requestedBufferSampleCount    = sampleRate * 2 /* stereo */ * writeBufferInSeconds;
    const uint32_t bufferChunksToAllocate        = 1 + ( requestedBufferSampleCount / bufferChunkSize);

    return base::protected_make_shared<WAVWriter>(
        ISampleStreamProcessor::allocateNewInstanceID(),
        fpWAV,
        bufferChunkSize * bufferChunksToAllocate,
        sampleRate );
}

// ---------------------------------------------------------------------------------------------------------------------
void WAVWriter::appendSamples( float* buffer0, float* buffer1, const uint32_t sampleCount )
{
    for ( uint32_t i = 0; i < sampleCount; i++ )
    {
        m_writeBuffer.push_back( buffer0[i] );
        m_writeBuffer.push_back( buffer1[i] );
    }

    m_totalSamples += sampleCount;

    if ( m_writeBuffer.size() >= m_writeBufferChunkSize )
        CommitWriteBuffers();
}

// ---------------------------------------------------------------------------------------------------------------------
WAVWriter::WAVWriter( const StreamProcessorInstanceID instanceID, FILE* fp, const size_t writeBufferChunkSize, const uint32_t sampleRate )
    : ISampleStreamProcessor( instanceID )
    , m_writeBufferChunkSize( writeBufferChunkSize )
    , m_fp( fp )
    , m_totalSamples( 0 )
    , m_sampleRate( sampleRate )
{
    m_writeBuffer.reserve( m_writeBufferChunkSize );
}

// ---------------------------------------------------------------------------------------------------------------------
void WAVWriter::CommitWriteBuffers()
{
    // append new samples
    fwrite( m_writeBuffer.data(), sizeof( float ), m_writeBuffer.size(), m_fp );

    // update the header
    fseek( m_fp, 0, SEEK_SET );
    data::WavHeader wavHeader( m_totalSamples, m_sampleRate );
    fwrite( &wavHeader, sizeof( data::WavHeader ), 1, m_fp );
    fseek( m_fp, 0, SEEK_END );

    // reset buffers
    m_writeBuffer.clear();
}

// ---------------------------------------------------------------------------------------------------------------------
WAVWriter::~WAVWriter()
{
    CommitWriteBuffers();

    fclose( m_fp );
    m_fp = nullptr;
}

} // namespace ssp
