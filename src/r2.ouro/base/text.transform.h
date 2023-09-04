// original version from Jan Bergstrom
// https://stackoverflow.com/questions/36897781/how-to-uppercase-lowercase-utf-8-characters-in-c

// modified to allocate and return a std::string as that is more practically useful for our purposes

#include <string.h>
#include <string>

namespace base
{
    // convert a string to lower case with extended support outside of just base ASCII
    std::string StrToLwrExt( const std::string_view pInput );

} // namespace base
