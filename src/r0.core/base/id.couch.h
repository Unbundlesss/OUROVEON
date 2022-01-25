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

    inline const std::string& value() const { return _value; }
    inline std::string& value() { return _value; }
    inline operator const std::string&() const { return _value; }
    inline operator const char*() const { return _value.c_str(); }

    inline auto operator<=>( const StringWrapper& ) const = default;

    inline bool empty() const { return _value.empty(); }
    inline const char* c_str() const { return _value.c_str(); }
    inline std::size_t size() const { return _value.size(); }

    template <class Archive,
        cereal::traits::EnableIf<cereal::traits::is_text_archive<Archive>::value>
        = cereal::traits::sfinae>
        std::string save_minimal( Archive& ) const
    {
        return _value;
    }

    // Enabled for text archives (e.g. XML, JSON)
    template <class Archive,
        cereal::traits::EnableIf<cereal::traits::is_text_archive<Archive>::value>
        = cereal::traits::sfinae>
        void load_minimal( Archive const&, std::string const& str )
    {
        _value = str;
    }

private:
    std::string _value;

};

} // namespace id
} // namespace base

template< typename _StringWrapper >
struct cid_hash {
    size_t operator()( _StringWrapper const& tid ) const noexcept { return std::hash<std::string>{}(tid.value()); }
};

// create shims for Fmt and Cereal
#define Gen_StringWrapperFormatter( _type )                                                                                 \
            template <> struct fmt::formatter<_type> : formatter<std::string> {                                             \
                template <typename FormatContext>                                                                           \
                auto format( const _type& c, FormatContext& ctx ) {                                                         \
                    return formatter<std::string>::format( c, ctx );                                                        \
                }                                                                                                           \
            };