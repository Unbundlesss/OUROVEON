//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once
#include "base/construction.h"
#include "isamplestreamprocessor.h"

namespace ssp {

// ---------------------------------------------------------------------------------------------------------------------
// 
class FLACWriter : public ISampleStreamProcessor
{
public:
    DECLARE_NO_COPY( FLACWriter );

    ~FLACWriter();

    static std::shared_ptr<FLACWriter> Create(
        const fs::path&     outputFile,
        const uint32_t      sampleRate,
        const float         writeBufferInSeconds );

    void appendSamples( float* buffer0, float* buffer1, const uint32_t sampleCount ) override;
    uint64_t getStorageUsageInBytes() const override;

private:

    struct StreamInstance;
    std::unique_ptr< StreamInstance >  m_state;

protected:

    FLACWriter( const StreamProcessorInstanceID instanceID, std::unique_ptr< StreamInstance >& state );
};

} // namespace ssp
