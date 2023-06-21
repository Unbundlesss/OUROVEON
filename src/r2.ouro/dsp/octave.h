//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "base/construction.h"

namespace dsp {

    struct OctaveBandFrequencies
    {
        constexpr OctaveBandFrequencies( const double low, const double center, const double high )
            : m_low( low )
            , m_center( center )
            , m_high( high )
        {}
        cycfi::q::frequency m_low;
        cycfi::q::frequency m_center;
        cycfi::q::frequency m_high;
    };

    // convenient storage of the frequency bands corresponding to useful octaves (well, also a pair below 40hz)
    static constexpr std::array OctaveBands = 
    {
        OctaveBandFrequencies(    11.049,    15.625,    22.097 ),
        OctaveBandFrequencies(    22.097,    31.250,    44.194 ),
        OctaveBandFrequencies(    44.194,    62.500,    88.388 ),
        OctaveBandFrequencies(    88.388,   125.000,   176.777 ),
        OctaveBandFrequencies(   176.777,   250.000,   353.553 ),
        OctaveBandFrequencies(   353.553,   500.000,   707.107 ),
        OctaveBandFrequencies(   707.107,  1000.000,  1414.214 ),
        OctaveBandFrequencies(  1414.214,  2000.000,  2828.427 ),
        OctaveBandFrequencies(  2828.427,  4000.000,  5656.854 ),
        OctaveBandFrequencies(  5656.854,  8000.000, 11313.708 ),
        OctaveBandFrequencies( 11313.708, 16000.000, 22627.417 )
    };

    // helper object for building an array of 'frequency buckets' to sort an FFT into;
    // produces a look-up array the size of a FFT window with indices into TFrequencyBucketCount number of octave buckets
    template < std::size_t TFrequencyBucketCount >
    struct FFTOctaveBuckets
    {
        DECLARE_NO_COPY_NO_MOVE( FFTOctaveBuckets );

        static constexpr std::size_t BucketCount = TFrequencyBucketCount;

        using CountPerBucket        = std::array< uint32_t, TFrequencyBucketCount >;
        using RecpFCountPerBucket   = std::array< float, TFrequencyBucketCount >;

        FFTOctaveBuckets() = default;
        ~FFTOctaveBuckets()
        {
            mem::free16( m_bucketIndices );
            m_bucketIndices     = nullptr;
            m_bucketIndicesSize = 0;
        }

        // pass in an array of TFrequencyBucketCount octave indices, eg. 3, 6, 9 to have 3 buckets covering 0-3, 3-6, 6-9 octave bands sorted
        void configure( const std::array< std::size_t, TFrequencyBucketCount >& OctaveBandLayout, const uint32_t sampleRate, const uint32_t fftWindowSize )
        {
            ABSL_ASSERT( OctaveBandLayout.size() == TFrequencyBucketCount );
            
            if ( m_bucketIndices != nullptr )
                mem::free16( m_bucketIndices );

            // https://www.nti-audio.com/en/support/know-how/fast-fourier-transform-fft
            const uint32_t nyquist = sampleRate / 2; // theoretical maximum frequency that can be determined by the FFT
            const double nyquistRecp = 1.0 / static_cast<double>(nyquist);

            // only half the resulting FFT is 'useful' for frequency binning before mirroring
            const uint32_t frequencyBinSize = fftWindowSize / 2;
            const double frequencyBinSizeDbl = static_cast<double>(frequencyBinSize);

            m_bucketIndicesSize = frequencyBinSize;
            m_bucketIndices     = mem::alloc16<uint8_t>( frequencyBinSize );

            m_countPerBucket.fill( 0 );
            m_countPerBucketRecpF.fill( 1 );

            // precompute frequency binning, splitting per octave 
            int previousSample = 0; // always assume we start from the 0th FFT index
            uint8_t bucket = 0;
            for ( std::size_t octave : OctaveBandLayout )
            {
                const uint32_t endSample = std::min( (uint32_t)(std::ceil( cycfi::q::as_double( OctaveBands[octave].m_high ) * nyquistRecp * frequencyBinSizeDbl )), frequencyBinSize );

                for ( uint32_t s = previousSample; s < endSample; s++ )
                {
                    m_bucketIndices[s] = bucket;
                }
                m_countPerBucket[bucket] = endSample - previousSample;
                m_countPerBucketRecpF[bucket] = 1.0f / static_cast<float>( m_countPerBucket[bucket] );

                blog::core( "bin {} |  {} Hz |  {} -> {}  ", octave, cycfi::q::as_float( OctaveBands[octave].m_high ), previousSample, endSample );

                previousSample = endSample;

                bucket++;
            }
            if ( previousSample > 0 )
            {
                // if the end bucket came in under the fft window size, just fill it in with the end bucket
                for ( uint32_t s = previousSample; s < frequencyBinSize; s++ )
                {
                    m_bucketIndices[s] = m_bucketIndices[previousSample - 1];
                }
            }
        }

        constexpr uint32_t getSizeOfBucketAt( const std::size_t index ) const
        {
            return m_countPerBucket[index];
        }

        constexpr float getRecpSizeOfBucketAt( const std::size_t index ) const
        {
            return m_countPerBucketRecpF[index];
        }

        constexpr uint8_t getBucketForFFTIndex( const std::size_t fftIndex ) const
        {
            ABSL_ASSERT( fftIndex < m_bucketIndicesSize );
            return m_bucketIndices[fftIndex];
        }

    private:

        CountPerBucket      m_countPerBucket;           // how many entries from the FFT block are funneled into each frequency bucket
        RecpFCountPerBucket m_countPerBucketRecpF;
        uint8_t*            m_bucketIndices = nullptr;  // per entry in the FFT block, which frequency bucket should it
                                                        // be shunted into; precalculated during init
        std::size_t         m_bucketIndicesSize = 0;
    };
} // namespace dsp
