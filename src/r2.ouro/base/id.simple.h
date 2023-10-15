//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  strongly-typed wrapper around int types 
//

#pragma once

#include "base/utils.h"

namespace base {
namespace id {

// ---------------------------------------------------------------------------------------------------------------------
template< typename _identity, typename _inttype, _inttype _defaultValue, _inttype _invalidValue >
struct Simple
{
    using IntType = _inttype;
    using SimpleType = Simple<_identity, _inttype, _defaultValue, _invalidValue >;

    constexpr Simple() : m_id( _defaultValue ) {}
    constexpr explicit Simple( const _inttype id ) : m_id( id )
    {
        static_assert(std::is_integral<_inttype>::value, "integral required for _idtype");
    }

    ouro_nodiscard constexpr static _inttype defaultValue() { return _defaultValue; }
    ouro_nodiscard constexpr static Simple invalid() { return Simple( _invalidValue ); }

    ouro_nodiscard constexpr bool isValid() const { return m_id != _invalidValue; }
    ouro_nodiscard constexpr _inttype get() const { return m_id; }

    ouro_nodiscard constexpr bool operator == ( const Simple& rhs ) const { return rhs.m_id == m_id; }
    ouro_nodiscard constexpr bool operator != ( const Simple& rhs ) const { return rhs.m_id != m_id; }

    // abseil
    template <typename H>
    friend H AbslHashValue( H h, const SimpleType& m )
    {
        return H::combine( std::move( h ), m.m_id );
    }

private:
    _inttype    m_id;
};

} // namespace id
} // namespace base

template< typename Simple >
struct smp_hash {
    size_t operator()( Simple const& tid ) const noexcept { return std::hash<typename Simple::IntType>( tid.get() ); }
};

struct _async_command_counter_id {};
using AsyncCommandCounter = base::id::Simple<_async_command_counter_id, uint32_t, 1, 0>;

