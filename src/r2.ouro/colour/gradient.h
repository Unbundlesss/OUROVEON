//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

// serial ports for ISPC code originally written on Win; holding here until we sort x-platform ISPC etc

#pragma once

#include "base/mathematics.h"

namespace colour {

inline float tricycle( const float phase )
{
    const float zO = base::fract( phase );
    const float rV = (zO <= 0.5f) ? zO : (1.0f - zO);
    return rV * 2.0f;
}

#define _PACK_COL32(R,G,B,A) ( ((uint32_t)(R)<<0) | ((uint32_t)(G)<<8) | ((uint32_t)(B)<<16) | ((uint32_t)(A)<<24) )

struct col3
{
    constexpr col3( const float _x, const float _y, const float _z )
        : x(_x)
        , y(_y)
        , z(_z)
    {}

    constexpr col3( const float _single )
        : x( _single )
        , y( _single )
        , z( _single )
    {}

    constexpr col3 operator*( const float rhs ) const { return col3( this->x * rhs, this->y * rhs, this->z * rhs ); }
    constexpr col3 operator+( const col3& rhs ) const { return col3( this->x + rhs.x, this->y + rhs.y, this->z + rhs.z ); }
    constexpr col3 operator*( const col3& rhs ) const { return col3( this->x * rhs.x, this->y * rhs.y, this->z * rhs.z ); }
    constexpr col3 operator/( const col3& rhs ) const { return col3( this->x / rhs.x, this->y / rhs.y, this->z / rhs.z ); }
    constexpr col3& operator*=( const float rhs ) { this->x *= rhs; this->y *= rhs; this->z *= rhs; return *this; }
    constexpr col3& operator+=( const col3& rhs ) { this->x += rhs.x; this->y += rhs.y; this->z += rhs.z; return *this; }
    constexpr col3& operator*=( const col3& rhs ) { this->x *= rhs.x; this->y *= rhs.y; this->z *= rhs.z; return *this; }
    constexpr col3& operator/=( const col3& rhs ) { this->x /= rhs.x; this->y /= rhs.y; this->z /= rhs.z; return *this; }

    inline uint32_t bgrU32() const
    {
        uint32_t _x = (uint32_t)(std::clamp( x, 0.0f, 1.0f ) * 255.0f );
        uint32_t _y = (uint32_t)(std::clamp( y, 0.0f, 1.0f ) * 255.0f );
        uint32_t _z = (uint32_t)(std::clamp( z, 0.0f, 1.0f ) * 255.0f );
        return _PACK_COL32( _z, _y, _x, 255 );
    }

    static col3 cosine( const col3& rhs )
    {
        return col3( (float)std::cos(rhs.x), (float)std::cos( rhs.y ), (float)std::cos( rhs.z ) );
    }

    static col3 cosineGradient(
        const float t,
        const col3& a,
        const col3& b,
        const col3& c,
        const col3& d )
    {
        return a + (b * col3::cosine( (c * t + d) * (float)constants::d_2pi ));
    }

    float x = 0.0f,
          y = 0.0f,
          z = 0.0f;

    friend inline col3 operator*( const float lhs, const col3& rhs ) { return col3( rhs.x * lhs, rhs.y * lhs, rhs.z * lhs ); }
};


namespace map
{
    col3 plasma( const float  t )
    {
        static constexpr col3 c0( 0.05873234392399702f, 0.02333670892565664f, 0.5433401826748754f );
        static constexpr col3 c1( 2.176514634195958f, 0.2383834171260182f, 0.7539604599784036f );
        static constexpr col3 c2( -2.689460476458034f, -7.455851135738909f, 3.110799939717086f );
        static constexpr col3 c3( 6.130348345893603f, 42.3461881477227f, -28.51885465332158f );
        static constexpr col3 c4( -11.10743619062271f, -82.66631109428045f, 60.13984767418263f );
        static constexpr col3 c5( 10.02306557647065f, 71.41361770095349f, -54.07218655560067f );
        static constexpr col3 c6( -3.658713842777788f, -22.93153465461149f, 18.19190778539828f );

        return c0 + t * (c1 + t * (c2 + t * (c3 + t * (c4 + t * (c5 + t * c6)))));
    }

    col3 inferno( const float  t )
    {
        static constexpr col3 c0( 0.0002189403691192265f, 0.001651004631001012f, -0.01948089843709184f );
        static constexpr col3 c1( 0.1065134194856116f, 0.5639564367884091f, 3.932712388889277f );
        static constexpr col3 c2( 11.60249308247187f, -3.972853965665698f, -15.9423941062914f );
        static constexpr col3 c3( -41.70399613139459f, 17.43639888205313f, 44.35414519872813f );
        static constexpr col3 c4( 77.162935699427f, -33.40235894210092f, -81.80730925738993f );
        static constexpr col3 c5( -71.31942824499214f, 32.62606426397723f, 73.20951985803202f );
        static constexpr col3 c6( 25.13112622477341f, -12.24266895238567f, -23.07032500287172f );

        return c0 + t * (c1 + t * (c2 + t * (c3 + t * (c4 + t * (c5 + t * c6)))));
    }

    // from https://www.shadertoy.com/view/Nd3fR2

    // makes cividis colormap with polynimal 6
    col3 cividis( const float t )
    {
        static constexpr col3 c0(   -0.008598f,    0.136152f,    0.291357f );
        static constexpr col3 c1(   -0.415049f,    0.639599f,    3.028812f );
        static constexpr col3 c2(   15.655097f,    0.392899f,  -22.640943f );
        static constexpr col3 c3(  -59.689584f,   -1.424169f,   75.666364f );
        static constexpr col3 c4(  103.509006f,    2.627500f, -122.512551f );
        static constexpr col3 c5(  -84.086992f,   -2.156916f,   94.888003f );
        static constexpr col3 c6(   26.055600f,    0.691800f,  -28.537831f );

        return (c0 + t * (c1 + t * (c2 + t * (c3 + t * (c4 + t * (c5 + t * c6))))));
    }

    // makes coolwarm colormap with polynimal 6
    col3 coolwarm( const float t )
    {
        static constexpr col3 c0(    0.227376f,    0.286898f,    0.752999f );
        static constexpr col3 c1(    1.204846f,    2.314886f,    1.563499f );
        static constexpr col3 c2(    0.102341f,   -7.369214f,   -1.860252f );
        static constexpr col3 c3(    2.218624f,   32.578457f,   -1.643751f );
        static constexpr col3 c4(   -5.076863f,  -75.374676f,   -3.704589f );
        static constexpr col3 c5(    1.336276f,   73.453060f,    9.595678f );
        static constexpr col3 c6(    0.694723f,  -25.863102f,   -4.558659f );

        return (c0 + t * (c1 + t * (c2 + t * (c3 + t * (c4 + t * (c5 + t * c6))))));
    }

    // makes Spectral_r colormap with polynimal 6
    col3 Spectral_r( const float t )
    {
        static constexpr col3 c0(    0.426208f,    0.275203f,    0.563277f );
        static constexpr col3 c1(   -5.321958f,    3.761848f,    5.477444f );
        static constexpr col3 c2(   42.422339f,  -15.057685f,  -57.232349f );
        static constexpr col3 c3( -100.917716f,   57.029463f,  232.590601f );
        static constexpr col3 c4(  106.422535f, -116.177338f, -437.123306f );
        static constexpr col3 c5(  -48.460514f,  103.570154f,  378.807920f );
        static constexpr col3 c6(    6.016269f,  -33.393152f, -122.850806f );

        return (c0 + t * (c1 + t * (c2 + t * (c3 + t * (c4 + t * (c5 + t * c6))))));
    }

    // makes YlGnBu_r colormap with polynimal 6
    col3 YlGnBu_r( const float t )
    {
        static constexpr col3 c0(    0.016999f,    0.127718f,    0.329492f );
        static constexpr col3 c1(    1.571728f,    0.025897f,    2.853610f );
        static constexpr col3 c2(   -4.414197f,    5.924816f,  -11.635781f );
        static constexpr col3 c3(  -12.438137f,   -8.086194f,   34.584365f );
        static constexpr col3 c4(   67.131044f,   -2.929808f,  -58.635788f );
        static constexpr col3 c5(  -82.372983f,   11.898509f,   47.184502f );
        static constexpr col3 c6(   31.515446f,   -5.975157f,  -13.820580f );

        return (c0 + t * (c1 + t * (c2 + t * (c3 + t * (c4 + t * (c5 + t * c6))))));
    }

    // makes RdYlBu_r colormap with polynimal 6
    col3 RdYlBu_r( const float t )
    {
        static constexpr col3 c0(    0.207621f,    0.196195f,    0.618832f );
        static constexpr col3 c1(   -0.088125f,    3.196170f,   -0.353302f );
        static constexpr col3 c2(    8.261232f,   -8.366855f,   14.368787f );
        static constexpr col3 c3(   -2.922476f,   33.244294f,  -43.419173f );
        static constexpr col3 c4(  -34.085327f,  -74.476041f,   37.159352f );
        static constexpr col3 c5(   50.429790f,   67.145621f,   -1.750169f );
        static constexpr col3 c6(  -21.188828f,  -20.935464f,   -6.501427f );

        return (c0 + t * (c1 + t * (c2 + t * (c3 + t * (c4 + t * (c5 + t * c6))))));
    }

    col3 trans( const float t )
    {
        static constexpr col3 c0(  0.428f, 0.598f, 1.258f );
        static constexpr col3 c1(  0.418f, 0.308f, 0.418f );
        static constexpr col3 c2( -0.722f, 0.448f, 0.338f );
        static constexpr col3 c3(  1.978f, 0.528f, 0.468f );

        return col3::cosineGradient( t, c0, c1, c2, c3 );
    }

} // namespace map

} // namespace colour
