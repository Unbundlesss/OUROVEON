//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

// pffft
struct PFFFT_Setup;

namespace dsp {

// ---------------------------------------------------------------------------------------------------------------------
// the fft scope accepts a continual stream of samples; once it has enough, it extracts frequency buckets
// for visualisation elsewhere
//
struct Scope
{
    constexpr static std::size_t FFTWindow          = 2048;                                 // FFT sample chunk size
    constexpr static int32_t     FFTBaseBuckets     = 16;                                   // initial set of frequency buckets
    constexpr static float       FFTBaseBucketsRcp  = 1.0f / (float)(FFTBaseBuckets);
    constexpr static int32_t     FFTFinalBuckets    = FFTBaseBuckets >> 1;                  // final number of buckets reported
    constexpr static float       FFTFinalBucketsRcp = 1.0f / (float)(FFTFinalBuckets);
    constexpr static float       FFTWindowRcp       = 1.0f / (float)(FFTWindow);

    using Result = std::array< float, FFTFinalBuckets >;

    Scope();
    ~Scope();

    // add sampleCount number of samples from left/right buffers given, potentially running the extraction if the
    // FFT block is filled
    void append( const float* samplesLeft, const float* samplesRight, std::size_t sampleCount );

    // fetch the current analysis
    constexpr const Result& getCurrentResult() const { return m_bucketsFinal; }

private:
    
    using BaseBuckets = std::array< float, FFTBaseBuckets >;

    // same as muFFT
    struct complexf
    {
        complexf( const float real, const float imag )
            : m_real( real )
            , m_imag( imag )
        {}

        float m_real;
        float m_imag;
    };

    PFFFT_Setup*    m_pffftPlan     = nullptr;

    complexf*       m_input         = nullptr;
    complexf*       m_output        = nullptr;
    std::size_t     m_writeIndex    = 0;

    BaseBuckets     m_bucketsBase;
    Result          m_bucketsFinal;
};

} // namespace dsp
