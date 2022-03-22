
#include "common.isph"


// ---------------------------------------------------------------------------------------------------------------------
// 
export void downmix_8channel_stereo( 
    uniform const float  global_volume,
    uniform const int    sample_count,
    uniform float        input_left_channel_0[],
    uniform float        input_left_channel_1[],
    uniform float        input_left_channel_2[],
    uniform float        input_left_channel_3[],
    uniform float        input_left_channel_4[],
    uniform float        input_left_channel_5[],
    uniform float        input_left_channel_6[],
    uniform float        input_left_channel_7[],
    uniform float        input_right_channel_0[],
    uniform float        input_right_channel_1[],
    uniform float        input_right_channel_2[],
    uniform float        input_right_channel_3[],
    uniform float        input_right_channel_4[],
    uniform float        input_right_channel_5[],
    uniform float        input_right_channel_6[],
    uniform float        input_right_channel_7[],
    uniform float        output_left[],
    uniform float        output_right[]
    )
{
    foreach (i = 0 ... sample_count)
    {
        output_left[i]  = ( input_left_channel_0[i] + 
                            input_left_channel_1[i] + 
                            input_left_channel_2[i] + 
                            input_left_channel_3[i] + 
                            input_left_channel_4[i] + 
                            input_left_channel_5[i] + 
                            input_left_channel_6[i] + 
                            input_left_channel_7[i] ) * global_volume;

        output_right[i] = ( input_right_channel_0[i] + 
                            input_right_channel_1[i] + 
                            input_right_channel_2[i] + 
                            input_right_channel_3[i] + 
                            input_right_channel_4[i] + 
                            input_right_channel_5[i] + 
                            input_right_channel_6[i] + 
                            input_right_channel_7[i] ) * global_volume;
    }
}


// ---------------------------------------------------------------------------------------------------------------------
// convert two channels of float samples, clamp to 0..1, convert to 24-bit int, store interleaved in a 32-bit int output stream
//
export void interleave_float_to_int24(
    uniform const int    sample_count,
    uniform float        input_left[],
    uniform float        input_right[],
    uniform int          output_int24_stride32[]
)
{
    uniform float fScaler24 = (float)0x7fffffL;
    uniform int fInt24Max   = (  0x7fffffL );
    uniform int fInt24Min   = ( -fInt24Max - 1 );

    foreach (i = 0 ... sample_count)
    {
        output_int24_stride32[( i * 2 ) + 0] = clamp( (int32)(input_left[i]  * fScaler24), fInt24Min, fInt24Max );
        output_int24_stride32[( i * 2 ) + 1] = clamp( (int32)(input_right[i] * fScaler24), fInt24Min, fInt24Max );
    }
}