//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//

#pragma once

#include "base/utils.h"
#include "base/mathematics.h"

namespace base {

// ---------------------------------------------------------------------------------------------
template< typename _Type >
class Buffer2D
{
using selftype = Buffer2D< _Type >;
public:

    Buffer2D() = delete;
    Buffer2D( const Buffer2D& rhs ) = delete;

    Buffer2D(
        const uint32_t width,
        const uint32_t height )
        : m_width(width)
        , m_height(height)
    {
        m_buffer = mem::alloc16To<_Type>( m_width * m_height, (_Type)0 );
    }

    Buffer2D( Buffer2D&& other )
        : m_width( other.getWidth() )
        , m_height( other.getHeight() )
    {
        m_buffer = other.m_buffer;
        other.m_buffer = nullptr;
    }

    ~Buffer2D()
    {
        mem::free16( m_buffer );
    }

    ouro_nodiscard constexpr uint32_t getWidth() const
    {
        return m_width;
    }

    ouro_nodiscard constexpr uint32_t getHeight() const
    {
        return m_height;
    }


    constexpr void clear( const _Type& value )
    {
        for ( size_t i = 0; i < m_width * m_height; i++ )
        {
            m_buffer[i] = value;
        }
    }


    // direct (x,y) accessor does no error checking
    ouro_nodiscard constexpr const _Type& operator () ( const uint32_t &x, const uint32_t &y ) const
    {
        ABSL_ASSERT( x < m_width );
        ABSL_ASSERT( y < m_height );

        return m_buffer[( y * m_width ) + x];
    }

    // direct (x,y) accessor does no error checking
    ouro_nodiscard constexpr _Type& operator () ( const uint32_t &x, const uint32_t &y )
    {
        ABSL_ASSERT( x < m_width );
        ABSL_ASSERT( y < m_height );

        return m_buffer[( y * m_width ) + x];
    }

    // access the data buffer linearly with []
    ouro_nodiscard constexpr const _Type& operator [] ( const size_t offset ) const
    {
        ABSL_ASSERT( offset < ( m_width * m_height ) );

        return m_buffer[offset];
    }

    // access the data buffer linearly with []
    ouro_nodiscard constexpr _Type& operator [] ( const size_t offset )
    {
        ABSL_ASSERT( offset < ( m_width * m_height ) );

        return m_buffer[offset];
    }


    // set a entry, safely ignoring out-of-bounds XY coordinates
    constexpr void poke(
        const uint32_t x,
        const uint32_t y,
        const _Type& colour )
    {
        if ( x >= m_width ||
             y >= m_height )
        {
            return;
        }

        m_buffer[( y * m_width ) + x] = colour;
    }

    // get a pointer back to a entry, or null if XY is out-of bounds
    ouro_nodiscard constexpr _Type* peek(
        const uint32_t x,
        const uint32_t y )
    {
        if ( x >= m_width ||
             y >= m_height )
        {
            return nullptr;
        }

        return &m_buffer[( y * m_width ) + x];
    }

    ouro_nodiscard constexpr       _Type* getBuffer()       { return m_buffer; }
    ouro_nodiscard constexpr const _Type* getBuffer() const { return m_buffer; }

    constexpr void findMinMax( _Type& minValue, _Type& maxValue )
    {
        minValue = std::numeric_limits< _Type>::max();
        maxValue = std::numeric_limits< _Type>::lowest();
        for ( size_t i = 0; i < m_width * m_height; i++ )
        {
            minValue = std::min( minValue, m_buffer[i] );
            maxValue = std::max( maxValue, m_buffer[i] );
        }
    }

private:

    _Type      *m_buffer;

    uint32_t     m_width;
    uint32_t     m_height;
};


// ---------------------------------------------------------------------------------------------

using FloatBuffer     = Buffer2D<float>;
using U32Buffer       = Buffer2D<uint32_t>;


// ------------------------------------------------------------------------------------------------
//
inline float bilinearSample( 
    const FloatBuffer& image,
    const float x, 
    const float y )
{
    // reflect if position is negative
    const float absX = fabsf(x);
    const float absY = fabsf(y);

    // find the candidate X/Y cells at either edge of the float positions; clamp in the progress
    const int32_t floorX = (int32_t)( floorf( absX ) ) % image.getWidth();
    const int32_t ceilX  = (int32_t)( ceilf( absX ) )  % image.getWidth();
    const int32_t floorY = (int32_t)( floorf( absY ) ) % image.getHeight();
    const int32_t ceilY  = (int32_t)( ceilf( absY ) )  % image.getHeight();

    // compute relative weights for the ceil/floor values on each axis
    const float CX_weight = fract( x );
    const float FX_weight = 1.0f - CX_weight;
    const float CY_weight = fract( y );
    const float FY_weight = 1.0f - CY_weight;

    // bilinear blend
    const float sample_FX_FY = ( image( floorX,  floorY ) * FY_weight ) * FX_weight;
    const float sample_CX_FY = ( image( ceilX,   floorY ) * FY_weight ) * CX_weight;
    const float sample_FX_CY = ( image( floorX,  ceilY  ) * CY_weight ) * FX_weight;
    const float sample_CX_CY = ( image( ceilX,   ceilY  ) * CY_weight ) * CX_weight;

    return sample_FX_FY + 
           sample_CX_FY + 
           sample_FX_CY + 
           sample_CX_CY;
}

} // namespace base
