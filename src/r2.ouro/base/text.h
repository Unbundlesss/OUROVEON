//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//

#pragma once

#include <string>

namespace base {

// ---------------------------------------------------------------------------------------------------------------------
inline void trimRight( std::string& str, const std::string& trimChars )
{
   std::string::size_type pos = str.find_last_not_of( trimChars );
   str.erase( pos + 1 );
}


// ---------------------------------------------------------------------------------------------------------------------
inline void trimLeft( std::string& str, const std::string& trimChars )
{
   std::string::size_type pos = str.find_first_not_of( trimChars );
   str.erase( 0, pos );
}


// ---------------------------------------------------------------------------------------------------------------------
inline void trim( std::string& str, const std::string& trimChars )
{
   trimRight( str, trimChars );
   trimLeft( str, trimChars );
} 

// ---------------------------------------------------------------------------------------------------------------------
inline void sanitiseNameForPath( const std::string_view source, std::string& dest, const char32_t replacementChar = '_', bool allowWhitespace = true )
{
    dest.clear();
    dest.reserve( source.length() );

    const char* w = source.data();
    const char* sourceEnd = w + source.length();

    // decode the source as a UTF8 stream to preserve any interesting, valid characters
    while ( w != sourceEnd )
    {
        char32_t cp = utf8::next( w, sourceEnd );

        // blitz control characters
        if ( cp >= 0x00 && cp <= 0x1f )
            cp = replacementChar;
        if ( cp >= 0x80 && cp <= 0x9f )
            cp = replacementChar;

        // strip out problematic pathname characters
        switch ( cp )
        {
        case '/':
        case '?':
        case '<':
        case '>':
        case '\\':
        case ':':
        case '*':
        case '|':
        case '\"':
        case '~':
        case '.':
            cp = replacementChar;
            break;

        default:
            break;
        }

        if ( !allowWhitespace )
        {
            switch ( cp )
            {
            case ' ':
            case '\t':
            case '\n':
                cp = replacementChar;
                break;

            default:
                break;
            }
        }

        utf8::append( cp, dest );
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// https://stackoverflow.com/questions/1094841/reusable-library-to-get-human-readable-version-of-file-size
//
inline std::string humaniseByteSize( const char* prefix, const uint64_t bytes )
{
    if ( bytes == 0 )
        return fmt::format( "{} 0 bytes", prefix );
    else 
    if ( bytes == 1 )
        return fmt::format( "{} 1 byte", prefix );
    else
    {
        auto exponent = (int32_t)( std::log( bytes ) / std::log( 1024 ) );
        auto quotient = double( bytes ) / std::pow( 1024, exponent );

        // done via a switch as fmt::format needs a consteval format arg
        switch ( exponent )
        {
            case 0: return fmt::format( FMTX( "{}{:.0f} bytes" ), prefix, quotient );
            case 1: return fmt::format( FMTX( "{}{:.0f} kB" ),    prefix, quotient );
            case 2: return fmt::format( FMTX( "{}{:.1f} MB" ),    prefix, quotient );
            case 3: return fmt::format( FMTX( "{}{:.2f} GB" ),    prefix, quotient );
            case 4: return fmt::format( FMTX( "{}{:.2f} TB" ),    prefix, quotient );
            case 5: return fmt::format( FMTX( "{}{:.2f} PB" ),    prefix, quotient );
            default:
                return "unknown";
                break;
        }
    }
}

} // namespace base
