
#include "lib/all.glsl"

// --------------------------------------------------------------------------------------------
void main(void)
{
    vec4 inputCol = texture(iInputBufferA, inputBufferUVMap(oUV));

    glOutColour = vec4( inputCol.rgb, 1.0 );
}
