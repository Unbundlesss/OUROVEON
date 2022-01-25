//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

namespace app {

struct LogHost
{
    virtual void Info( const char* msg ) const = 0;
    virtual void Error( const char* msg ) const = 0;

    void Info( const std::string& smsg ) const { Info( smsg.c_str() ); }
    void Error( const std::string& smsg ) const { Error( smsg.c_str() ); }
};

} // namespace app

