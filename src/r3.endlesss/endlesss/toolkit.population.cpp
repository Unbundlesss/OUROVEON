//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"
#include "endlesss/toolkit.population.h"
#include "endlesss/config.h"


namespace endlesss {
namespace toolkit {


// ---------------------------------------------------------------------------------------------------------------------
void PopulationQuery::loadPopulationData( const config::IPathProvider& pathProvider )
{
    m_nameTrieValid = false;
    m_nameTrie.clear();

    config::endlesss::PopulationGlobalUsers populationData;
    const auto dataLoad = config::load( pathProvider, populationData );
    if ( dataLoad == config::LoadResult::Success )
    {
        for ( const auto& username : populationData.users )
        {
            m_nameTrie.emplace( username );
        }
        blog::core( FMTX( "loaded {} usernames into population trie" ), populationData.users.size() );

        m_nameTrieValid = !populationData.users.empty();
    }
    else
    {
        blog::error::core( FMTX( "unable to load {}, population search will be unavailable" ), config::endlesss::PopulationGlobalUsers::StorageFilename );
    }
}

// ---------------------------------------------------------------------------------------------------------------------
bool PopulationQuery::prefixQuery( const std::string_view prefix, Result& result ) const
{
    if ( m_nameTrieValid == false )
        return false;

    // run query with given prefix
    auto prefixRange = m_nameTrie.equal_prefix_range( prefix );

    result.clear();

    for ( auto it = prefixRange.first; it != prefixRange.second; ++it, result.m_validCount ++ )
    {
        if ( result.m_validCount == MaximumQueryResults )
            break;

        // extract result into existing buffers
        it.key( result.m_values[result.m_validCount] );
    }

    return result.m_validCount > 0;
}

} // namespace toolkit
} // namespace endlesss
