//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  helper tools for working with rapidyaml
//

#pragma once

#include <rapidyaml/rapidyaml.hpp>

// Daniel Lemire's fast_float version of std::from_chars
#include "data/fast_float.h"

namespace data {

struct HexFloat { float result; };

template< typename _ValueType >
absl::StatusOr<_ValueType> parseYamlValue( const ryml::Tree& tree, std::string_view keyContext, const c4::csubstr& nodeValue )
{
    const std::string_view nodeValueString{ nodeValue.begin(), nodeValue.size() };

    // string_view should remain valid from the tree's copy of the input text
    if constexpr ( std::is_same_v<_ValueType, std::string> )
    {
        return std::string( nodeValueString );
    }
    else if constexpr ( std::is_same_v<_ValueType, bool> )
    {
        if ( nodeValueString == "true" )
        {
            return true;
        }
        if ( nodeValueString == "false" )
        {
            return false;
        }
        return absl::UnknownError( fmt::format( FMTX( "boolean value [{}] unrecognised" ), nodeValueString ) );
    }
    // use fast_float conversion for double/float, double can be used for most integers we care about 
    else if constexpr ( std::is_same_v<_ValueType, double> || std::is_same_v<_ValueType, float> )
    {
        _ValueType result;
        auto [ptr, errorCode] = fast_float::from_chars( nodeValueString.data(), (&nodeValueString.back()) + 1, result );

        if ( errorCode == std::errc() )
        {
            return result;
        }
        else
        {
            const int32_t errNumber = static_cast<int32_t>(errorCode);

            return absl::Status(
                absl::ErrnoToStatusCode( errNumber ),
                fmt::format( FMTX( "Yaml [{}] failed to convert '{}' to number ({})" ), keyContext, nodeValueString, std::strerror( errNumber ) ) );
        }
    }
    // HexFloat specialisation to use strtof() that can handle the standardised hex float format
    else if constexpr ( std::is_same_v<_ValueType, HexFloat> )
    {
        if ( nodeValueString.size() < 3 || nodeValueString[0] != '0' || nodeValueString[1] != 'x' )
        {
            return absl::OutOfRangeError( fmt::format( FMTX( "Yaml [{}] invalid hex digit string [{}]" ), keyContext, nodeValueString ) );
        }

        HexFloat hf;
        hf.result = std::strtof( nodeValueString.data(), nullptr );

        return hf;
    }
    // anything else will stop compilation
    else
    {
        static_assert("unknown type to parse from yaml");
    }
}

template< typename _ValueType >
absl::StatusOr<_ValueType> parseYamlValue( const ryml::Tree& tree, std::string_view keyName )
{
    auto nodeKey = ryml::csubstr( std::data( keyName ), std::size( keyName ) );

    const auto nodeID = tree.find_child( tree.root_id(), nodeKey );
    if ( nodeID == size_t( -1 ) )
    {
        return absl::InvalidArgumentError( fmt::format( FMTX( "requied key [{}] does not exist" ), keyName ) );
    }

    const auto nodeRef = tree[nodeKey];

    if ( !nodeRef.valid() )
    {
        return absl::InvalidArgumentError( fmt::format( FMTX( "requied key [{}] was not valid" ), keyName ) );
    }
    if ( !nodeRef.has_val() )
    {
        return absl::InvalidArgumentError( fmt::format( FMTX( "requied key [{}] has no value" ), keyName ) );
    }

    return parseYamlValue<_ValueType>( tree, keyName, nodeRef.val() );
}

} // namespace data
