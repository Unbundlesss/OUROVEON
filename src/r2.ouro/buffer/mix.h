//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

// serial ports for ISPC code originally written on Win; holding here until we sort x-platform ISPC etc

#pragma once

namespace buffer {

// ---------------------------------------------------------------------------------------------------------------------
// 
constexpr void downmix_8channel_stereo( 
    const float  global_gain,
    const int    sample_count,
    float        input_left_channel_0[],
    float        input_left_channel_1[],
    float        input_left_channel_2[],
    float        input_left_channel_3[],
    float        input_left_channel_4[],
    float        input_left_channel_5[],
    float        input_left_channel_6[],
    float        input_left_channel_7[],
    float        input_right_channel_0[],
    float        input_right_channel_1[],
    float        input_right_channel_2[],
    float        input_right_channel_3[],
    float        input_right_channel_4[],
    float        input_right_channel_5[],
    float        input_right_channel_6[],
    float        input_right_channel_7[],
    float        output_left[],
    float        output_right[]
    )
{
    for ( auto i = 0; i < sample_count; i++ )
    {
        const float sV  = ( input_left_channel_0[i] + 
                            input_left_channel_1[i] + 
                            input_left_channel_2[i] + 
                            input_left_channel_3[i] + 
                            input_left_channel_4[i] + 
                            input_left_channel_5[i] + 
                            input_left_channel_6[i] + 
                            input_left_channel_7[i] ) * global_gain;

        output_left[i] = sV;
    }

    for ( auto i = 0; i < sample_count; i++ )
    {
        const float sV  = ( input_right_channel_0[i] +
                            input_right_channel_1[i] + 
                            input_right_channel_2[i] + 
                            input_right_channel_3[i] + 
                            input_right_channel_4[i] + 
                            input_right_channel_5[i] + 
                            input_right_channel_6[i] + 
                            input_right_channel_7[i] ) * global_gain;

        output_right[i] = sV;
    }
}


// ---------------------------------------------------------------------------------------------------------------------
// convert two channels of float samples, clamp to 0..1, convert to 24-bit int, store interleaved in a 32-bit int output stream
//
constexpr void interleave_float_to_int24(
    const int    sample_count,
    float        input_left[],
    float        input_right[],
    int          output_int24_stride32[]
)
{
    const float fScaler24 = (float)0x7fffffL;
    const int fInt24Max   = (  0x7fffffL );
    const int fInt24Min   = ( -fInt24Max - 1 );

    for ( auto i = 0; i < sample_count; i++ )
    {
        output_int24_stride32[( i * 2 ) + 0] = std::clamp( (int32_t)(input_left[i]  * fScaler24), fInt24Min, fInt24Max );
        output_int24_stride32[( i * 2 ) + 1] = std::clamp( (int32_t)(input_right[i] * fScaler24), fInt24Min, fInt24Max );
    }
}

} // namespace buffer
