//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#pragma once

#include "dsp/fft.util.h"
#include "dsp/octave.h"
#include "config/spectrum.h"

#include <q/synth/hann_gen.hpp>

// pffft
struct PFFFT_Setup;

namespace dsp {

// ---------------------------------------------------------------------------------------------------------------------
// the 8-bucket fft scope accepts a continual stream of samples; once it has enough, it extracts frequency buckets
// for visualisation elsewhere
//
struct Scope8
{
    // 8 buckets chosen as standard here as we end up encoding them into the stem data texture for the visualiser
    constexpr static std::size_t    FrequencyBucketCount = 8;

    using Result = std::array< float, FrequencyBucketCount >;

    Scope8( const float measurementLengthSeconds, uint32_t sampleRate, const config::Spectrum& config );
    ~Scope8();

    // get/set a new batch of configuration values
    inline const config::Spectrum& getConfiguration() const { return m_config; }
    void setConfiguration( const config::Spectrum& config ) { m_config = config; }

    // add sampleCount number of samples from left/right buffers given, potentially running the extraction if the
    // FFT block is filled
    void append( const float* samplesLeft, const float* samplesRight, uint32_t sampleCount );

    // fetch a copy of the current analysis
    inline Result getCurrentResult() const
    { 
        const std::size_t current = m_outputBucketsIndex.load();
        return m_outputBuckets.at(current);
    }


private:

    using FFTOctaves     = dsp::FFTOctaveBuckets< 8 >;
    using ResultBuffers  = std::array< Result, 2 >;
    using ResultIndex    = std::atomic< std::size_t >;
    using HannGen        = cycfi::q::hann_gen;


    config::Spectrum    m_config;

    uint32_t            m_sampleRate        = 0;
    uint32_t            m_fftWindowSize     = 0;        // size of the FFT input/output block
    uint32_t            m_frequencyBinSize  = 0;        // size of the active frequency bins in the output ( half the fft window )

    PFFFT_Setup*        m_pffftPlan         = nullptr;

    uint32_t            m_inputWriteIndex   = 0;        // point we're actively writing into the input buffers
    float*              m_inputL            = nullptr;  // accumulation buffer building FFT input data
    float*              m_inputR            = nullptr;
    complexf*           m_outputL           = nullptr;  // FFT output stages
    complexf*           m_outputR           = nullptr;


    // double-buffered outputs to help avoid other bits of the tool fetching buffers while they are being modified
    ResultIndex         m_outputBucketsIndex;
    ResultBuffers       m_outputBuckets;

    FFTOctaves          m_octaves;

    HannGen             m_hannGenerator;
};

} // namespace dsp
