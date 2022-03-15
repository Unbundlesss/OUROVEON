//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once
#include "base/macro.h"
#include "base/instrumentation.h"

#include "isamplestreamprocessor.h"

namespace ssp {

// ---------------------------------------------------------------------------------------------------------------------
struct OpusPacketData
{
    OpusPacketData( const uint32_t packetCount );
    ~OpusPacketData();

    const size_t            m_opusDataBufferSize;
    uint8_t*                m_opusData              = nullptr;
    std::vector<uint16_t>   m_opusPacketSizes;

    uint32_t                m_averagePacketSize     = 0;

    size_t                  m_dispatchedPackets     = 0;
    size_t                  m_dispatchedSize        = 0;
};

using OpusPacketDataInstance = std::unique_ptr< OpusPacketData >;


// ---------------------------------------------------------------------------------------------------------------------
// 
class OpusStream : public ISampleStreamProcessor
{
public:
    DeclareUncopyable( OpusStream );

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

    // should make this configurable later
    static constexpr uint32_t   cFrameSize      = 2880;
    static constexpr uint32_t   cBufferedFrames = 25;
    static constexpr float      cFrameTimeSec   = ( 1.0f / 48000.0f ) * (float)cFrameSize;

    ~OpusStream();

    using NewDataCallback = std::function< void( OpusPacketDataInstance&& ) >;


    static std::shared_ptr<OpusStream> Create(
        const NewDataCallback&  newDataCallback,
        const uint32_t          sampleRate );

    void appendSamples( float* buffer0, float* buffer1, const uint32_t sampleCount ) override;
    uint64_t getStorageUsageInBytes() const override;


    struct CompressionSetup
    {
        int32_t     m_bitrate                   = 0;
        int32_t     m_expectedPacketLossPercent = 0;
    };
    CompressionSetup getCurrentCompressionSetup() const;
    void setCompressionSetup( const CompressionSetup& setup );


private:

    struct StreamInstance;
    std::unique_ptr< StreamInstance >  m_state;

protected:

    OpusStream( const StreamProcessorInstanceID instanceID, std::unique_ptr< StreamInstance >& state );
};

} // namespace ssp