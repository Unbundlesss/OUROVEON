
#include "lib/common.glsl"
#include "lib/colourspace.glsl"


// https://www.shadertoy.com/view/3tfcD8



uniform vec2        iResolution;
uniform vec4        iBeat;
uniform float       iTime;
uniform sampler2D   iAudio;
uniform sampler2D   iInputBufferA;

in vec2 oUV;

layout (location = 0) out vec4 glOutColour;

// --------------------------------------------------------------------------------------------
vec3 ACESFilm(vec3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return (x*(a*x+b))/(x*(c*x+d)+e);
}

vec3 flares(sampler2D tex, vec2 uv, float threshold, float intensity, float stretch, float brightness)
{
    threshold = 1.0 - threshold;
    
    vec3 hdr = texture(tex, inputBufferUVMap(uv) ).rgb;
    hdr = vec3(floor(threshold+pow(hdr.r, 1.0)));
    
    float d = intensity; //200.;
    float c = intensity*stretch; //100.;
    
    
    //horizontal
    for (float i=c; i>-1.0; i--)
    {
        float texL = texture(tex, inputBufferUVMap( uv+vec2(i/d, 0.0) ) ).r;
        float texR = texture(tex, inputBufferUVMap( uv-vec2(i/d, 0.0) ) ).r;
        hdr += floor(threshold+pow(max(texL,texR), 4.0))*(1.0-i/c);
    }
    
    //vertical
    for (float i=c/2.0; i>-1.0; i--)
    {
        float texU = texture(tex, inputBufferUVMap( uv+vec2(0.0, i/d) ) ).r;
        float texD = texture(tex, inputBufferUVMap( uv-vec2(0.0, i/d) ) ).r;
        hdr += floor(threshold+pow(max(texU,texD), 40.0))*(1.0-i/c) * 0.25;
    }
    
    hdr *= vec3(0.5,0.4,1.0); //tint
    
	return hdr*brightness;
}


// --------------------------------------------------------------------------------------------
void main(void)
{
    vec2 distFromCenter = oUV - 0.5;
    vec2 aberrated = (0.001 - iBeat.r * 0.04) * distFromCenter;


    vec3 glitched = vec3
    (
        texture(iInputBufferA, inputBufferUVMap(oUV - aberrated)).r,
        texture(iInputBufferA, inputBufferUVMap(oUV) ).g,
        texture(iInputBufferA, inputBufferUVMap(oUV + aberrated)).b
    );

    glitched += filmGrain( oUV, 14.0, iTime );
    glitched += flares(iInputBufferA, oUV, 0.99, 550.0, 0.9, 0.02);
    
    glOutColour = vec4(ACESFilm(glitched), 1.0);
}
