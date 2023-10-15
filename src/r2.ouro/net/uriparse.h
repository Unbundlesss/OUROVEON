//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//

#pragma once

namespace base {

class UriParse
{
public:
    UriParse( const std::string& uri );
    ~UriParse();

    inline bool isValid() const { return m_valid; }

    std::string scheme()   const;
    std::string host()     const;
    std::string port()     const;
    std::string path()     const;
    std::string query()    const;
    std::string fragment() const;

private:

    struct ParsedData;
    std::unique_ptr< ParsedData >   m_data;

    std::string m_uri;
    bool        m_valid;
};

} // namespace base