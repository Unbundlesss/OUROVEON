//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

namespace ssp {

#define DEFINE_SSP(_classname)                              \
    _classname()                                = delete;   \
    _classname( const _classname& rhs )         = delete;   \
    _classname& operator=( const _classname& )  = delete;   \
    _classname( _classname&& )                  = default;  \
    _classname& operator=( _classname&& )       = default;

struct ISampleStreamProcessor
{
    virtual ~ISampleStreamProcessor() {}

    // called by audio output source to pass samples to process
    virtual void appendSamples( float* buffer0, float* buffer1, const uint32_t sampleCount ) = 0;
    
    // for UI feedback; general 'data storage' estimate, could be bytes on disk, could be memory usage, could be both
    virtual uint64_t getStorageUsageInBytes() const = 0;
};

using SampleStreamProcessorInstance = std::unique_ptr<ISampleStreamProcessor>;

} // namespace ssp {
