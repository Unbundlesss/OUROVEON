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
// carefully strip out invalid characters from a utf8 string stream, ensuring the result can be used as a valid
// path on any of the platforms that we run on; any invalid characters will be replaced by a chosen character and
// whitespace can be compacted if desired
//
inline void sanitiseNameForPath( const std::string_view source, std::string& dest, const char32_t replacementChar = '_', bool allowWhitespace = true )
{
    dest.clear();
    dest.reserve( source.length() );

    const char* w = source.data();
    const char* sourceEnd = w + source.length();

    bool endsWithWhitespace = false;

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

        switch ( cp )
        {
            case ' ':
            case '\t':
            case '\n':
            {
                if ( !allowWhitespace )
                {
                    cp = replacementChar;
                }
                endsWithWhitespace = true;
            }
            break;

            default:
            {
                endsWithWhitespace = false;
            }
            break;
        }

        utf8::append( cp, dest );
    }

    if ( endsWithWhitespace )
    {
        dest += "_";
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// stupid compression of the extended latin range back to basic, just dozy replacement
//
inline void latinExtendedToBasic( const std::string_view source, std::string& dest )
{
    dest.clear();
    dest.reserve( source.length() );

    const char* w = source.data();
    const char* sourceEnd = w + source.length();

    // decode the source as a UTF8 stream to preserve any interesting, valid characters
    while ( w != sourceEnd )
    {
        char32_t cp = utf8::next( w, sourceEnd );

        switch ( cp )
        {
            case U'\x00C0':
            case U'\x00C1':
            case U'\x00C2':
            case U'\x00C3':
            case U'\x00C4':
            case U'\x00C5':
            case U'\x00C6':
                cp = U'A';
                break;

            case U'\x00C7':
                cp = U'C';
                break;

            case U'\x00C8':
            case U'\x00C9':
            case U'\x00CA':
            case U'\x00CB':
                cp = U'E';
                break;

            case U'\x00CC':
            case U'\x00CD':
            case U'\x00CE':
            case U'\x00CF':
                cp = U'I';
                break;

            case U'\x00D0':
                cp = U'D';
                break;

            case U'\x00D1':
                cp = U'N';
                break;

            case U'\x00D2':
            case U'\x00D3':
            case U'\x00D4':
            case U'\x00D5':
            case U'\x00D6':
            case U'\x00D8':
                cp = U'O';
                break;

            case U'\x00D7':
                cp = U'x';
                break;

            case U'\x00D9':
            case U'\x00DA':
            case U'\x00DB':
            case U'\x00DC':
                cp = U'U';
                break;

            case U'\x00DD':
                cp = U'Y';
                break;
            case U'\x00DF':
                cp = U'S';
                break;

            // ------------------------------

            case U'\x00E0':
            case U'\x00E1':
            case U'\x00E2':
            case U'\x00E3':
            case U'\x00E4':
            case U'\x00E5':
            case U'\x00E6':
                cp = U'a';
                break;

            case U'\x00E7':
                cp = U'c';
                break;

            case U'\x00E8':
            case U'\x00E9':
            case U'\x00EA':
            case U'\x00EB':
                cp = U'e';
                break;

            case U'\x00EC':
            case U'\x00ED':
            case U'\x00EE':
            case U'\x00EF':
                cp = U'i';
                break;

            case U'\x00F0':
                cp = U'd';
                break;

            case U'\x00F1':
                cp = U'n';
                break;

            case U'\x00F2':
            case U'\x00F3':
            case U'\x00F4':
            case U'\x00F5':
            case U'\x00F6':
            case U'\x00F8':
                cp = U'o';
                break;

            case U'\x00F7':
                cp = U'd';
                break;

            case U'\x00F9':
            case U'\x00FA':
            case U'\x00FB':
            case U'\x00FC':
                cp = U'u';
                break;

            case U'\x00FD':
            case U'\x00FF':
                cp = U'y';
                break;

            case U'\x00FE':
                cp = U'b';
                break;
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
