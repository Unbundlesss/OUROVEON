//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//

#pragma once

namespace base {

class UriParse
{
public:
    UriParse(std::string uri);
    ~UriParse();

    bool isValid() const { return m_valid; }

    std::string scheme()   const;
    std::string host()     const;
    std::string port()     const;
    std::string path()     const;
    std::string query()    const;
    std::string fragment() const;

private:
    std::string m_uri;
    bool        m_valid;

    struct ParsedData;
    std::unique_ptr< ParsedData >   m_data;
};

} // namespace base