//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  a strongly typed wrapper for hash/ID values, so we can distinguish
//  between - say - the hash value for a Riff and a Stem
//

#pragma once

#include "base/hashing.h"

namespace base {
namespace id {

// ---------------------------------------------------------------------------------------------------------------------
template< typename _identity >
struct HashWrapper
{
    using HashWrapperType = HashWrapper<_identity>;

    static inline const HashWrapper Invalid() { return HashWrapper(); }

    constexpr explicit HashWrapper( const uint64_t _id ) : id( _id )
    {
        // we produce a secondary 32-bit hash for UI usage (and to help avoid double-zero results, 
        // which we use as 'invalid' even though technically it's a valid hash value)
        id32 = base::reduce64To32( id );
        ABSL_ASSERT( id != 0 && id32 != 0 ); // TBD 
    }
    constexpr HashWrapper( const HashWrapper& rhs ) : id( rhs.id ), id32( rhs.id32) {}

    ouro_nodiscard constexpr uint64_t getID() const { return id; }
    ouro_nodiscard constexpr uint32_t getID32() const { return id32; }

    constexpr bool operator == ( const HashWrapper& rhs ) const { return rhs.id == id && rhs.id32 == id32; }
    constexpr bool operator != ( const HashWrapper& rhs ) const { return rhs.id != id || rhs.id32 != id32; }

    // abseil
    template <typename H>
    friend H AbslHashValue( H h, const HashWrapperType& m )
    {
        return H::combine( std::move( h ), m.id );
    }

private:

    constexpr explicit HashWrapper() : id( 0ULL ), id32( 0U ) {}

    uint64_t    id;
    uint32_t    id32;
};

} // namespace id
} // namespace base

template< typename _HashWrapper >
struct hw_hash {
    size_t operator()( _HashWrapper const& tid ) const noexcept { return tid.getID(); }
};
