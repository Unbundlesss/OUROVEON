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

#include "base/utils.h"

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

    float x = 0.0f,
          y = 0.0f,
          z = 0.0f;

    friend inline col3 operator*( const float lhs, const col3& rhs ) { return col3( rhs.x * lhs, rhs.y * lhs, rhs.z * lhs ); }
};


col3 cosineGradient(
    const float t,
    const col3& a,
    const col3& b,
    const col3& c,
    const col3& d )
{
    return a + ( b * col3::cosine( (c * t + d) * (float)constants::d_2pi ) );
}

col3 cosineGradientRainbow( const float t )
{
    static constexpr col3 f3_pt5 ( 0.5f,  0.5f, 0.5f );
    static constexpr col3 f3_one ( 1.0f,  1.0f, 1.0f );
    static constexpr col3 f3_mix ( 0.0f, 0.33f, 0.67f );

    return cosineGradient( t, f3_pt5, f3_pt5, f3_one, f3_mix );
}

col3 cosineGradientOrangeBlue( const float t )
{
    static constexpr col3 f3_pt5  ( 0.5f, 0.5f, 0.5f );
    static constexpr col3 f3_row3 ( 0.8f, 0.8f, 0.5f );
    static constexpr col3 f3_row4 ( 0.0f, 0.2f, 0.5f );

    return cosineGradient( t, f3_pt5, f3_pt5, f3_row3, f3_row4 );
}


// ---------------------------------------------------------------------------------------------------------------------

uint32_t gradient_grayscale_u32( const float  t )
{
    const col3 c0( base::fract( t ) );

    return c0.bgrU32();
}
uint32_t gradient_grayscale_cycled_u32( const float  t )
{
    const col3 c0( tricycle( t ) );

    return c0.bgrU32();
}

// ---------------------------------------------------------------------------------------------------------------------
// 
col3 gradient_plasma( const float  t )
{
    static constexpr col3 c0(   0.05873234392399702f, 0.02333670892565664f, 0.5433401826748754f  );
    static constexpr col3 c1(   2.176514634195958f,   0.2383834171260182f,  0.7539604599784036f  );
    static constexpr col3 c2(  -2.689460476458034f,  -7.455851135738909f,   3.110799939717086f   );
    static constexpr col3 c3(   6.130348345893603f,  42.3461881477227f,   -28.51885465332158f    );
    static constexpr col3 c4( -11.10743619062271f,  -82.66631109428045f,   60.13984767418263f    );
    static constexpr col3 c5(  10.02306557647065f,   71.41361770095349f,  -54.07218655560067f    );
    static constexpr col3 c6(  -3.658713842777788f, -22.93153465461149f,   18.19190778539828f    );

    return c0 + t * (c1 + t * (c2 + t * (c3 + t * (c4 + t * (c5 + t * c6)))));
}

uint32_t gradient_plasma_u32( const float  t )
{
    return gradient_plasma( base::fract(t) ).bgrU32();
}
uint32_t gradient_plasma_cycled_u32( const float  t )
{
    return gradient_plasma( tricycle(t) ).bgrU32();
}


// ---------------------------------------------------------------------------------------------------------------------
// 
col3 gradient_turbo( const float  t )
{
    static constexpr col3 c0(    0.1140890109226559f, 0.06288340699912215f, 0.2248337216805064f  );
    static constexpr col3 c1(    6.716419496985708f,  3.182286745507602f,   7.571581586103393f   );
    static constexpr col3 c2(  -66.09402360453038f,  -4.9279827041226f,   -10.09439367561635f    );
    static constexpr col3 c3(  228.7660791526501f,   25.04986699771073f,  -91.54105330182436f    );
    static constexpr col3 c4( -334.8351565777451f,  -69.31749712757485f,  288.5858850615712f     );
    static constexpr col3 c5(  218.7637218434795f,   67.52150567819112f, -305.2045772184957f     );
    static constexpr col3 c6(  -52.88903478218835f, -21.54527364654712f,  110.5174647748972f     );

    return c0 + t * (c1 + t * (c2 + t * (c3 + t * (c4 + t * (c5 + t * c6)))));
}

uint32_t gradient_turbo_u32( const float  t )
{
    return gradient_turbo( base::fract(t) ).bgrU32();
}
uint32_t gradient_turbo_cycled_u32( const float  t )
{
    return gradient_turbo( tricycle(t) ).bgrU32();
}

// ---------------------------------------------------------------------------------------------------------------------
// 
col3 gradient_magma( const float  t )
{
    static constexpr col3 c0(  -0.002136485053939582f, -0.000749655052795221f, -0.005386127855323933f  );
    static constexpr col3 c1(   0.2516605407371642f,    0.6775232436837668f,    2.494026599312351f     );
    static constexpr col3 c2(   8.353717279216625f,    -3.577719514958484f,     0.3144679030132573f    );
    static constexpr col3 c3( -27.66873308576866f,     14.26473078096533f,    -13.64921318813922f      );
    static constexpr col3 c4(  52.17613981234068f,    -27.94360607168351f,     12.94416944238394f      );
    static constexpr col3 c5( -50.76852536473588f,     29.04658282127291f,      4.23415299384598f      );
    static constexpr col3 c6(  18.65570506591883f,    -11.48977351997711f,     -5.601961508734096f     );

    return c0 + t * (c1 + t * (c2 + t * (c3 + t * (c4 + t * (c5 + t * c6)))));
}

uint32_t gradient_magma_u32( const float  t )
{
    return gradient_magma( base::fract(t) ).bgrU32();
}
uint32_t gradient_magma_cycled_u32( const float  t )
{
    return gradient_magma( tricycle(t) ).bgrU32();
}

// ---------------------------------------------------------------------------------------------------------------------
// 
col3 gradient_inferno( const float  t )
{
    static constexpr col3 c0(   0.0002189403691192265f, 0.001651004631001012f, -0.01948089843709184f );
    static constexpr col3 c1(   0.1065134194856116f,    0.5639564367884091f,    3.932712388889277f   );
    static constexpr col3 c2(  11.60249308247187f,     -3.972853965665698f,   -15.9423941062914f     );
    static constexpr col3 c3( -41.70399613139459f,     17.43639888205313f,     44.35414519872813f    );
    static constexpr col3 c4(  77.162935699427f,      -33.40235894210092f,    -81.80730925738993f    );
    static constexpr col3 c5( -71.31942824499214f,     32.62606426397723f,     73.20951985803202f    );
    static constexpr col3 c6(  25.13112622477341f,    -12.24266895238567f,    -23.07032500287172f    );

    return c0 + t * (c1 + t * (c2 + t * (c3 + t * (c4 + t * (c5 + t * c6)))));
}

uint32_t gradient_inferno_u32( const float  t )
{
    return gradient_inferno( base::fract(t) ).bgrU32();
}
uint32_t gradient_inferno_cycled_u32( const float  t )
{
    return gradient_inferno( tricycle(t) ).bgrU32();
}

// ---------------------------------------------------------------------------------------------------------------------
// 
col3 gradient_viridis( const float  t )
{
    static constexpr col3 c0(  0.2777273272234177f,  0.005407344544966578f,  0.3340998053353061f  );
    static constexpr col3 c1(  0.1050930431085774f,  1.404613529898575f,     1.384590162594685f   );
    static constexpr col3 c2( -0.3308618287255563f,  0.214847559468213f,     0.09509516302823659f );
    static constexpr col3 c3( -4.634230498983486f,  -5.799100973351585f,   -19.33244095627987f    );
    static constexpr col3 c4(  6.228269936347081f,  14.17993336680509f,     56.69055260068105f    );
    static constexpr col3 c5(  4.776384997670288f, -13.74514537774601f,    -65.35303263337234f    );
    static constexpr col3 c6( -5.435455855934631f,   4.645852612178535f,    26.3124352495832f     );

    return ( c0 + t * (c1 + t * (c2 + t * (c3 + t * (c4 + t * (c5 + t * c6))))) );
}

uint32_t gradient_viridis_u32( const float  t )
{
    return gradient_viridis( base::fract(t) ).bgrU32();
}
uint32_t gradient_viridis_cycled_u32( const float  t )
{
    return gradient_viridis( tricycle(t) ).bgrU32();
}

// ---------------------------------------------------------------------------------------------------------------------

uint32_t gradient_rainbow_ultra_u32( const float  t )
{
    return cosineGradientRainbow( base::fract(t) ).bgrU32();
}

uint32_t gradient_blueorange_u32( const float  t )
{
    return cosineGradientOrangeBlue( 1.0f - base::fract(t) ).bgrU32();
}

uint32_t gradient_redblue_u32( const float  t )
{
    const float v = base::fract(t);
    const col3 v0( v, 0, 1.0f - v );

    return v0.bgrU32();
}


// ---------------------------------------------------------------------------------------------------------------------
// https://www.shadertoy.com/view/3dByzK
//
col3 gradient_rainbow( const float  t )
{
    const float cs1_x = std::cos( 1.0f * t * 6.283185307179586f );
    const float cs1_y = std::sin( 1.0f * t * 6.283185307179586f );
    const float cs2_x = std::cos( 2.0f * t * 6.283185307179586f );
    const float cs2_y = std::sin( 2.0f * t * 6.283185307179586f );


    static constexpr col3 v0(  0.4499723076135584f,  0.609426607533859f,   0.4288887656855271f   );
    static constexpr col3 v1(  1.356958715978088f,  -0.6223492834243289f, -0.2455749072607498f   );
    static constexpr col3 v2( -0.2058207830435541f,  0.4820199928163624f, -0.8671833275731918f   );
    static constexpr col3 v3( -0.2862391770341579f,  0.05434355975084173f, 0.4356605836093163f   );
    static constexpr col3 v4( -0.5279746052180661f, -0.1719661920391326f, -0.2836479508431202f   );


    static constexpr col3 r0(  1.0f,                 1.0f,                 1.0f                  );
    static constexpr col3 r1(  0.7153502777378531f, -0.9219020024148035f, -0.2910109117528888f   );
    static constexpr col3 r2( -0.3381133734426577f,  0.7903005432576772f, -0.3172368432843283f   );
    static constexpr col3 r3( -0.2639897520454177f,  0.06346650999193199f, 0.5784878335459051f   );
    static constexpr col3 r4( -0.3066564712099898f, -0.2161956627263612f, -0.2188031991633754f   );

    col3 n = v0 + ( v1 * cs1_x ) + 
                            ( v2 * cs1_y ) + 
                            ( v3 * cs2_x ) + 
                            ( v4 * cs2_y );
    col3 d = r0 + ( r1 * cs1_x ) + 
                            ( r2 * cs1_y ) + 
                            ( r3 * cs2_x ) + 
                            ( r4 * cs2_y );

    return n/d;
}

uint32_t gradient_rainbow_u32( const float  t )
{
    return gradient_rainbow( t ).bgrU32();
}


// ---------------------------------------------------------------------------------------------------------------------
// https://www.shadertoy.com/view/3dByzK
//
col3 gradient_pastel( const float  t )
{
    const float cs1_x = std::cos( 1.0f * t * 6.283185307179586f );
    const float cs1_y = std::sin( 1.0f * t * 6.283185307179586f );
    const float cs2_x = std::cos( 2.0f * t * 6.283185307179586f );
    const float cs2_y = std::sin( 2.0f * t * 6.283185307179586f );


    static constexpr col3 v0(  0.6698260593431076f,   0.5741866585654442f,    0.6074284221076398f    );
    static constexpr col3 v1(  0.8263467981452095f,  -0.2020418406514409f,   -0.06579882943043414f   );
    static constexpr col3 v2(  0.1278739528808921f,   0.2055033211946446f,   -0.4912381708231384f    );
    static constexpr col3 v3(  0.1580550606222037f,  -0.002035314944658304f, -0.03683228443676777f   );
    static constexpr col3 v4(  0.05270503006417088f, -0.01747401166680522f,   0.01104082480565293f   );


    static constexpr col3 r0(  1.0f,                  1.0f,                   1.0f                   );
    static constexpr col3 r1(  1.045501814882021f,   -0.227676649398989f,    -0.07644732041559271f   );
    static constexpr col3 r2(  0.2273682478445634f,   0.3236347287584649f,   -0.4813798195577666f    );
    static constexpr col3 r3(  0.0975546708844913f,  -0.009724725764886694f,  0.004645741124919443f  );
    static constexpr col3 r4(  0.06757418348606457f, -0.01672661576390289f,   0.002341600573231208f  );

    col3 n = v0 + ( v1 * cs1_x ) + ( v2 * cs1_y ) + ( v3 * cs2_x ) + ( v4 * cs2_y );
    col3 d = r0 + ( r1 * cs1_x ) + ( r2 * cs1_y ) + ( r3 * cs2_x ) + ( r4 * cs2_y );

    return n/d;
}

uint32_t gradient_pastel_u32( const float  t )
{
    return gradient_rainbow( t ).bgrU32();
}

} // namespace colour