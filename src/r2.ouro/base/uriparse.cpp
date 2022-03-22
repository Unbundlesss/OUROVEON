//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  spacetime is a namespace full of datetime/chrono/etc related stuff
//  named as such to be clear separate and distinct, even if a little silly

#include "pch.h"

#include "base/uriparse.h"
#include <uriparser/Uri.h>

namespace base {

struct UriParse::ParsedData 
{
    ~ParsedData()
    {
        uriFreeUriMembersA( &m_uriParsed );
    }

    UriUriA m_uriParsed;

    static std::string fromRange( const UriTextRangeA& rng )
    {
        return std::string( rng.first, rng.afterLast );
    }

    static std::string fromList( UriPathSegmentA* xs, const std::string& delim )
    {
        UriPathSegmentStructA* head( xs );
        std::string accum;

        while ( head )
        {
            accum += delim + fromRange( head->text );
            head = head->next;
        }

        return accum;
    }
};

UriParse::UriParse( const std::string& uri ) 
    : m_data( std::make_unique<ParsedData>() )
    , m_uri( uri )
    , m_valid( false )
{
    UriParserStateA state_;
    state_.uri = &m_data->m_uriParsed;
    m_valid = uriParseUriA( &state_, m_uri.c_str() ) == URI_SUCCESS;
}

UriParse::~UriParse()
{
}


std::string UriParse::scheme()   const { return m_data->fromRange( m_data->m_uriParsed.scheme ); }
std::string UriParse::host()     const { return m_data->fromRange( m_data->m_uriParsed.hostText ); }
std::string UriParse::port()     const { return m_data->fromRange( m_data->m_uriParsed.portText ); }
std::string UriParse::path()     const { return m_data->fromList(  m_data->m_uriParsed.pathHead, "/" ); }
std::string UriParse::query()    const { return m_data->fromRange( m_data->m_uriParsed.query ); }
std::string UriParse::fragment() const { return m_data->fromRange( m_data->m_uriParsed.fragment ); }

} // namespace base
