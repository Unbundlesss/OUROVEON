// import vibes standard library of glsl tools and declarations
#include "lib/all.glsl"

// goofy christmas treat, hacked from https://www.shadertoy.com/view/4tlXWX by Robotic


float opUnion( float d1, float d2 )
{
    return min(d1,d2);
}

float opSubtraction( float d1, float d2 )
{
    return max(-d1,d2);
}

float opIntersection( float d1, float d2 )
{
    return max(d1,d2);
}

float opXor(float d1, float d2 )
{
    return max(min(d1,d2),-max(d1,d2));
}

// --------------------------------------------------------------------------------------------

float sdCircle( vec2 p, float r )
{
    return length(p) - r;
}

float aastep(float threshold, float value)
{
    float afwidth = length(vec2(dFdx(value), dFdy(value))) * 0.70710678118654757;
    return smoothstep(threshold-afwidth, threshold+afwidth, value);
}

vec3 hash3( float n )
{
    return vec3(-0.5,-0.5,0.0)+fract(sin(vec3(n,n+1.0,n+2.0))*vec3(13.5453123,31.1459123,37.3490423));
}

vec3 noise( in float x )
{
    float p = floor(x);
    float f = fract(x);
    f = f*f*(3.0-2.0*f);
    return mix( hash3(p+0.0), hash3(p+1.0),f);
}

vec4 rgb(in int r, in int g, in int b)
{
    float rf=float(r);
    float gf=float(g);
    float bf=float(b);
    
    return vec4(rf/255.0,gf/255.0,bf/255.0,1.0);
}

vec4 eye(in vec4 c,in vec2 coord,in vec2 pos, in float size)
{
    vec4 e=c;

    vec4 audio2 = texture(iAudio, vec2(0.0, 0.0));

    vec4 col1 = rgb(192,228,80);
    vec4 col2 = rgb(98,193,51);

    col1 = mix(  col1, rgb(145,210,218), audio2.r * audio2.r );
    col2 = mix(  col2, rgb(84,133,139), audio2.r * audio2.r );


    vec2 iris=pos + noise(pos.y*123.0+float(iTime/1.0)).xy / 2.0*size;

    //iris.x += (-0.02 + (audio2.y * 0.02));
    
    vec2 almond=coord;
    
    float opening=0.5+sin(iTime)/2.;
    
    almond=vec2(almond.x,almond.y-sign(pos.y-almond.y)*size*0.9 );
    
    coord-=(vec2(0.5,0.5)-coord)*0.6*size;
    
    
    float almAA = aastep( size * 1.5, length(almond-pos) );
    
    {
        if (length(pos-coord)<size)
        {
            if (length(coord-iris)>0.5*size)
                e=rgb(255,251,237);
            else
            {
                if (length(coord-iris)>(0.25- iBeat.x*0.06)*size)
                {
                    e=mix(col1,col2,(coord.y-iris.y)/size);
                    if (length(coord-iris)>0.3*size
                        && length(coord-iris)<0.45*size)
                    {
                        vec2 rad=coord-iris;

                        float irisEdgeI = floor( (atan(rad.y,rad.x) / 3.14) * iBeat.y);

                        if (irisEdgeI==iBeat.z - iBeat.y || irisEdgeI==iBeat.z)
                            e = e * 1.5;

                        if (mod(irisEdgeI,2.0)==0.0)
                            e = e * 0.9;
                    }
                    
                }
                else
                    e=rgb(44,68,71);
            }
        
        if (length(coord-(pos+vec2(0.5,-0.2)*size))<0.1*size)
            e=vec4(1.0);
        }
        else
        {
            e=rgb(233,191,175);
        }
        
        
        if (length(almond-pos)>size*1.4 && almond.y<=pos.y)
        {
            e=e*0.9;
        }
        
    }
    
    return mix( e, c, almAA );
}


// --------------------------------------------------------------------------------------------

void main(void)
{
    vec2 sampleUVD = lensDistort( oUV, 0.2 + (iBeat.x*0.06) );
    vec2 sampleUV = oUV;

    vec2 aspectAdjustment = ( 1.0 / max( iResolution.x, iResolution.y ) ) * iResolution.xy;

    vec2 xUV = vec2(0.5)  * aspectAdjustment;
    vec2 cUV = sampleUV * aspectAdjustment;
    vec2 cUVD = sampleUVD * aspectAdjustment;
    float aspect = iResolution.x / iResolution.y;


//    float eyeBallL = sdCircle( cUV - vec2(0.34, 0.5)* aspectAdjustment, 0.15 );
//    float eyeBallR = sdCircle( cUV - vec2(0.66, 0.5)* aspectAdjustment, 0.15 );
//    float eyeBalls = opUnion( eyeBallL, eyeBallR );
//    float eyeBallCut = aastep( 0, eyeBalls );

    vec2 centerUV = xUV - cUV;

    float v = ( 3.14 + atan(centerUV.y, centerUV.x) ) / 6.28;


    vec4 audio2 = texture(iAudio, vec2( 0.0, fract( v + (iTime * 0.23) ) ) );
    float punchout = audio2.z * audio2.z * audio2.z * 3.0;

    vec4 audio3 = texture(iAudio, vec2( -0.2 + length(centerUV) * (4.0 - punchout) , fract( v + (iTime * 0.23) )  ));

    v = audio3.x;

    vec3 resultColour = pal(saturate(v), vec3(0.5,0.5,0.5),vec3(0.5,0.5,0.5),vec3(1.0,1.0,0.5),vec3(0.8,0.90,0.30) )  * smoothstep( 0.0, 0.01, audio3.w * v );

    resultColour *= smoothstep( 0.05, 0.051, length(centerUV) );

//    eyeBallL = sdCircle( cUV - vec2(0.34, 0.5) * aspectAdjustment, 0.148 );
//    eyeBallR = sdCircle( cUV - vec2(0.66, 0.5) * aspectAdjustment, 0.148 );
//    eyeBalls = opUnion( eyeBallL, eyeBallR );
//    eyeBallCut = 1.0 - aastep( 0, eyeBalls );
//
//    resultColour += eyeBallCut;


    vec4 result = vec4( resultColour, 1.0 );

    result = eye( result, cUVD, vec2(0.33,0.5) * aspectAdjustment, 0.12);
    result = eye( result, cUVD, vec2(0.67,0.5) * aspectAdjustment, 0.12);

    glOutColour = result;
}
