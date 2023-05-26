//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

namespace dsp {

    // tuple used when setting up FFT input buffers and decoding outputs
    struct complexf
    {
        complexf( const float real, const float imag )
            : m_real( real )
            , m_imag( imag )
        {}

        float m_real;
        float m_imag;

        inline float hypot() const
        {
            return std::sqrt( (m_real * m_real) + (m_imag * m_imag) );
        }
    };

} // namespace dsp
