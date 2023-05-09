
#include "lib/all.glsl"



vec2 Warp(vec2 pos, vec2 warpT)
{
    pos  = pos*2.0-1.0;    
    pos *= vec2(1.0 + (pos.y*pos.y)*warpT.x, 1.0 + (pos.x*pos.x)*warpT.y);
    
    return pos*0.5 + 0.5;
}


mat2 rot(float a)
{
    return mat2( cos(a), -sin(a),
                 sin(a),  cos(a) );
}


    
//convert HSV to RGB
vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

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

vec3 ScreenSpaceDither( vec2 vScreenPos )
{
    vec3 vDither = vec3( dot( vec2( 171.0, 231.0 ), vScreenPos.xy ) );
    vDither.rgb = fract( vDither.rgb / vec3( 103.0, 71.0, 97.0 ) );

    return vDither.rgb / 255.0;
}

const float dots = 64.; //number of lights
const float radius = .22; //radius of light ring
const float brightness = 0.05;

void mainImage( out vec4 fragColor, in vec2 uv, in vec2 uv2 ) 
{
    vec3 c = vec3( 0.8, 0.9, 1.1 );


    for(float i=0.;i<dots; i++)
    {
        vec4 audi = texture(iAudio, vec2(i/dots, 0.0));


        {
            float vol = pow(audi.r+ (iBeat.r * 0.02), 4.0);
            float b = vol * brightness ;
            
            //get location of dot
            float x = radius*cos( (2.*3.14*float(i)/dots) );
            float y = radius*sin( (2.*3.14*float(i)/dots) );
            vec2 o = vec2(x,y);
            
            //get color of dot based on its index in the 
            //circle + time to rotate colors
            vec3 dotCol = hsv2rgb(vec3(
                (i + iTime * 1.),
                0.5 + (audi.b * 0.5),
                1.0));
            
            //get brightness of this pixel based on distance to dot
            c -= smoothstep( 0.0, mix(1.0, 0.7, audi.b), b/( length(uv2-o) ) ) * dotCol;
        }
    }
    for(float xx=1.;xx<66.; xx+=4.)
    for(float i=0.;i<8.; i++)
    {
        vec4 audi = texture(iAudio, vec2(i/8, 0.00390625 * xx));

        vec2 xUV = ( (Warp(uv, vec2(sin(iTime - xx * 0.56), cos(iTime + xx * -0.85)) * 0.5 ) - 0.5) * iResolution.xy ) / min( iResolution.x, iResolution.y );

        {
            float rt = (iTime * (0.2 + (i * -0.03))) + (-0.24 * xx) + (i * 0.78539816339744830961566);

            vec2 r_xy = vec2( sin(rt), cos(rt) );
            float cxr = drawline(xUV, r_xy * 0.18* audi.g, r_xy * 0.9 * audi.g , 0.012 + (audi.b * 0.005));

            c = mix(c, vec3(0.0,0.0,0.0), cxr);
        }
    }

#ifdef FILMGRAIN

    // black circle overlay
    float dist = distance(uv2 , vec2(0));
    c = c * smoothstep(0.23, 0.235, dist);
    
#endif 

/*
    for(float i = 0.0; i <= 1.0; i+= 0.03125)
    {
        vec3 scope = texture(iAudio, vec2(0.125, 1.0 - (i / 8.)) ).rgb;

        float rt = 3.14 + (i * 6.28);

        float x = sin(rt) * 0.1;
        float y = cos(rt) * 0.1;

        float dst = 0.1 + ( scope.b * 0.1 ) * (i);

        vec2 b = vec2(sin(rt) * dst, cos(rt) * dst);

        float cxr = drawline(uv2, vec2(x, y), b, 0.01);

        c = mix(c, vec3(1.0,0.0,0.0), cxr);
    }
*/

    c = clamp(c, 0, 1);

    fragColor = vec4(c,1);
}
vec3 jodieReinhardTonemap(vec3 c){
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    vec3 tc = c / (c + 1.0);

    return mix(c / (l + 1.0), tc, tc);
}

vec3 GammaEncode(vec3 x) {return pow(x, vec3(1.0 / 2.2));}

vec2 normalizeScreenCoords(vec2 screenCoord)
{
    vec2 result = 2.0 * (screenCoord/iResolution.xy - 0.5);
    result.x *= iResolution.x/iResolution.y; // Correct for aspect ratio
    return result;
}

// --------------------------------------------------------------------------------------------
void main(void)
{
    vec2 inputUV = oUV;

    vec2 cUV = ( (inputUV - 0.5) * iResolution.xy ) / min( iResolution.x, iResolution.y );

    vec4 resultColour;
    mainImage( resultColour, oUV, cUV );

    vec3 audi = texture(iAudio, vec2(0.125, 0.0)).rgb;
    vec3 audi2 = texture(iAudio, vec2(0.250, 0.0)).rgb;

    {
        vec2 rOV = inputUV - 0.5;
        rOV *= rot(sin(iTime * 0.5) * mix( -0.015, 0.015, audi2.b * audi2.b * audi2.b )) * mix( 0.94, 1.02, audi.b * audi.b * audi.b );
        rOV += 0.5;
        
        vec3 prev = texture( iInputBufferA, iViewportOffset + (rOV * iViewportUV) ).rgb;

        resultColour.rgb += pow(prev, vec3(1.4)) * 0.98;
    }

        resultColour.rgb *= sin(gl_FragCoord.y * 30. + iTime) * 0.01 + 1.0;
        resultColour.rgb *= sin(gl_FragCoord.x * 300. + iTime) * 0.05 + 1.0;



    //col *= pow( 16.0*q.x*q.y*(1.0-q.x)*(1.0-q.y), 0.1)*0.35+0.65; //Vign
    
    vec2 vUV = oUV * ( 1.0 - oUV.yx );
    float vig = vUV.x * vUV.y * 20.0;
    vig = pow(vig, 0.2);

    //resultColour.rgb = jodieReinhardTonemap(resultColour.rgb);

    resultColour.rgb = RGBtoYIQ( resultColour.rgb );
    resultColour.r *= vig;
    resultColour.rgb = YIQtoRGB( resultColour.rgb );

    glOutColour = resultColour ;
    //glOutColour = vec4(0);
}
