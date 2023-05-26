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
#include "base/mathematics.h"
#include "dsp/scope.h"
#include "dsp/octave.h"

// fft
#include "pffft.h"

using namespace cycfi::q::literals;

namespace dsp {

// ---------------------------------------------------------------------------------------------------------------------
Scope8::Scope8( const float measurementLengthSeconds, const uint32_t sampleRate, const config::Spectrum& config )
    : m_sampleRate( sampleRate )
    , m_hannGenerator( cycfi::q::duration(1.0), sampleRate ) // this has no default ctor, so this is a dummy, it gets rewrited elsewhere
{
    // stash default config
    setConfiguration( config );

    // work out an FFT size that is about the size required to sample at the requested measurement length
    const uint32_t samplesPerMeasurement = static_cast<uint32_t>( measurementLengthSeconds * static_cast<float>(m_sampleRate) );
    m_fftWindowSize = base::nextPow2( samplesPerMeasurement );

    blog::core( "Allocating FFT scope with {} samples", m_fftWindowSize );

    // create a pffft plan for the chosen size
    m_pffftPlan = pffft_new_setup( m_fftWindowSize, PFFFT_REAL );

    // configure hann window with chosen size
    m_hannGenerator.config( cycfi::q::duration( (double)m_fftWindowSize / (double)m_sampleRate ), m_sampleRate );


    // https://www.nti-audio.com/en/support/know-how/fast-fourier-transform-fft
    const uint32_t nyquist = sampleRate / 2; // theoretical maximum frequency that can be determined by the FFT
    const double nyquistRecp = 1.0 / static_cast<double>(nyquist);

    const uint32_t frequencyBinSize  = m_fftWindowSize / 2;
    const double frequencyBinSizeDbl = static_cast<double>(frequencyBinSize);
    const double frequencyResolution = static_cast<double>(m_sampleRate) / frequencyBinSizeDbl;


    // allocate all worker buffers, reset everything ready
    m_inputL        = mem::alloc16<float>( m_fftWindowSize );
    m_inputR        = mem::alloc16<float>( m_fftWindowSize );
    m_outputL       = mem::alloc16<complexf>( m_fftWindowSize );
    m_outputR       = mem::alloc16<complexf>( m_fftWindowSize );

    m_outputBucketsIndex = 0;
    m_outputBuckets[0].fill( 0.0f );
    m_outputBuckets[1].fill( 0.0f );

    m_bucketIndices = mem::alloc16<uint8_t>( m_fftWindowSize );

    m_countPerBucket.fill( 0 );

    // precompute frequency binning, splitting per octave 
    int previousSample = 0;
    uint8_t bucket = 0;
    for ( std::size_t octave = 3; octave <= 10; octave++, bucket++ )
    {
        const uint32_t midSample = std::min( (uint32_t)(std::ceil( cycfi::q::as_double( OctaveBands[octave].m_center ) * nyquistRecp * frequencyBinSizeDbl )), frequencyBinSize );
        const uint32_t endSample = std::min( (uint32_t)(std::ceil( cycfi::q::as_double( OctaveBands[octave].m_high )   * nyquistRecp * frequencyBinSizeDbl )), frequencyBinSize );

        for ( uint32_t s = previousSample; s < endSample; s++ )
        {
            m_bucketIndices[s] = bucket;
        }
        m_countPerBucket[bucket] = endSample - previousSample;

        // blog::core( "bin {} |  {} Hz |  {} | {}  ", octave, cycfi::q::as_float( OctaveBands[octave].m_high ), midSample, endSample );

        previousSample = endSample;
    }
    if ( previousSample > 0 )
    {
        // if the end bucket came in under the fft window size, just fill it in with the end bucket
        for ( uint32_t s = previousSample; s < m_fftWindowSize; s++ )
        {
            m_bucketIndices[s] = m_bucketIndices[previousSample - 1];
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
Scope8::~Scope8()
{
    mem::free16( m_bucketIndices );

    mem::free16( m_outputR );
    mem::free16( m_outputL );
    mem::free16( m_inputR );
    mem::free16( m_inputL );

    pffft_destroy_setup( m_pffftPlan );
}

// if enabled, can ingest multiple fft-blocks' worth of samples in a single append, potentially running fft execute multiple times
#define SCOPE_ALLOW_MULTI_FFT_PER_APPEND    0

// ---------------------------------------------------------------------------------------------------------------------
void Scope8::append( const float* samplesLeft, const float* samplesRight, uint32_t sampleCount )
{
    // emergency bail on overflow
    ABSL_ASSERT( m_inputWriteIndex <= m_fftWindowSize );
    if ( m_inputWriteIndex > m_fftWindowSize )
        return;

#if SCOPE_ALLOW_MULTI_FFT_PER_APPEND
    for ( ;; )
#endif // SCOPE_ALLOW_MULTI_FFT_PER_APPEND
    {
        const uint32_t samplesUntilProcess = m_fftWindowSize - m_inputWriteIndex;

        // copy in as many new samples as we can, up to the fill of the fft input buffer
        const uint32_t samplesToCopy = std::min( samplesUntilProcess, sampleCount );
        if ( samplesToCopy > 0 )
        {
            memcpy( &m_inputL[m_inputWriteIndex], samplesLeft,  samplesToCopy * sizeof( float ) );
            memcpy( &m_inputR[m_inputWriteIndex], samplesRight, samplesToCopy * sizeof( float ) );

            m_inputWriteIndex += samplesToCopy;
            sampleCount  -= samplesToCopy;
        }
        // if we filled the buffer, execute the fft and extract the frequency data we want
        if ( m_inputWriteIndex == m_fftWindowSize )
        {
            // optionally apply Hann window which reduces spectral leakage 
            // https://tinyurl.com/fft-windowing
            if ( m_config.applyHannWindow )
            {
                m_hannGenerator.reset();
                for ( std::size_t freqBin = 0; freqBin < m_fftWindowSize; freqBin++ )
                {
                    const float windowFactor = m_hannGenerator();
                    m_inputL[freqBin] *= windowFactor;
                    m_inputR[freqBin] *= windowFactor;
                }
            }

            pffft_transform_ordered( m_pffftPlan, m_inputL, reinterpret_cast<float*>(m_outputL), nullptr, PFFFT_FORWARD );
            pffft_transform_ordered( m_pffftPlan, m_inputR, reinterpret_cast<float*>(m_outputR), nullptr, PFFFT_FORWARD );

            // grab which of the double-buffer arrays we should write into - !the current one
            const std::size_t currentBufferIdx = m_outputBucketsIndex.load();
            const std::size_t writeBufferIdx = !currentBufferIdx;

            // reset buckets
            Result& bucketResult = m_outputBuckets[writeBufferIdx];
            bucketResult.fill( 0 );

            const double maxDb      = m_config.maxDb;
            const double minDb      = m_config.minDb;
            const double headroom   = maxDb - minDb;

            // sum magnitudes into the chosen buckets
            for ( std::size_t freqBin = 0; freqBin < m_fftWindowSize / 2; freqBin++ )
            {
                const float fftMagL = m_outputL[freqBin].hypot();
                const float fftMagR = m_outputR[freqBin].hypot();

                bucketResult[m_bucketIndices[freqBin]] += (fftMagL + fftMagR) * 0.5f; // #hdd average of magnitudes 'correct' here?
            }
            // then process each bucket
            for ( std::size_t bucketIndex = 0; bucketIndex < bucketResult.size(); bucketIndex++ )
            {
                bucketResult[bucketIndex] /= static_cast<float>( m_countPerBucket[bucketIndex] );

                // convert to dB to help normalise the range
                double fftDb = cycfi::q::lin_to_db( bucketResult[bucketIndex] ).rep;

                // then clip and rescale to 0..1 via headroom dB range
                       fftDb = std::max( 0.0, fftDb + std::abs( minDb ) );
                       fftDb = std::min( 1.0, fftDb / headroom );

                bucketResult[bucketIndex] = static_cast<float>( fftDb );
            }

            // flip buffers by updating the current index with the one we just wrote into
            m_outputBucketsIndex.store( writeBufferIdx );

            // reset the buffer write position ready for the next batch
            m_inputWriteIndex = 0;
        }

#if SCOPE_ALLOW_MULTI_FFT_PER_APPEND
        if ( sampleCount == 0 )
            break;
#endif // SCOPE_ALLOW_MULTI_FFT_PER_APPEND
    }
}

} // namespace dsp
