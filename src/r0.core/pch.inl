//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

// no gods no masters
#define _CRT_SECURE_NO_WARNINGS

// std
#include <array>
#include <bitset>
#include <cassert>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <deque>
#include <fstream>
#include <mutex>
#include <optional>
#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// github.com/HowardHinnant/date
#include "date/date.h"
#include "date/tz.h"

// gl
#include "glad/glad.h"
#undef APIENTRY

// fs
#include <filesystem>
namespace fs = std::filesystem;

// cereal
#include "cereal/cereal.hpp"
#include "cereal/types/array.hpp"
#include "cereal/types/chrono.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/unordered_set.hpp"
#include "cereal/types/unordered_map.hpp"
#include "cereal/archives/json.hpp"

#include "optional_json.hpp"
#include "optional_nvp.hpp"

// nlohmann JSON
#include "nlohmann/json.hpp"

// robinhood
#include "robin_hood.h"

// {fmt}
#include "fmt/core.h"
#include "fmt/color.h"
#include "fmt/chrono.h"
#include "fmt/xchar.h"

// we want the power of {fmt}'s formatting but with a bit of boilerplate to establish common
// colours and output formatting per subsystem; this exposes an arbitrarily coloured blog::<system> and blog::error::<system>
// that standardize on the column prefixes and other details whilst otherwise behaving exactly like fmt::print
namespace blog {
namespace detail {

    template < const fmt::color _fg1, const fmt::color _fg2, typename S, typename... Args, fmt::FMT_ENABLE_IF( fmt::detail::is_string<S>::value )>
    void _printer( const std::string_view& prefix, const S& format_str, const Args&... args )
    {
        constexpr std::string_view midsep = " | ";
        constexpr std::string_view suffix = "\n";

        constexpr fmt::detail::ansi_color_escape<char> foreground1( _fg1, "\x1b[38;2;" );
        constexpr fmt::detail::ansi_color_escape<char> foreground2( _fg2, "\x1b[38;2;" );

        // size of buffer used by ansi_color_escape
        constexpr size_t size_of_text_style = 7u + 3u * 4u + 1u;

        const auto size = fmt::formatted_size( format_str, args... ) + 1;

        // format the input into our first buffer
        static thread_local std::string formatBuffer;
        {
            formatBuffer.resize( size );
            std::fill_n( formatBuffer.begin(), size, '\0' );
            fmt::format_to( &formatBuffer[0], format_str, args... );
        }

        // build the final output by appending components
        static thread_local std::string outputBuffer;
        {
            static constexpr auto reset_color = "\x1b[0m";

            const size_t bufferSize =
                (size_of_text_style * 5) +
                prefix.size() +
                size +
                suffix.size();

            outputBuffer.clear();
            outputBuffer.reserve( bufferSize );

            outputBuffer.append( foreground1.begin(), foreground1.end() );

            outputBuffer += prefix;

            outputBuffer += reset_color;
            outputBuffer += midsep;

            outputBuffer.append( foreground2.begin(), foreground2.end() );

            outputBuffer += formatBuffer;
            outputBuffer += suffix;

            outputBuffer += reset_color;
        }

        std::cout << outputBuffer;
    }
}

#define ADD_BLOG( _name, _colour, _nameBold )                                                                                                                               \
        template <typename S, typename... Args, fmt::FMT_ENABLE_IF( fmt::detail::is_string<S>::value )>                                                                     \
        void _name( const S& format_str, const Args&... args ) { detail::_printer<fmt::color::white, (fmt::color)_colour,S,Args...>( _nameBold, format_str, args...); }     \
        namespace error {                                                                                                                                                   \
        template <typename S, typename... Args, fmt::FMT_ENABLE_IF( fmt::detail::is_string<S>::value )>                                                                     \
        void _name( const S& format_str, const Args&... args ) { detail::_printer<fmt::color::red, fmt::color::orange_red,S,Args...>( _nameBold, format_str, args... ); }   \
        }

ADD_BLOG( core,     0xFD971F,    "CORE" )
ADD_BLOG( gl,       0xb05279,    "  GL" )
ADD_BLOG( app,      0xA6E22E,    " APP" )
ADD_BLOG( perf,     0xAEEF1A,    "PERF" )
ADD_BLOG( cfg,      0xe6a637,    " CFG" )
ADD_BLOG( cache,    0xe6d738,    "  C$" )
ADD_BLOG( api,      0xa2e65a,    " API" )
ADD_BLOG( vst,      0x78DCE8,    " VST" )
ADD_BLOG( discord,  0x885de6,    "DSCD" )

ADD_BLOG( mix,      0x5bcce6,    " MIX" )
ADD_BLOG( jam,      0x5c7ee6,    " JAM" )
ADD_BLOG( riff,     0xe458e9,    "RIFF" )
ADD_BLOG( stem,     0xe65ea9,    "STEM" )

} // namespace blog


// imgui
#define IM_VEC2_CLASS_EXTRA                                                                                         \
        inline ImVec2 operator*( const float rhs ) const { return ImVec2( this->x * rhs, this->y * rhs ); }         \
        inline ImVec2 operator/( const float rhs ) const { return ImVec2( this->x / rhs, this->y / rhs ); }         \
        inline ImVec2 operator+( const ImVec2& rhs ) const { return ImVec2( this->x + rhs.x, this->y + rhs.y ); }   \
        inline ImVec2 operator-( const ImVec2& rhs ) const { return ImVec2( this->x - rhs.x, this->y - rhs.y ); }   \
        inline ImVec2 operator*( const ImVec2& rhs ) const { return ImVec2( this->x * rhs.x, this->y * rhs.y ); }   \
        inline ImVec2 operator/( const ImVec2& rhs ) const { return ImVec2( this->x / rhs.x, this->y / rhs.y ); }   \
        inline ImVec2& operator*=( const float rhs ) { this->x *= rhs; this->y *= rhs; return *this; }              \
        inline ImVec2& operator/=( const float rhs ) { this->x /= rhs; this->y /= rhs; return *this; }              \
        inline ImVec2& operator+=( const ImVec2& rhs ) { this->x += rhs.x; this->y += rhs.y; return *this; }        \
        inline ImVec2& operator-=( const ImVec2& rhs ) { this->x -= rhs.x; this->y -= rhs.y; return *this; }        \
        inline ImVec2& operator*=( const ImVec2& rhs ) { this->x *= rhs.x; this->y *= rhs.y; return *this; }        \
        inline ImVec2& operator/=( const ImVec2& rhs ) { this->x /= rhs.x; this->y /= rhs.y; return *this; }

// imgui
#define IM_VEC4_CLASS_EXTRA                                                                                                                             \
        friend inline ImVec4 operator*( const float lhs, const ImVec4& rhs ) { return ImVec4( rhs.x * lhs, rhs.y * lhs, rhs.z * lhs, rhs.w * lhs ); }    \
        inline ImVec4 operator*( const float rhs ) const { return ImVec4( this->x * rhs, this->y * rhs, this->z * rhs, this->w * rhs ); }               \
        inline ImVec4 operator+( const ImVec4& rhs ) const { return ImVec4( this->x + rhs.x, this->y + rhs.y, this->z + rhs.z, this->w + rhs.w ); }     \
        inline ImVec4 operator*( const ImVec4& rhs ) const { return ImVec4( this->x * rhs.x, this->y * rhs.y, this->z * rhs.z, this->w * rhs.w ); }     \
        inline ImVec4& operator*=( const float rhs ) { this->x *= rhs; this->y *= rhs; this->z *= rhs; this->w *= rhs; return *this; }                  \
        inline ImVec4& operator+=( const ImVec4& rhs ) { this->x += rhs.x; this->y += rhs.y; this->z += rhs.z; this->w += rhs.w; return *this; }        \
        inline ImVec4& operator*=( const ImVec4& rhs ) { this->x *= rhs.x; this->y *= rhs.y; this->z *= rhs.z; this->w *= rhs.w; return *this; }


#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"
#include "imgui_freetype.h"
#include "imgui_stdlib.h"
#include "imbezier.h"
#include "imnodes.h"
#include "implot.h"

// sqlite
#include "sqlite3.h"
#include "SQLiteWrapper.h"

// SDL
#define SDL_MAIN_HANDLED
#include "SDL.h"
#include "SDL_syswm.h"

// etc
#include "utf8.h"

// moodycamel concurrency
#include "readerwriterqueue.h"
#include "concurrentqueue.h"
#include "atomicops.h"
namespace mcc = moodycamel;

// CityHash
#include "city.h"

// STB
#include "stb_image.h"
#include "stb_image_write.h"

// kissFFT
#include "kiss_fft.h"
#include "kiss_fftr.h"

// taskflow
#include "taskflow.hpp"

// base64 codec
#include "base64_rfc4648.hpp"

// r8brain
#include "CDSPResampler.h"

// midi
#include "RtMidi.h"

// http
#include "httplib.h"


namespace constants
{
static constexpr auto d_pi = 3.1415926535897932384626433832795028841971693993751058209749445923;
static constexpr auto f_pi = 3.14159265358979323846264338f;
static constexpr auto d_2pi = 6.2831853071795864769252867665590057683943387987502116419498891846;
static constexpr auto f_2pi = 6.28318530717958647692528676f;
} // namespace constants
