//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  a strongly typed wrapper Couch UID values; it helps to distinguish when a function
//  expects a "Jam ID" string vs a "Riff ID" string, despite them looking alike; this 
//  also adds shims for all the serialization and hashing required

#pragma once

#include "base/utils.h"

namespace base {
namespace id {

template<class _identity>
struct StringWrapper
{
    explicit inline StringWrapper( const char* rhs ) : _value( rhs ) {}
    explicit inline StringWrapper( std::string rhs ) : _value( std::move(rhs) ) {}
    explicit inline StringWrapper( const std::string_view& rhs ) : _value( rhs ) {}
    inline StringWrapper() : _value() {}

    ouro_nodiscard constexpr const std::string& value() const { return _value; }
    ouro_nodiscard constexpr std::string& value() { return _value; }
    ouro_nodiscard constexpr operator const std::string&() const { return _value; }
    ouro_nodiscard constexpr operator const char*() const { return _value.c_str(); }

    inline auto operator<=>( const StringWrapper& ) const = default;

    ouro_nodiscard constexpr bool empty() const { return _value.empty(); }
    ouro_nodiscard constexpr const char* c_str() const { return _value.c_str(); }
    ouro_nodiscard constexpr std::size_t size() const { return _value.size(); }
    ouro_nodiscard constexpr std::string substr( size_t len ) const { return _value.substr( 0, len ); }

    // enabled for archival via cereal
    template <class Archive,
        cereal::traits::EnableIf<cereal::traits::is_text_archive<Archive>::value>
        = cereal::traits::sfinae>
        std::string save_minimal( Archive& ) const
    {
        return _value;
    }
    template <class Archive,
        cereal::traits::EnableIf<cereal::traits::is_text_archive<Archive>::value>
        = cereal::traits::sfinae>
        void load_minimal( Archive const&, std::string const& str )
    {
        _value = str;
    }

    using StringWrapperType = StringWrapper<_identity>;

    // absl hash support
    template <typename H>
    friend H AbslHashValue( H h, const StringWrapperType& m )
    {
        return H::combine( std::move( h ), m._value );
    }

private:
    std::string _value;
};

} // namespace id
} // namespace base


// create output shims for {fmt}, sit on top of existing std::string formatting
#define Gen_StringWrapperFormatter( _type )                                                         \
            template <> struct fmt::formatter<_type> : formatter<std::string>                       \
            {                                                                                       \
                template <typename FormatContext>                                                   \
                auto format( const _type& c, FormatContext& ctx ) const -> decltype(ctx.out())      \
                {                                                                                   \
                    return formatter<std::string>::format( c, ctx );                                \
                }                                                                                   \
            };

