// import vibes standard library of glsl tools and declarations
#include "lib/all.glsl"


// --------------------------------------------------------------------------------------------

void main(void)
{
    vec2 inputUV = oUV;

    vec3 resultColour = vec3(inputUV.x, inputUV.y, 0.0);

    glOutColour = vec4( resultColour, 1.0 );
}
