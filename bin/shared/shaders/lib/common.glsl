
uniform vec2        iViewportUV;
uniform vec2        iViewportOffset;

const float PI          = 3.1415926535897932384626433832795;
const float TAU         = PI * 2.0;
const float HALF_PI     = PI * 0.5;
const float RCP_PI      = 1.0 / PI;
const float RCP_TAU     = 1.0 / TAU;

const float PHI         = sqrt(5.0) * 0.5 + 0.5;
const float RCP_LOG2    = 1.0 / log(2.0);


const vec3 lumCoeff     = vec3(0.2125, 0.7154, 0.0721);


#define saturate(x)     clamp(x, 0.0, 1.0)


#define vecmax(a)       max(max(a.x, a.y), a.z)
#define vecmin(a)       min(min(a.x, a.y), a.z)


vec2 inputBufferUVMap( vec2 inUV )
{
    return iViewportOffset + (inUV * iViewportUV);
}

vec2 sincos(float x)
{
    return vec2( sin(x), cos(x) );
}

float remap( float value, const float fromMin, const float fromMax, const float toMin, const float toMax)
{
    return (((value - fromMin) / (fromMax - fromMin)) * (toMax - toMin)) + toMin;
}


//https://www.shadertoy.com/view/4sXSWs strength= 16.0
float filmGrain( vec2 uv, float strength, float timer )
{
    float x = (uv.x + 4.0 ) * (uv.y + 4.0 ) * ((mod(timer, 800.0) + 10.0) * 10.0);
    return  (mod((mod(x, 13.0) + 1.0) * (mod(x, 123.0) + 1.0), 0.01)-0.005) * strength;
}

float InterleavedGradientNoise( vec2 uv )
{
    const vec3 magic = vec3( 0.06711056, 0.00583715, 52.9829189 );
    return fract( magic.z * fract( dot( uv, magic.xy ) ) );
}

vec3 cosineGradient(float x, vec3 phase, vec3 amp, vec3 freq, vec3 offset)
{
  phase *= TAU;
  x *= TAU;

  return vec3(offset + amp * 0.5 * cos(x * freq + phase) + 0.5);
}

vec3 pal( in float t, in vec3 a, in vec3 b, in vec3 c, in vec3 d )
{
    return a + b * cos( TAU * ( c * t + d ) );
}

vec3 spectrum(float n)
{
    return pal( n, vec3(0.5,0.5,0.5),vec3(0.5,0.5,0.5),vec3(1.0,1.0,1.0),vec3(0.0,0.33,0.67) );
}

// https://www.shadertoy.com/view/lstyzs
vec2 lensDistort(vec2 c, float factor)
{
    // [0;1] -> [-1;1]
    c = (c - 0.5) * 2.0;
    // [-1;1] -> film frame size
    c.y *= 3.0/4.0;
    // distort
    c /= 1.0 + dot(c, c) * -factor + 1.6 * factor;
    // film frame size -> [-1;1]
    c.y *= 4.0/3.0;
    // [-1;1] -> [0;1]
    c = c * 0.5 + 0.5;
    return c;
}
