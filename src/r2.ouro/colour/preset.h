//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "base/float.util.h"

namespace colour {

// ---------------------------------------------------------------------------------------------------------------------
// handy constexpr way to build a multi-tone colour from hex digit inputs (as those are most often found on colour palette websites)
struct Preset
{
    constexpr Preset() = delete;
    constexpr Preset( const std::string_view light, const std::string_view& neutral, const std::string_view& dark )
        : m_light( fromHex( light ) )
        , m_lightU32( float4ToU32( fromHex( light ) ) )
        , m_neutral( fromHex( neutral ) )
        , m_neutralU32( float4ToU32( fromHex( neutral ) ) )
        , m_dark( fromHex( dark ) )
        , m_darkU32( float4ToU32( fromHex( dark ) ) )
    {
    }

    ouro_nodiscard constexpr ImVec4 light( float alpha = 1.0f ) const
    {
        ImVec4 v = m_light;
        v.w = alpha;
        return v;
    }
    ouro_nodiscard constexpr uint32_t lightU32() const { return m_lightU32; }

    ouro_nodiscard constexpr ImVec4 neutral( float alpha = 1.0f ) const
    {
        ImVec4 v = m_neutral;
        v.w = alpha;
        return v;
    }
    ouro_nodiscard constexpr uint32_t neutralU32() const { return m_neutralU32; }

    ouro_nodiscard constexpr ImVec4 dark( float alpha = 1.0f ) const
    {
        ImVec4 v = m_dark;
        v.w = alpha;
        return v;
    }
    ouro_nodiscard constexpr uint32_t darkU32() const { return m_darkU32; }

private:

    ouro_nodiscard static constexpr uint32_t float4ToU32( const ImVec4& in )
    {
        uint32_t out;
        out  = ((uint32_t)IM_F32_TO_INT8_UNBOUND( in.x )) << IM_COL32_R_SHIFT;
        out |= ((uint32_t)IM_F32_TO_INT8_UNBOUND( in.y )) << IM_COL32_G_SHIFT;
        out |= ((uint32_t)IM_F32_TO_INT8_UNBOUND( in.z )) << IM_COL32_B_SHIFT;
        out |= ((uint32_t)IM_F32_TO_INT8_UNBOUND( in.w )) << IM_COL32_A_SHIFT;
        return out;
    }

    ouro_nodiscard static constexpr std::size_t fromHexDigit( const char c )
    {
        if ( c >= '0' && c <= '9' )
            return c - '0';
        if ( c >= 'a' && c <= 'f' )
            return c - 'a' + 10;
        if ( c >= 'A' && c <= 'F' )
            return c - 'A' + 10;

        ABSL_ASSERT( 0 );
        return 0;
    }

    ouro_nodiscard static constexpr std::size_t fromHexPair( const char a, const char b )
    {
        return (fromHexDigit( a ) << 4) + fromHexDigit( b );
    }

    ouro_nodiscard static constexpr ImVec4 fromHex( const std::string_view hex )
    {
        ABSL_ASSERT( hex.size() == 6 );
        return ImVec4(
            base::LUT::u8_to_float[fromHexPair( hex[0], hex[1] )],
            base::LUT::u8_to_float[fromHexPair( hex[2], hex[3] )],
            base::LUT::u8_to_float[fromHexPair( hex[4], hex[5] )],
            1.0f
        );
    }

    ImVec4 m_light;
    uint32_t m_lightU32;

    ImVec4 m_neutral;
    uint32_t m_neutralU32;

    ImVec4 m_dark;
    uint32_t m_darkU32;
};

struct shades
{
    // from various https://coolors.co/palettes/trending
    // clang-format off
    static constexpr Preset blue        { "2b75b7", "00408f", "092867" };
    static constexpr Preset blue_gray   { "c2dfe3", "9db4c0", "5c6b73" };
    static constexpr Preset toast       { "e0c092", "b88563", "91554a" };
    static constexpr Preset green       { "9bd24d", "59b341", "35823a" };
    static constexpr Preset sea_green   { "c7f9cc", "80ed99", "57cc99" };
    static constexpr Preset lime        { "bfd200", "aacc00", "80b918" };
    static constexpr Preset orange      { "f4aa41", "e68127", "e25d26" };
    static constexpr Preset pink        { "ffb3c6", "ff8fab", "fb6f92" };
    static constexpr Preset slate       { "9a8c98", "4a4e69", "22223b" };
    static constexpr Preset white       { "e2e2e2", "f2f2f2", "ffffff" };

    static constexpr Preset errors      { "c9184a", "f53923", "800f2f" };
    static constexpr Preset callout     { "467f8e", "56b8d0", "c0faff" };
    // clang-format on
};

} // namespace colour
