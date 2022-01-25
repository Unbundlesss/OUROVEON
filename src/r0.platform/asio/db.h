//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
// ASIO homebrew SDK
// functions for digging through Win32 registry for registered ASIO drivers
//

#pragma once

#include "app/log.h"
#include "asio/common.h"

#include <string>
#include <unordered_map>
#include <algorithm>


namespace asio {

class DriverDatabase
{
public:

    // entry for a driver we found
    struct Entry
    {
        std::string    m_name;
        std::string    m_path;
        std::string    m_description;
        std::string    m_clsid;
        std::wstring   m_clsidWide; // used with CLSIDFromString
    };
    using Entries = std::unordered_map< std::string, Entry >;

    // fetch ASIO driver data from the registry; if registry errors occur, they will be logged with the given
    // callback and this function returns false. still returns true if we found no drivers
    bool Load( const app::LogHost* logHost );


    inline const Entries& getEntries() const { return m_entries; }

    inline const Entry* findEntry( const std::string& name ) const
    {
        auto eIt = m_entries.find( name );
        if ( eIt == m_entries.end() )
            return nullptr;

        return &(*eIt).second;
    }

private:

    Entries     m_entries;
};

} // namespace asio