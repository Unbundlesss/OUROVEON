
#include "lib/all.glsl"



// --------------------------------------------------------------------------------------------

// original Starfield Tutorial by Martijn Steinrucken aka BigWings - 2020
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.
// 

#define NUM_LAYERS 4.

mat2 Rot(float a) {
    float s=sin(a), c=cos(a);
    return mat2(c, -s, s, c);
}

float Star(float rtx, vec2 uv, float flare) {
    float d = length(uv);
    float m = .05/d;


    
    uv *= Rot(3.1415/4. + rtx);
    float rays = max(0., 1.-abs(uv.x*uv.y*1000.));
    m += rays*flare;
    uv *= Rot(3.1415/4. - rtx);
    rays = max(0., 1.-abs(uv.x*uv.y*1000.));
    m += rays*.3*flare;
    
    m *= smoothstep(1., .2, d);
    return m;
}

float Hash21(vec2 p) {
    p = fract(p*vec2(123.34, 456.21));
    p += dot(p, p+45.32);
    return fract(p.x*p.y);
}

vec3 StarLayer(vec2 uv) {
    vec3 col = vec3(0);
    
    vec2 gv = fract(uv)-.5;
    vec2 id = floor(uv);
    
    for(int y=-1;y<=1;y++) {
        for(int x=-1;x<=1;x++) {
            vec2 offs = vec2(x, y);
            
            float n = Hash21(id+offs); // random between 0 and 1

            vec4 audi = texture(iAudio, vec2(n, 0.0));

            float size = fract(n*345.32);
            
            float star = Star(n + iTime * 0.2, gv-offs-vec2(n, fract(n*34.))+.5, smoothstep(.9, 1., size)*.6);
            
            vec3 color = mix( vec3(.6, .4, .8), vec3(0.4, 1.2, 2.0), audi.b );
            //color = color*vec3(1,.25,1.+size)+vec3(.2, .2, .1)*2.;

            star = pow( star, 2.0 + audi.b );
            star *= audi.r;//mix( audi.r, audi.g, step( 0.9, n )  );
            col += star*size*color;
        }
    }
    return col;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = fragCoord;
    vec2 M = vec2(sin(iTime*0.07),sin(iTime*-0.093)); // sin(iTime*0.07), sin(iTime*-0.093)
    
    float t = (iTime*.06) - (iBeat.r * 0.004);

    vec3 col = vec3(0);
    
    for(float i=0.; i<1.; i+=1./NUM_LAYERS) {
        float depth = fract(i+t);

        vec4 audi = texture(iAudio, vec2(depth, 0.0));

        
        vec2 layerUV = lensDistort( uv, -0.1 * i );
        layerUV += M * (2.0 + (i * 0.1));
        layerUV *= Rot(t - i);

        float scale = mix(15., 1.0, depth);
        float fade = depth*smoothstep(1., .9, depth);
        col += StarLayer(layerUV*scale+i*453.2-M)*fade;
    }
    
    col = sqrt(col);

    fragColor = vec4( col,1.0);
}

// --------------------------------------------------------------------------------------------
void main(void)
{
    vec2 sampleUV = oUV;
    vec2 cUV = ( (sampleUV) * iResolution.xy ) / max( iResolution.x, iResolution.y );

    vec4 resultColour;
    mainImage( resultColour, cUV );

    vec4 previous = texture(iInputBufferA, iViewportOffset + (oUV * iViewportUV));

    resultColour += smoothstep(0.8, 1.0, previous) * vec4(0.7, 0.1, 0.2, 0);

    glOutColour = resultColour;
}
