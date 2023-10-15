//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//

#pragma once

#include <limits>

namespace base {
namespace detail {

double constexpr sqrtNewtonRaphson(double x, double curr, double prev)
{
	return curr == prev
		? curr
		: sqrtNewtonRaphson(x, 0.5 * (curr + x / curr), curr);
}

} // namespace detail

// ---------------------------------------------------------------------------------------------------------------------
// https://gist.github.com/alexshtf/eb5128b3e3e143187794
double constexpr constSqrt( double x )
{
	return x >= 0 && x < std::numeric_limits<double>::infinity()
		? detail::sqrtNewtonRaphson(x, x, 0)
		: std::numeric_limits<double>::quiet_NaN();
}

// ---------------------------------------------------------------------------------------------------------------------
constexpr uint32_t nextPow2( uint32_t v )
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

// ---------------------------------------------------------------------------------------------------------------------
template <typename _T>
inline _T fract( const _T x )
{
    return x - std::floor( x );
}

// ---------------------------------------------------------------------------------------------------------------------
// wrap x -> [0,max)
inline float wrapMax( const float x, const float max )
{
    // (max + x % max) % max
    return std::fmod( max + std::fmod( x, max ), max );
}

// ---------------------------------------------------------------------------------------------------------------------
// wrap x -> [min,max) 
inline float wrapMinMax( const float x, const float min, const float max )
{
    return min + wrapMax( x - min, max - min );
}

// ---------------------------------------------------------------------------------------------------------------------
// remap a value inside [source min/max] to [target min/max]
template <typename _T>
constexpr _T remapRange( const _T inputValue, const _T sourceRangeMin, const _T sourceRangeMax, const _T targetRangeMin, const _T targetRangeMax )
{
    ABSL_ASSERT( sourceRangeMax != sourceRangeMin );
    return targetRangeMin + ((targetRangeMax - targetRangeMin) * (inputValue - sourceRangeMin)) / (sourceRangeMax - sourceRangeMin);
}

// ---------------------------------------------------------------------------------------------------------------------
template< typename _T >
constexpr _T lerp( const _T& a, const _T& b, const float s )
{
    return a + ( (b - a) * s );
}

// ---------------------------------------------------------------------------------------------------------------------
constexpr float smoothstep( const float edge0, const float edge1, const float f )
{
    const float t = std::clamp( (f - edge0) / (edge1 - edge0), 0.0f, 1.0f );
    return t * t * (3.0f - 2.0f * t);
}

// ---------------------------------------------------------------------------------------------------------------------
inline bool floatAlmostEqualRelative( float A, float B, float maxRelDiff = FLT_EPSILON )
{
    // calculate the difference.
    const float diff = std::abs( A - B );
    A = std::abs( A );
    B = std::abs( B );

    // find the largest
    const float largest = (B > A) ? B : A;

    if ( diff <= largest * maxRelDiff )
        return true;
    return false;
}

// ---------------------------------------------------------------------------------------------------------------------
template <typename _T>
constexpr _T decibelsToGain ( const _T decibels, const _T minusInfinityDb = _T (-100) )
{
    return decibels > minusInfinityDb ? std::pow ( _T(10.0), decibels * _T(0.05) )
                                        : _T();
}

// ---------------------------------------------------------------------------------------------------------------------
template <typename _T>
static _T gainToDecibels ( const _T gain, const _T minusInfinityDb = _T ( -100 ) )
{
    return gain > _T() ? std::max( minusInfinityDb, static_cast<_T>( std::log10(gain) ) * _T(20.0) )
                       : minusInfinityDb;
}


} // namespace base
