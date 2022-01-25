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
struct OpusPacketStream
{
    OpusPacketStream( const uint32_t packetCount );
    ~OpusPacketStream();

    const size_t            m_opusDataBufferSize;
    uint8_t*                m_opusData              = nullptr;
    std::vector<uint16_t>   m_opusPacketSizes;

    uint32_t                m_averagePacketSize     = 0;

    size_t                  m_dispatchedPackets     = 0;
    size_t                  m_dispatchedSize        = 0;
};

using OpusPacketStreamInstance = std::unique_ptr< OpusPacketStream >;


// ---------------------------------------------------------------------------------------------------------------------
// 
class PagedOpus : public ISampleStreamProcessor
{
public:
    DEFINE_SSP( PagedOpus )

    // permitted Opus frame sizes for 48 kHz sample rate
    enum class FrameSize
    {
        b120  = 120,
        b240  = 240,
        b480  = 480,
        b960  = 960,
        b1920 = 1920,
        b2880 = 2880,
    };

    ~PagedOpus();

    using NewPageCallback = std::function< void( OpusPacketStreamInstance&& ) >;


    static std::unique_ptr<PagedOpus> Create(
        const NewPageCallback&  newPageCallback,
        const uint32_t          sampleRate,
        const uint32_t          writeBufferInSeconds = 10 );

    void appendSamples( float* buffer0, float* buffer1, const uint32_t sampleCount ) override;
    uint64_t getStorageUsageInBytes() const override;


private:

    struct StreamInstance;
    std::unique_ptr< StreamInstance >  m_state;

    PagedOpus( std::unique_ptr< StreamInstance >& state );
};

} // namespace ssp