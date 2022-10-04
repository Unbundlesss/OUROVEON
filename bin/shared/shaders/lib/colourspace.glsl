

// https://en.wikipedia.org/wiki/YIQ

vec3 RGBtoYIQ(vec3 RGB)
{
    const mat3 yiqmat = mat3(
        0.2989,  0.5870,  0.1140,
        0.5959, -0.2744, -0.3216,
        0.2115, -0.5229,  0.3114 );

    return RGB * yiqmat;
}

vec3 YIQtoRGB(vec3 YIQ)
{
    const mat3 rgbmat = mat3(
        1.0,  0.9560,  0.6210,
        1.0, -0.2720, -0.6474,
        1.0, -1.1060,  1.7046);
    
    return YIQ * rgbmat;
}
