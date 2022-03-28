//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "base/utils.h"

namespace base {

// ---------------------------------------------------------------------------------------------------------------------
template< typename _quantisedType, uint32_t _quantBits >
struct InterleavingQuantiseBuffer
{
    DeclareUncopyable( InterleavingQuantiseBuffer );
    InterleavingQuantiseBuffer( InterleavingQuantiseBuffer&& ) = default;
    InterleavingQuantiseBuffer& operator= ( InterleavingQuantiseBuffer&& ) = default;

    InterleavingQuantiseBuffer() = delete;
    InterleavingQuantiseBuffer( const uint32_t sampleCount )
        : m_maximumSamples( sampleCount )
    {
        const auto totalStereoSamples = m_maximumSamples * 2;

        m_interleavedFloat  = mem::malloc16As< float >( totalStereoSamples );
        m_interleavedQuant  = mem::malloc16AsSet< _quantisedType >( totalStereoSamples, 0 );
    }

    ~InterleavingQuantiseBuffer()
    {
        if ( m_interleavedFloat != nullptr )
            mem::free16( m_interleavedFloat );
        m_interleavedFloat = nullptr;

        if ( m_interleavedQuant != nullptr )
            mem::free16( m_interleavedQuant );
        m_interleavedQuant = nullptr;

        m_committed = false;
    }

    inline void quantise();

    const uint32_t      m_maximumSamples;
    uint32_t            m_currentSamples    = 0;
    float*              m_interleavedFloat  = nullptr;
    _quantisedType*     m_interleavedQuant  = nullptr;
    bool                m_committed         = false;
};

// ---------------------------------------------------------------------------------------------------------------------
template<>
inline void InterleavingQuantiseBuffer<int16_t, 16>::quantise()
{
    static constexpr float   fScaler16  = (float)0x7fffL;
    static constexpr int32_t fInt16Max  = ( 0x7fffL        );
    static constexpr int32_t fInt16Min  = ( -fInt16Max - 1 );

    const auto totalStereoSamples = m_currentSamples * 2;
    for ( size_t i = 0; i < totalStereoSamples; i++ )
    {
        m_interleavedQuant[i] = (int16_t)std::clamp( (int32_t)(m_interleavedFloat[i] * fScaler16), fInt16Min, fInt16Max );
    }
}
using IQ16Buffer = InterleavingQuantiseBuffer< int16_t, 16 >;

// ---------------------------------------------------------------------------------------------------------------------
template<>
inline void InterleavingQuantiseBuffer<int32_t, 24>::quantise()
{
    static constexpr float   fScaler24  = (float)0x7fffffL;
    static constexpr int32_t fInt24Max  = (  0x7fffffL     );
    static constexpr int32_t fInt24Min  = ( -fInt24Max - 1 );

    const auto totalStereoSamples = m_currentSamples * 2;
    for ( size_t i = 0; i < totalStereoSamples; i++ )
    {
        m_interleavedQuant[i] = (int32_t)std::clamp( (int32_t)(m_interleavedFloat[i] * fScaler24), fInt24Min, fInt24Max );
    }
}
using IQ24Buffer = InterleavingQuantiseBuffer< int32_t, 24 >;

// ---------------------------------------------------------------------------------------------------------------------
template< typename T >
concept IQBufferType = requires(T x) {
    { InterleavingQuantiseBuffer{ std::move(x) } } -> std::same_as<T>;
};

} // namespace base