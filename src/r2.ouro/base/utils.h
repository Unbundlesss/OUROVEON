//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//

#pragma once

// ---------------------------------------------------------------------------------------------------------------------
namespace mem {

// allocate numElements of _T aligned to 16 bytes
template< typename _T >
inline _T* alloc16( const size_t numElements )
{
    _T* mblock = reinterpret_cast<_T*>( rpmalloc( sizeof( _T ) * numElements ) );

    return mblock;
}

// allocate numElements of _T aligned to 16 bytes
template< typename _T >
inline _T* alloc16To( const size_t numElements, const _T defaultValue )
{
    _T* mblock = reinterpret_cast<_T*>( rpmalloc( sizeof( _T ) * numElements ) );

    for ( size_t kI = 0; kI < numElements; kI++ )
        mblock[kI] = defaultValue;

    return mblock;
}

// free memory allocated with allocateAlign16
inline void free16( void* ptr )
{
    return rpfree( ptr );
}

} // namespace mem

namespace base {

// ---------------------------------------------------------------------------------------------------------------------
template<class TContainer, class F>
auto erase_where( TContainer& c, F&& f )
{
    return c.erase( std::remove_if( c.begin(),
        c.end(),
        std::forward<F>( f ) ),
        c.end() );
}

template <typename TVectorElement>
void vector_move( std::vector<TVectorElement>& v, std::size_t oldIndex, std::size_t newIndex )
{
    if ( oldIndex > newIndex )
        std::rotate( v.rend() - oldIndex - 1, v.rend() - oldIndex, v.rend() - newIndex );
    else
        std::rotate( v.begin() + oldIndex, v.begin() + oldIndex + 1, v.begin() + newIndex + 1 );
}

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
template <int32_t _windowSize>
struct RollingAverage
{
    static constexpr double cNewValueWeight = 1.0 / (double)_windowSize;
    static constexpr double cOldValueWeight = 1.0 - cNewValueWeight;

    double  m_average       = 0.0;
    bool    m_initialSample = true;

    constexpr inline void reset( double toDefault = 0.0 )
    {
        m_average       = toDefault;
        m_initialSample = true;
    }

    constexpr inline void update( double v )
    {
        if ( m_initialSample )
        {
            m_average = v;
            m_initialSample = false;
        }
        else
            m_average = ( v * cNewValueWeight ) + ( m_average * cOldValueWeight );
    }
};

// https://github.com/miloyip/itoa-benchmark
namespace itoa {

    inline void u32toa( uint32_t value, char* buffer )
    {
        static constexpr char gDigitsLut[200] = {
            '0','0','0','1','0','2','0','3','0','4','0','5','0','6','0','7','0','8','0','9',
            '1','0','1','1','1','2','1','3','1','4','1','5','1','6','1','7','1','8','1','9',
            '2','0','2','1','2','2','2','3','2','4','2','5','2','6','2','7','2','8','2','9',
            '3','0','3','1','3','2','3','3','3','4','3','5','3','6','3','7','3','8','3','9',
            '4','0','4','1','4','2','4','3','4','4','4','5','4','6','4','7','4','8','4','9',
            '5','0','5','1','5','2','5','3','5','4','5','5','5','6','5','7','5','8','5','9',
            '6','0','6','1','6','2','6','3','6','4','6','5','6','6','6','7','6','8','6','9',
            '7','0','7','1','7','2','7','3','7','4','7','5','7','6','7','7','7','8','7','9',
            '8','0','8','1','8','2','8','3','8','4','8','5','8','6','8','7','8','8','8','9',
            '9','0','9','1','9','2','9','3','9','4','9','5','9','6','9','7','9','8','9','9'
        };

        char temp[10];
        char* p = temp;

        while ( value >= 100 )
        {
            const unsigned i = (value % 100) << 1;
            value /= 100;
            *p++ = gDigitsLut[i + 1];
            *p++ = gDigitsLut[i];
        }

        if ( value < 10 )
        {
            *p++ = char( value ) + '0';
        }
        else
        {
            const unsigned i = value << 1;
            *p++ = gDigitsLut[i + 1];
            *p++ = gDigitsLut[i];
        }

        do
        {
            *buffer++ = *--p;
        } while ( p != temp );

        *buffer = '\0';
    }

    inline void i32toa( int32_t value, char* buffer )
    {
        uint32_t u = static_cast<uint32_t>(value);
        if ( value < 0 )
        {
            *buffer++ = '-';
            u = ~u + 1;
        }
        u32toa( u, buffer );
    }

} // namespace itoa
} // namespace base
