
#include "common.isph"


// ---------------------------------------------------------------------------------------------------------------------

export uniform uint32 gradient_grayscale_u32( uniform const float  t )
{
    ispc_construct( uniform const float3 c0, _ctf3 { frac(t),frac(t),frac(t) } );

    return bgrFloatToU32( c0 );
}
export uniform uint32 gradient_grayscale_cycled_u32( uniform const float  t )
{
    ispc_construct( uniform const float3 c0, _ctf3 { tricycle(t),tricycle(t),tricycle(t) } );

    return bgrFloatToU32( c0 );
}

// ---------------------------------------------------------------------------------------------------------------------
// 
uniform float3 gradient_plasma( uniform const float  t )
{
    ispc_construct( static uniform const float3 c0, _ctf3 {   0.05873234392399702f, 0.02333670892565664f, 0.5433401826748754f  } );
    ispc_construct( static uniform const float3 c1, _ctf3 {   2.176514634195958f,   0.2383834171260182f,  0.7539604599784036f  } );
    ispc_construct( static uniform const float3 c2, _ctf3 {  -2.689460476458034f,  -7.455851135738909f,   3.110799939717086f   } );
    ispc_construct( static uniform const float3 c3, _ctf3 {   6.130348345893603f,  42.3461881477227f,   -28.51885465332158f    } );
    ispc_construct( static uniform const float3 c4, _ctf3 { -11.10743619062271f,  -82.66631109428045f,   60.13984767418263f    } );
    ispc_construct( static uniform const float3 c5, _ctf3 {  10.02306557647065f,   71.41361770095349f,  -54.07218655560067f    } );
    ispc_construct( static uniform const float3 c6, _ctf3 {  -3.658713842777788f, -22.93153465461149f,   18.19190778539828f    } );

    return c0 + t * (c1 + t * (c2 + t * (c3 + t * (c4 + t * (c5 + t * c6)))));
}

export uniform uint32 gradient_plasma_u32( uniform const float  t )
{
    return bgrFloatToU32( gradient_plasma( frac(t) ) );
}
export uniform uint32 gradient_plasma_cycled_u32( uniform const float  t )
{
    return bgrFloatToU32( gradient_plasma( tricycle(t) ) );
}


// ---------------------------------------------------------------------------------------------------------------------
// 
uniform float3 gradient_turbo( uniform const float  t )
{
    ispc_construct( static uniform const float3 c0, _ctf3 {    0.1140890109226559f, 0.06288340699912215f, 0.2248337216805064f  } );
    ispc_construct( static uniform const float3 c1, _ctf3 {    6.716419496985708f,  3.182286745507602f,   7.571581586103393f   } );
    ispc_construct( static uniform const float3 c2, _ctf3 {  -66.09402360453038f,  -4.9279827041226f,   -10.09439367561635f    } );
    ispc_construct( static uniform const float3 c3, _ctf3 {  228.7660791526501f,   25.04986699771073f,  -91.54105330182436f    } );
    ispc_construct( static uniform const float3 c4, _ctf3 { -334.8351565777451f,  -69.31749712757485f,  288.5858850615712f     } );
    ispc_construct( static uniform const float3 c5, _ctf3 {  218.7637218434795f,   67.52150567819112f, -305.2045772184957f     } );
    ispc_construct( static uniform const float3 c6, _ctf3 {  -52.88903478218835f, -21.54527364654712f,  110.5174647748972f     } );

    return c0 + t * (c1 + t * (c2 + t * (c3 + t * (c4 + t * (c5 + t * c6)))));
}

export uniform uint32 gradient_turbo_u32( uniform const float  t )
{
    return bgrFloatToU32( gradient_turbo( frac(t) ) );
}
export uniform uint32 gradient_turbo_cycled_u32( uniform const float  t )
{
    return bgrFloatToU32( gradient_turbo( tricycle(t) ) );
}

// ---------------------------------------------------------------------------------------------------------------------
// 
uniform float3 gradient_magma( uniform const float  t )
{
    ispc_construct( static uniform const float3 c0, _ctf3 {  -0.002136485053939582f, -0.000749655052795221f, -0.005386127855323933f  } );
    ispc_construct( static uniform const float3 c1, _ctf3 {   0.2516605407371642f,    0.6775232436837668f,    2.494026599312351f     } );
    ispc_construct( static uniform const float3 c2, _ctf3 {   8.353717279216625f,    -3.577719514958484f,     0.3144679030132573f    } );
    ispc_construct( static uniform const float3 c3, _ctf3 { -27.66873308576866f,     14.26473078096533f,    -13.64921318813922f      } );
    ispc_construct( static uniform const float3 c4, _ctf3 {  52.17613981234068f,    -27.94360607168351f,     12.94416944238394f      } );
    ispc_construct( static uniform const float3 c5, _ctf3 { -50.76852536473588f,     29.04658282127291f,      4.23415299384598f      } );
    ispc_construct( static uniform const float3 c6, _ctf3 {  18.65570506591883f,    -11.48977351997711f,     -5.601961508734096f     } );

    return c0 + t * (c1 + t * (c2 + t * (c3 + t * (c4 + t * (c5 + t * c6)))));
}

export uniform uint32 gradient_magma_u32( uniform const float  t )
{
    return bgrFloatToU32( gradient_magma( frac(t) ) );
}
export uniform uint32 gradient_magma_cycled_u32( uniform const float  t )
{
    return bgrFloatToU32( gradient_magma( tricycle(t) ) );
}

// ---------------------------------------------------------------------------------------------------------------------
// 
uniform float3 gradient_inferno( uniform const float  t )
{
    ispc_construct( static uniform const float3 c0, _ctf3 {   0.0002189403691192265f, 0.001651004631001012f, -0.01948089843709184f } );
    ispc_construct( static uniform const float3 c1, _ctf3 {   0.1065134194856116f,    0.5639564367884091f,    3.932712388889277f   } );
    ispc_construct( static uniform const float3 c2, _ctf3 {  11.60249308247187f,     -3.972853965665698f,   -15.9423941062914f     } );
    ispc_construct( static uniform const float3 c3, _ctf3 { -41.70399613139459f,     17.43639888205313f,     44.35414519872813f    } );
    ispc_construct( static uniform const float3 c4, _ctf3 {  77.162935699427f,      -33.40235894210092f,    -81.80730925738993f    } );
    ispc_construct( static uniform const float3 c5, _ctf3 { -71.31942824499214f,     32.62606426397723f,     73.20951985803202f    } );
    ispc_construct( static uniform const float3 c6, _ctf3 {  25.13112622477341f,    -12.24266895238567f,    -23.07032500287172f    } );

    return c0 + t * (c1 + t * (c2 + t * (c3 + t * (c4 + t * (c5 + t * c6)))));
}

export uniform uint32 gradient_inferno_u32( uniform const float  t )
{
    return bgrFloatToU32( gradient_inferno( frac(t) ) );
}
export uniform uint32 gradient_inferno_cycled_u32( uniform const float  t )
{
    return bgrFloatToU32( gradient_inferno( tricycle(t) ) );
}

// ---------------------------------------------------------------------------------------------------------------------
// 
uniform float3 gradient_viridis( uniform const float  t )
{
    ispc_construct( static uniform const float3 c0, _ctf3 {  0.2777273272234177f,  0.005407344544966578f,  0.3340998053353061f  } );
    ispc_construct( static uniform const float3 c1, _ctf3 {  0.1050930431085774f,  1.404613529898575f,     1.384590162594685f   } );
    ispc_construct( static uniform const float3 c2, _ctf3 { -0.3308618287255563f,  0.214847559468213f,     0.09509516302823659f } );
    ispc_construct( static uniform const float3 c3, _ctf3 { -4.634230498983486f,  -5.799100973351585f,   -19.33244095627987f    } );
    ispc_construct( static uniform const float3 c4, _ctf3 {  6.228269936347081f,  14.17993336680509f,     56.69055260068105f    } );
    ispc_construct( static uniform const float3 c5, _ctf3 {  4.776384997670288f, -13.74514537774601f,    -65.35303263337234f    } );
    ispc_construct( static uniform const float3 c6, _ctf3 { -5.435455855934631f,   4.645852612178535f,    26.3124352495832f     } );

    return ( c0 + t * (c1 + t * (c2 + t * (c3 + t * (c4 + t * (c5 + t * c6))))) );
}

export uniform uint32 gradient_viridis_u32( uniform const float  t )
{
    return bgrFloatToU32( gradient_viridis( frac(t) ) );
}
export uniform uint32 gradient_viridis_cycled_u32( uniform const float  t )
{
    return bgrFloatToU32( gradient_viridis( tricycle(t) ) );
}

// ---------------------------------------------------------------------------------------------------------------------

export uniform uint32 gradient_rainbow_ultra_u32( uniform const float  t )
{
    return bgrFloatToU32( cosineGradientRainbow( frac(t) ) );
}

export uniform uint32 gradient_blueorange_u32( uniform const float  t )
{
    return bgrFloatToU32( cosineGradientOrangeBlue( 1.0 - frac(t) ) );
}

export uniform uint32 gradient_redblue_u32( uniform const float  t )
{
    uniform const float v = frac(t);
    ispc_construct( uniform const float3 v0, _ctf3 { v, 0, 1.0 - v } );

    return bgrFloatToU32( v0 );
}


// ---------------------------------------------------------------------------------------------------------------------
// https://www.shadertoy.com/view/3dByzK
//
uniform float3 gradient_rainbow( uniform const float  t )
{
    ispc_construct( uniform const float2 cs1, _ctf2 { cos( 1.0f * t * 6.283185307179586f ), sin( 1.0f * t * 6.283185307179586f ) } );
    ispc_construct( uniform const float2 cs2, _ctf2 { cos( 2.0f * t * 6.283185307179586f ), sin( 2.0f * t * 6.283185307179586f ) } );


    ispc_construct( static uniform const float3 v0, _ctf3 {  0.4499723076135584f,  0.609426607533859f,   0.4288887656855271f   } );
    ispc_construct( static uniform const float3 v1, _ctf3 {  1.356958715978088f,  -0.6223492834243289f, -0.2455749072607498f   } );
    ispc_construct( static uniform const float3 v2, _ctf3 { -0.2058207830435541f,  0.4820199928163624f, -0.8671833275731918f   } );
    ispc_construct( static uniform const float3 v3, _ctf3 { -0.2862391770341579f,  0.05434355975084173f, 0.4356605836093163f   } );
    ispc_construct( static uniform const float3 v4, _ctf3 { -0.5279746052180661f, -0.1719661920391326f, -0.2836479508431202f   } );


    ispc_construct( static uniform const float3 r0, _ctf3 {  1.0f,                 1.0f,                 1.0f                  } );
    ispc_construct( static uniform const float3 r1, _ctf3 {  0.7153502777378531f, -0.9219020024148035f, -0.2910109117528888f   } );
    ispc_construct( static uniform const float3 r2, _ctf3 { -0.3381133734426577f,  0.7903005432576772f, -0.3172368432843283f   } );
    ispc_construct( static uniform const float3 r3, _ctf3 { -0.2639897520454177f,  0.06346650999193199f, 0.5784878335459051f   } );
    ispc_construct( static uniform const float3 r4, _ctf3 { -0.3066564712099898f, -0.2161956627263612f, -0.2188031991633754f   } );

    uniform float3 n = v0 + ( v1 * cs1.x ) + 
                            ( v2 * cs1.y ) + 
                            ( v3 * cs2.x ) + 
                            ( v4 * cs2.y );
    uniform float3 d = r0 + ( r1 * cs1.x ) + 
                            ( r2 * cs1.y ) + 
                            ( r3 * cs2.x ) + 
                            ( r4 * cs2.y );

    return n/d;
}

export uniform uint32 gradient_rainbow_u32( uniform const float  t )
{
    return bgrFloatToU32( gradient_rainbow( t ) );
}


// ---------------------------------------------------------------------------------------------------------------------
// https://www.shadertoy.com/view/3dByzK
//
uniform float3 gradient_pastel( uniform const float  t )
{
    ispc_construct( uniform const float2 cs1, _ctf2 { cos( 1.0 * t * 6.283185307179586f ), sin( 1.0 * t * 6.283185307179586f ) } );
    ispc_construct( uniform const float2 cs2, _ctf2 { cos( 2.0 * t * 6.283185307179586f ), sin( 2.0 * t * 6.283185307179586f ) } );


    ispc_construct( static uniform const float3 v0, _ctf3 {  0.6698260593431076f,   0.5741866585654442f,    0.6074284221076398f    } );
    ispc_construct( static uniform const float3 v1, _ctf3 {  0.8263467981452095f,  -0.2020418406514409f,   -0.06579882943043414f   } );
    ispc_construct( static uniform const float3 v2, _ctf3 {  0.1278739528808921f,   0.2055033211946446f,   -0.4912381708231384f    } );
    ispc_construct( static uniform const float3 v3, _ctf3 {  0.1580550606222037f,  -0.002035314944658304f, -0.03683228443676777f   } );
    ispc_construct( static uniform const float3 v4, _ctf3 {  0.05270503006417088f, -0.01747401166680522f,   0.01104082480565293f   } );


    ispc_construct( static uniform const float3 r0, _ctf3 {  1.0f,                  1.0f,                   1.0f                   } );
    ispc_construct( static uniform const float3 r1, _ctf3 {  1.045501814882021f,   -0.227676649398989f,    -0.07644732041559271f   } );
    ispc_construct( static uniform const float3 r2, _ctf3 {  0.2273682478445634f,   0.3236347287584649f,   -0.4813798195577666f    } );
    ispc_construct( static uniform const float3 r3, _ctf3 {  0.0975546708844913f,  -0.009724725764886694f,  0.004645741124919443f  } );
    ispc_construct( static uniform const float3 r4, _ctf3 {  0.06757418348606457f, -0.01672661576390289f,   0.002341600573231208f  } );

    uniform float3 n = v0 + ( v1 * cs1.x ) + ( v2 * cs1.y ) + ( v3 * cs2.x ) + ( v4 * cs2.y );
    uniform float3 d = r0 + ( r1 * cs1.x ) + ( r2 * cs1.y ) + ( r3 * cs2.x ) + ( r4 * cs2.y );

    return n/d;
}

export uniform uint32 gradient_pastel_u32( uniform const float  t )
{
    return bgrFloatToU32( gradient_rainbow( t ) );
}
