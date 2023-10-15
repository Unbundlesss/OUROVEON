//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
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
    , m_hannGenerator( cycfi::q::duration(1.0), 1.0f ) // this has no default ctor, so this is a dummy, it gets reconfigured below
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
    m_hannGenerator.config( cycfi::q::duration( (double)m_fftWindowSize / (double)m_sampleRate ), static_cast<float>( m_sampleRate ) );


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

    m_octaves.configure(
        { 3, 4, 5, 6, 7, 8, 9, 10 },
        sampleRate,
        m_fftWindowSize );
}

// ---------------------------------------------------------------------------------------------------------------------
Scope8::~Scope8()
{
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

            // sum magnitudes into the chosen buckets
            for ( std::size_t freqBin = 0; freqBin < m_fftWindowSize / 2; freqBin++ )
            {
                const float fftMagL = m_outputL[freqBin].hypot();
                const float fftMagR = m_outputR[freqBin].hypot();

                bucketResult[ m_octaves.getBucketForFFTIndex(freqBin) ] += (fftMagL + fftMagR) * 0.5f; // #hdd average of magnitudes 'correct' here?
            }

            // .. then process each bucket
            for ( std::size_t bucketIndex = 0; bucketIndex < bucketResult.size(); bucketIndex++ )
            {
                bucketResult[bucketIndex] *= m_octaves.getRecpSizeOfBucketAt(bucketIndex);
                bucketResult[bucketIndex]  = m_config.headroomNormaliseDb( bucketResult[bucketIndex] );
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
