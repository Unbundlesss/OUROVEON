//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//

#pragma once

namespace base {

template< typename _KeyType, typename _ValueType >
struct BiMap
{
    using KeyToValue = absl::flat_hash_map< _KeyType, _ValueType >;
    using ValueToKey = absl::flat_hash_map< _ValueType, _KeyType >;

    inline bool add( const _KeyType& key, const _ValueType& value )
    {
        const auto tryKey   = m_keyToValue.try_emplace( key, value );
        if ( !tryKey.second )
            return false;

        const auto tryValue = m_valueToKey.try_emplace( value, key );
        ABSL_ASSERT( tryKey.second );

        return true;
    }

    inline bool remove( const _KeyType& key )
    {
        const auto it = m_keyToValue.find( key );
        if ( it == m_keyToValue.end() )
            return false;

        const _ValueType& value = it->second;
        m_valueToKey.erase( value );
        m_keyToValue.erase( key );

        return true;
    }

    inline bool hasKey( const _KeyType& key ) const
    {
        const auto it = m_keyToValue.find( key );
        return ( it != m_keyToValue.end() );
    }

    inline bool hasValue( const _ValueType& value ) const
    {
        const auto it = m_valueToKey.find( value );
        return ( it != m_valueToKey.end() );
    }


protected:

    KeyToValue      m_keyToValue;
    ValueToKey      m_valueToKey;
};

} // namespace base
