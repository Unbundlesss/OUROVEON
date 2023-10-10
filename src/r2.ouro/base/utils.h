//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
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

} // namespace base
