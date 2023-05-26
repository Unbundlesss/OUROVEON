//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

namespace dsp {

    struct OctaveBandFrequencies
    {
        constexpr OctaveBandFrequencies( const double low, const double center, const double high )
            : m_low( low )
            , m_center( center )
            , m_high( high )
        {}
        cycfi::q::frequency m_low;
        cycfi::q::frequency m_center;
        cycfi::q::frequency m_high;
    };

    // convenient storage of the frequency bands corresponding to useful octaves (well, also a pair below 40hz)
    static constexpr std::array OctaveBands = 
    {
        OctaveBandFrequencies(    11.049,    15.625,    22.097 ),
        OctaveBandFrequencies(    22.097,    31.250,    44.194 ),
        OctaveBandFrequencies(    44.194,    62.500,    88.388 ),
        OctaveBandFrequencies(    88.388,   125.000,   176.777 ),
        OctaveBandFrequencies(   176.777,   250.000,   353.553 ),
        OctaveBandFrequencies(   353.553,   500.000,   707.107 ),
        OctaveBandFrequencies(   707.107,  1000.000,  1414.214 ),
        OctaveBandFrequencies(  1414.214,  2000.000,  2828.427 ),
        OctaveBandFrequencies(  2828.427,  4000.000,  5656.854 ),
        OctaveBandFrequencies(  5656.854,  8000.000, 11313.708 ),
        OctaveBandFrequencies( 11313.708, 16000.000, 22627.417 )
    };

} // namespace dsp
