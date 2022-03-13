//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//

#pragma once

#include <string>

#if OURO_PLATFORM_OSX
    #include <mach/mach_time.h>
#else
    #ifdef _MSC_VER
    #include <intrin.h>
    #else
    #include <x86intrin.h>
    #endif
#endif

#ifdef WIN32
#define finline     __forceinline
#else
#define finline     inline
#endif

// ---------------------------------------------------------------------------------------------------------------------
namespace mem {

// allocate numElements of _T aligned to 16 bytes
template< typename _T >
finline _T* malloc16As( const size_t numElements )
{
#ifdef WIN32
    return reinterpret_cast<_T*>(_aligned_malloc( sizeof( _T ) * numElements, 16 ));
#else
    return reinterpret_cast<_T*>(malloc( sizeof( _T ) * numElements ));
#endif
}

// allocate numElements of _T aligned to 16 bytes
template< typename _T >
finline _T* malloc16AsSet( const size_t numElements, const _T defaultValue )
{
#ifdef WIN32
    _T* mblock = reinterpret_cast<_T*>(_aligned_malloc( sizeof( _T ) * numElements, 16 ));
#else
    _T* mblock = reinterpret_cast<_T*>(malloc( sizeof( _T ) * numElements ));
#endif
    for ( size_t kI = 0; kI < numElements; kI++ )
        mblock[kI] = defaultValue;

    return mblock;
}

// free memory allocated with allocateAlign16
finline void free16( void* ptr )
{
#ifdef WIN32
    return _aligned_free( ptr );
#else
    return free( ptr );
#endif
}

} // namespace mem

namespace base {

// ---------------------------------------------------------------------------------------------------------------------
template< typename TCmdEnum > 
struct BasicCommandType
{
    constexpr BasicCommandType()
        : m_command( TCmdEnum::Invalid )
        , m_i64( 0 )
        , m_ptr( nullptr )
    {}
    constexpr BasicCommandType( const TCmdEnum command )
        : m_command( command )
        , m_i64( 0 )
        , m_ptr( nullptr )
    {}
    explicit constexpr BasicCommandType( const TCmdEnum command, void* ptr )
        : m_command( command )
        , m_i64( 0 )
        , m_ptr( ptr )
    {}
    explicit constexpr BasicCommandType( const TCmdEnum command, const int64_t i64 )
        : m_command( command )
        , m_i64( i64 )
        , m_ptr( nullptr )
    {}

    constexpr TCmdEnum getCommand() const { return m_command; }
    constexpr int64_t getI64() const { return m_i64; }
    constexpr void* getPtr() const { return m_ptr; }
    
    template< typename _CType >
    constexpr _CType* getPtrAs() const { return static_cast<_CType*>(m_ptr); }

private:

    TCmdEnum    m_command;
    int64_t     m_i64;
    void*       m_ptr;
};

// ---------------------------------------------------------------------------------------------------------------------
constexpr uint32_t nextPow2( uint32_t v )
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

// ---------------------------------------------------------------------------------------------------------------------
inline float oscSine( const float phase )
{
    return std::sin( phase * constants::f_2pi );
}

// ---------------------------------------------------------------------------------------------------------------------
constexpr float oscTriangle( const float phase )
{
    float rV;

    if ( phase <= 0.5f )
        rV = phase * 2.0f;
    else
        rV = (1.0f - phase) * 2.0f;

    return (rV * 2.0f) - 1.0f;
}

// ---------------------------------------------------------------------------------------------------------------------
constexpr float oscSquare( const float phase )
{
    if ( phase <= 0.5f )
        return 1.0f;
    else
        return -1.0f;
}

// ---------------------------------------------------------------------------------------------------------------------
constexpr float oscSawtooth( const float phase )
{
    return ((phase * 2.0f) - 1.0f) * -1.0f;
}

// ---------------------------------------------------------------------------------------------------------------------
inline float fract( const float x )
{
    return x - std::floor( x );
}

// ---------------------------------------------------------------------------------------------------------------------
// wrap x -> [0,max)
finline float wrapMax( const float x, const float max )
{
    // (max + x % max) % max
    return std::fmod( max + std::fmod( x, max ), max );
}

// ---------------------------------------------------------------------------------------------------------------------
// wrap x -> [min,max) 
finline float wrapMinMax( const float x, const float min, const float max )
{
    return min + wrapMax( x - min, max - min );
}

// ---------------------------------------------------------------------------------------------------------------------
finline float hardClip( const float s )
{
    return std::clamp( s, -1.0f, 1.0f );
}

// ---------------------------------------------------------------------------------------------------------------------
finline float softClip( const float s )
{
    float r = std::clamp( s, -1.0f, 1.0f );
    return 1.5f * r - 0.5f * r * r * r;
}

// ---------------------------------------------------------------------------------------------------------------------
template< typename _T >
finline _T lerp( const _T& a, const _T& b, const float s )
{
    return a + ((b - a) * s);
}

// ---------------------------------------------------------------------------------------------------------------------
finline float smoothstep( const float edge0, const float edge1, const float f )
{
    const float t = std::clamp( (f - edge0) / (edge1 - edge0), 0.0f, 1.0f );
    return t * t * (3.0f - 2.0f * t);
}

// ---------------------------------------------------------------------------------------------------------------------
finline bool floatAlmostEqualRelative( float A, float B, float maxRelDiff = FLT_EPSILON )
{
    // calculate the difference.
    const float diff = std::abs( A - B );
    A = std::abs( A );
    B = std::abs( B );

    // find the largest
    const float largest = (B > A) ? B : A;

    if ( diff <= largest * maxRelDiff )
        return true;
    return false;
}


// ---------------------------------------------------------------------------------------------------------------------
finline uint32_t mixU32( uint32_t value, const uint32_t key = 0x85ebca6b )
{
    value  = (value ^ 61) ^ (value >> 16);
    value += (value << 3);
    value ^= (value >> 4);
    value *= key;
    value ^= (value >> 15);
    return value;
}

// ---------------------------------------------------------------------------------------------------------------------
finline uint64_t expand32To64( const uint32_t value, const uint32_t key = 0xcc9e2d51 )
{
    uint32_t lower32 = mixU32( value, key );
    uint32_t upper32 = mixU32( key, value );

    return ( (uint64_t)lower32) | ((uint64_t)upper32 << 32 );
}

// ---------------------------------------------------------------------------------------------------------------------
finline uint32_t reduce64To32( const uint64_t value, const uint32_t key = 0x1b873593 )
{
    uint32_t hash = key, tmp;

    hash += (uint32_t)(value & 0xFFFF);
    tmp   = ((uint32_t)(value & 0xFFFF0000) >> 5) ^ hash;
    hash  = (hash << 16) ^ tmp;
    hash += hash >> 11;

    hash += (uint32_t)((value >> 32) & 0xFFFF);
    tmp   = (((uint32_t)(value >> 32) & 0xFFFF0000) >> 5) ^ hash;
    hash  = (hash << 16) ^ tmp;
    hash += hash >> 11;

    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 4;
    hash += hash >> 17;
    hash ^= hash << 25;
    hash += hash >> 6;

    return hash;
}

// ---------------------------------------------------------------------------------------------------------------------
finline uint64_t crush64( const uint64_t value1, const uint64_t value2, const uint64_t key = 0x9ddfea08eb382d69UL )
{
    // Murmur-inspired hashing
    uint64_t a = (value2 ^ value1) * key;
             a ^= (a >> 47);
    uint64_t b = (value1 ^ a) * key;
             b ^= (b >> 47);
             b *= key;
    return b;
}

// ---------------------------------------------------------------------------------------------------------------------
// mix a RDTSC value to produce a random seed
finline uint64_t randomU64()
{
    #if OURO_PLATFORM_OSX
    uint64_t value = mach_absolute_time();
    #else
    uint64_t value = __rdtsc();
    #endif
             value ^= value >> 33;
             value *= 0x64dd81482cbd31d7UL;
             value ^= value >> 33;
             value *= 0xe36aa5c613612997UL;
             value ^= value >> 33;

    return value;
}

finline uint64_t randomU32()
{
    return reduce64To32( randomU64() );
}

// ---------------------------------------------------------------------------------------------------------------------
finline void trimRight( std::string& str, const std::string& trimChars )
{
   std::string::size_type pos = str.find_last_not_of( trimChars );
   str.erase( pos + 1 );
}


// ---------------------------------------------------------------------------------------------------------------------
finline void trimLeft( std::string& str, const std::string& trimChars )
{
   std::string::size_type pos = str.find_first_not_of( trimChars );
   str.erase( 0, pos );
}


// ---------------------------------------------------------------------------------------------------------------------
finline void trim( std::string& str, const std::string& trimChars )
{
   trimRight( str, trimChars );
   trimLeft( str, trimChars );
} 

// ---------------------------------------------------------------------------------------------------------------------
finline void asciifyString( const std::string& source, std::string& dest, const char compressionChar = '\0')
{
    dest.clear();
    dest.reserve( source.size() );
    for ( std::string::const_iterator it = source.begin(), itEnd = source.end(); it != itEnd; ++it )
    {
        if ( !isalnum( static_cast<unsigned int>(*it) ) )
        {
            if ( compressionChar != '\0' )
                dest.push_back( compressionChar );
        }
        else
        {
            dest.push_back( *it );
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// https://stackoverflow.com/questions/1094841/reusable-library-to-get-human-readable-version-of-file-size
//
inline std::string humaniseByteSize( const char* prefix, const uint64_t bytes )
{
    if ( bytes == 0 )
        return fmt::format( "{} 0 bytes", prefix );
    else 
    if ( bytes == 1 )
        return fmt::format( "{} 1 byte", prefix );
    else
    {
        auto exponent = (int32_t)(std::log( bytes ) / std::log( 1024 ));
        auto quotient = double( bytes ) / std::pow( 1024, exponent );

        // done via a switch as fmt::format needs a consteval format arg
        switch ( exponent)
        {
            case 0: return fmt::format( "{}{:.0f} bytes", prefix, quotient );
            case 1: return fmt::format( "{}{:.0f} kB",    prefix, quotient );
            case 2: return fmt::format( "{}{:.1f} MB",    prefix, quotient );
            case 3: return fmt::format( "{}{:.2f} GB",    prefix, quotient );
            case 4: return fmt::format( "{}{:.2f} TB",    prefix, quotient );
            case 5: return fmt::format( "{}{:.2f} PB",    prefix, quotient );
            default:
                return "unknown";
                break;
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
template <int32_t _windowSize>
struct RollingAverage
{
    static constexpr double cNewValueWeight = 1.0 / (double)_windowSize;
    static constexpr double cOldValueWeight = 1.0 - cNewValueWeight;

    double  m_average       = 0.0;
    bool    m_initialSample = true;

    inline void update( double v )
    {
        if ( m_initialSample )
        {
            m_average = v;
            m_initialSample = false;
        }
        else
            m_average = ( v * cNewValueWeight) + ( m_average * cOldValueWeight);
    }
};

} // namespace base