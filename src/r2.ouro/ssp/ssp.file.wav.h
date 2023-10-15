//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#pragma once
#include "base/construction.h"
#include "isamplestreamprocessor.h"

namespace ssp {

// ---------------------------------------------------------------------------------------------------------------------
// fairly simple limitedly-buffered WAV file writer; emits 32b floating point stereo streams so RIP to your free drive space
//
class WAVWriter : public ISampleStreamProcessor
{
public:
    DECLARE_NO_COPY( WAVWriter );

    ~WAVWriter();

    static std::shared_ptr<WAVWriter> Create(
        const fs::path& outputFile,
        const uint32_t  sampleRate,
        const uint32_t  writeBufferInSeconds = 10 );

    void appendSamples( float* buffer0, float* buffer1, const uint32_t sampleCount ) override;

    uint64_t getStorageUsageInBytes() const override
    {
        return ( m_totalSamples * sizeof( float ) * 2 );
    }

private:

    // empty current cache to disk, update file header
    void CommitWriteBuffers();

    std::vector< float >        m_writeBuffer;              // #TBD just use a plain buffer, no need for a slow vector here
    size_t                      m_writeBufferChunkSize;

    FILE*                       m_fp;
    uint32_t                    m_totalSamples;

    uint32_t                    m_sampleRate;

protected:

    WAVWriter( const StreamProcessorInstanceID instanceID, FILE* fp, const size_t writeBufferChunkSize, const uint32_t sampleRate );
};

} // namespace ssp
