//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//

#pragma once

#include "base/utils.h"

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
        const int32_t width,
        const int32_t height )
        : m_width(width)
        , m_height(height)
    {
        m_buffer = mem::malloc16AsSet<_Type>( m_width * m_height, (_Type)0 );
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

    inline int32_t getWidth() const
    {
        return m_width;
    }

    inline int32_t getHeight() const
    {
        return m_height;
    }


    inline void clear( const _Type& value )
    {
        for ( size_t i = 0; i < m_width * m_height; i++ )
        {
            m_buffer[i] = value;
        }
    }


    // direct (x,y) accessor does no error checking
    inline const _Type& operator () ( const int32_t &x, const int32_t &y ) const
    {
        assert( x >= 0 && x < m_width );
        assert( y >= 0 && y < m_height );

        return m_buffer[( y * m_width ) + x];
    }

    // direct (x,y) accessor does no error checking
    inline _Type& operator () ( const int32_t &x, const int32_t &y )
    {
        assert( x >= 0 && x < m_width );
        assert( y >= 0 && y < m_height );

        return m_buffer[( y * m_width ) + x];
    }

    // access the data buffer linearly with []
    inline const _Type& operator [] ( const size_t offset ) const
    {
        assert( offset < ( m_width * m_height) );

        return m_buffer[offset];
    }

    // access the data buffer linearly with []
    inline _Type& operator [] ( const size_t offset )
    {
        assert( offset < ( m_width * m_height) );

        return m_buffer[offset];
    }


    // set a entry, safely ignoring out-of-bounds XY coordinates
    inline void poke(
        const int32_t x,
        const int32_t y,
        const _Type& colour )
    {
        if ( x < 0 ||
             x >= m_width ||
             y < 0 ||
             y >= m_height )
        {
            return;
        }

        m_buffer[( y * m_width ) + x] = colour;
    }

    // get a pointer back to a entry, or null if XY is out-of bounds
    inline _Type* peek(
        const int32_t x,
        const int32_t y )
    {
        if ( x < 0 ||
             x >= m_width ||
             y < 0 ||
             y >= m_height )
        {
            return nullptr;
        }

        return &m_buffer[( y * m_width ) + x];
    }

    inline _Type* getBuffer() { return m_buffer; }

    inline void findMinMax( _Type& minValue, _Type& maxValue )
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

    int32_t     m_width;
    int32_t     m_height;
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
