//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once
#include "isamplestreamprocessor.h"

namespace ssp {

// ---------------------------------------------------------------------------------------------------------------------
// 
class FLACWriter : public ISampleStreamProcessor
{
public:
    DEFINE_SSP( FLACWriter )

    ~FLACWriter();

    static std::unique_ptr<FLACWriter> Create(
        const std::string&  outputFile,
        const uint32_t      sampleRate,
        const uint32_t      writeBufferInSeconds = 10 );

    void appendSamples( float* buffer0, float* buffer1, const uint32_t sampleCount ) override;
    uint64_t getStorageUsageInBytes() const override;

private:

    struct StreamInstance;
    std::unique_ptr< StreamInstance >  m_state;

    uint32_t                        m_totalUncompressedSamples;
    
    FLACWriter( std::unique_ptr< StreamInstance >& state );
};

} // namespace ssp