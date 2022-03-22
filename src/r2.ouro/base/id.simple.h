//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
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

    constexpr Simple() : m_id( _defaultValue ) {}

    constexpr explicit Simple( const _inttype _id ) : m_id( _id )
    {
        static_assert(std::is_integral<_inttype>::value, "integral required for _idtype");
    }

    constexpr _inttype get() const { return m_id; }

    constexpr bool operator == ( const Simple& rhs ) const { return rhs.m_id == m_id; }
    constexpr bool operator != ( const Simple& rhs ) const { return rhs.m_id != m_id; }


    static Simple invalid() { return Simple( _invalidValue ); }

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

