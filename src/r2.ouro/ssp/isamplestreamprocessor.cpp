//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#include "pch.h"

#include "ssp/isamplestreamprocessor.h"

namespace ssp {

ssp::StreamProcessorInstanceID ISampleStreamProcessor::allocateNewInstanceID()
{
    static std::atomic_uint32_t cCounter = 1;

    const auto newID = cCounter++;
    return StreamProcessorInstanceID( newID );
}

} // namespace ssp
