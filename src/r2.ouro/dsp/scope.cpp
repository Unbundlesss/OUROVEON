//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"

#include "base/utils.h"

#include "dsp/scope.h"
#include "dsp/fft.h"

#include "pffft.h"

namespace dsp {

// ---------------------------------------------------------------------------------------------------------------------
Scope::Scope()
{
    m_pffftPlan = pffft_new_setup( FFTWindow, PFFFT_COMPLEX );

    m_input  = mem::alloc16<complexf>( FFTWindow );
    m_output = mem::alloc16<complexf>( FFTWindow );

    m_bucketsBase.fill( 0.0f );
    m_bucketsFinal.fill( 0.0f );
}

// ---------------------------------------------------------------------------------------------------------------------
Scope::~Scope()
{
    mem::free16( m_output );
    mem::free16( m_input );

    pffft_destroy_setup( m_pffftPlan );
}

// if enabled, can ingest multiple fft-blocks' worth of samples in a single append, potentially running fft execute multiple times
#define SCOPE_ALLOW_MULTI_FFT_PER_APPEND    0

// ---------------------------------------------------------------------------------------------------------------------
void Scope::append( const float* samplesLeft, const float* samplesRight, std::size_t sampleCount )
{
    ABSL_ASSERT( m_writeIndex <= FFTWindow );
    std::size_t samplesUntilProcess = FFTWindow - m_writeIndex;

#if SCOPE_ALLOW_MULTI_FFT_PER_APPEND
    for ( ;; )
#endif // SCOPE_ALLOW_MULTI_FFT_PER_APPEND
    {
        // copy in as many new samples as we can, up to the fill of the fft input buffer
        const std::size_t samplesToCopy = std::min( samplesUntilProcess, sampleCount );
        if ( samplesToCopy > 0 )
        {
            for ( std::size_t i = 0; i < samplesToCopy; i++ )
            {
                m_input[m_writeIndex + i] = complexf( samplesLeft[i], samplesRight[i] );
            }
            m_writeIndex += samplesToCopy;
            sampleCount  -= samplesToCopy;
        }

        // if we filled the buffer, execute the fft and extract the frequency data we want
        if ( m_writeIndex == FFTWindow )
        {
            pffft_transform_ordered( m_pffftPlan, (float*)m_input, (float*)m_output, nullptr, PFFFT_FORWARD );

            m_bucketsBase.fill( 0.0f );

            std::size_t bandI = 0;
            float       bandF = 0.0f;
            for ( ; bandI < FFTBaseBuckets; bandI++, bandF += FFTBaseBucketsRcp )
            {
                const auto skewedProportionX  = 1.0f - std::expf( std::logf( 1.0f - bandF ) * 0.2f );
                const auto fftDataIndex       = (int)( skewedProportionX * FFTWindow );


                const float outReal  = m_output[fftDataIndex].m_real;
                const float outImag  = m_output[fftDataIndex].m_imag;

                const float fftPower = ( outReal * outReal ) +
                                       ( outImag * outImag );

                const float fftMag   = std::sqrt( fftPower ) * FFTBaseBucketsRcp;

                m_bucketsBase[bandI] = std::clamp( fftMag, 0.0f, 1.0f );
            }

            // average down to the final bucket values
            for ( std::size_t bIdx = 0; bIdx < FFTFinalBuckets; bIdx++ )
            {
                const auto baseReadIndex = bIdx << 1;
                m_bucketsFinal[ bIdx ] = m_bucketsBase[ baseReadIndex + 0 ] +
                                         m_bucketsBase[ baseReadIndex + 1 ];

                m_bucketsFinal[bIdx]  *= 0.5f;
            }

            // reset the buffer write position ready for the next batch
            m_writeIndex = 0;
        }

#if SCOPE_ALLOW_MULTI_FFT_PER_APPEND
        if ( sampleCount == 0 )
            break;
#endif // SCOPE_ALLOW_MULTI_FFT_PER_APPEND
    }
}

} // namespace dsp
