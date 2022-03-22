//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "base/id.simple.h"

namespace ssp {

// each SSP gets a unique ID for trivially referring to an instance
struct _stream_processor_id {};
using StreamProcessorInstanceID = base::id::Simple<_stream_processor_id, uint32_t, 1, 0>;

// ---------------------------------------------------------------------------------------------------------------------
struct ISampleStreamProcessor
{
    ISampleStreamProcessor( const StreamProcessorInstanceID instanceID )
        : m_streamProcessorInstanceID( instanceID )
    {}
    virtual ~ISampleStreamProcessor() {}

    // called by audio output source to pass samples to process
    virtual void appendSamples( float* buffer0, float* buffer1, const uint32_t sampleCount ) = 0;
    
    // for UI feedback; general 'data storage' estimate, could be bytes on disk, could be memory usage, could be both
    virtual uint64_t getStorageUsageInBytes() const = 0;


protected:
    StreamProcessorInstanceID m_streamProcessorInstanceID;
public:
    constexpr StreamProcessorInstanceID getInstanceID() const { return m_streamProcessorInstanceID; }

    // get new unique ID for instantiating a processor
    static StreamProcessorInstanceID allocateNewInstanceID();
};

using SampleStreamProcessorInstance = std::shared_ptr<ISampleStreamProcessor>;

} // namespace ssp
