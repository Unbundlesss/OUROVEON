//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//

#include "base/utils.h"

#include "app/module.audio.h"
#include "rec/irecordable.h"

namespace mix {

// ---------------------------------------------------------------------------------------------------------------------
struct Slipstream : public app::module::MixerInterface,
                    public rec::IRecordable
{
    using AudioBuffer = app::module::Audio::OutputBuffer;

    // app::module::Audio::MixerInterface
    virtual void update(
        const AudioBuffer&  outputBuffer,
        const float         outputVolume,
        const uint32_t      samplesToWrite,
        const uint64_t      samplePosition ) override;

    // rec::IRecordable
    virtual bool beginRecording( const std::string& outputPath, const std::string& filePrefix ) override;
    virtual void stopRecording() override;
    virtual bool isRecording() const override;
    virtual uint64_t getRecordingDataUsage() const override;
    virtual const char* getRecorderName() const override;
    virtual const char* getFluxState() const override;
};

} // namespace mix