// import vibes standard library of glsl tools and declarations
#include "lib/all.glsl"


// --------------------------------------------------------------------------------------------

float line(vec2 p, vec2 p0, vec2 p1, float width)
{
    vec2 dir0 = p1 - p0;
    vec2 dir1 = p - p0;
    float h = clamp(dot(dir1, dir0)/dot(dir0, dir0), 0.0, 1.0);
    float d = (length(dir1 - dir0 * h) - width * 0.5);
    return d;
}

float drawline(vec2 p, vec2 p0, vec2 p1, float width)
{
    float d = line(p, p0, p1, width);
    float w = fwidth(d) * 1.0;
    
    return 1.-smoothstep(-w, w, d);
}

vec2 normalizeScreenCoords(vec2 screenCoord)
{
    vec2 result = 2.0 * (screenCoord/iResolution.xy - 0.5);
    result.x *= iResolution.x/iResolution.y; // Correct for aspect ratio
    return result;
}

// --------------------------------------------------------------------------------------------

void main(void)
{
    vec2 uv = oUV;
    float aspect = iResolution.x / iResolution.y;


    vec3 resultColour = vec3(0, 0, 0);

    // for these debug modes, we just want to visualise each channel in the audio texture; 
    // this is a 8 x N texture that captures N previous frames of 8 stems worth of layer data
#if defined(PULSE)
    resultColour = texture(iAudio, uv).rrr;

#elif defined(ENERGY)
    resultColour = texture(iAudio, uv).ggg;

#elif defined(SCOPE)
    resultColour = texture(iAudio, uv).bbb;

#elif defined(GAIN)
    resultColour = texture(iAudio, uv).aaa;

#elif defined(BEAT)

    float shortedViewEdge = min( iResolution.x, iResolution.y );
    vec2 cUV = ( uv * iResolution.xy ) / shortedViewEdge;
    float xl = ( 1.0 * iResolution.x ) / shortedViewEdge;

    vec3 unlitBeatColour = mix( vec3(0.1, 0.12, 0.1), vec3(0.3, 0.4, 0.8), iBeat.x );

    float barWidthInUV = 0.1;
    float uPerSeg = (xl-barWidthInUV) / iBeat.y;
    for ( float i = 0; i < iBeat.y; i += 1.0f )
    {
        float x1 = ( i * uPerSeg ) + barWidthInUV;
        float x2 = ( x1 + uPerSeg ) - barWidthInUV;
        vec3 beatColour = mix( unlitBeatColour, vec3(1.0, 1.0, 1.0), 1.0f - saturate( distance(i, iBeat.z) ) );

        resultColour += drawline( cUV, vec2(x1, 0.5), vec2(x2, 0.5), barWidthInUV ) * beatColour;
    }

    float riffProgressY = 0.5 + barWidthInUV * 2.0;
    resultColour += drawline( cUV, vec2(0, riffProgressY), vec2(xl, riffProgressY), barWidthInUV * 0.2 ) * vec3(0.1, 0.2, 0.1);
    resultColour += drawline( cUV, vec2(0, riffProgressY), vec2(iBeat.a * xl, riffProgressY), barWidthInUV * 0.1 ) * vec3(0.5, 1.0, 0.25);

#elif defined(UV)

    resultColour = spectrum(inputUV.x);

#endif 

    glOutColour = vec4( resultColour, 1.0 );
}
