//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//

#include "pch.h"

#include "mix/slipstream.h"

namespace mix {

void Slipstream::update(
    const AudioBuffer& outputBuffer,
    const float         outputVolume,
    const uint32_t      samplesToWrite,
    const uint64_t      samplePosition )
{

}

bool Slipstream::beginRecording( const std::string& outputPath, const std::string& filePrefix )
{
    return false;
}

void Slipstream::stopRecording()
{

}

bool Slipstream::isRecording() const
{
    return false;
}

uint64_t Slipstream::getRecordingDataUsage() const
{
    return 0;
}

const char* Slipstream::getRecorderName() const
{
    return "";
}

const char* Slipstream::getFluxState() const
{
    return "";
}

}