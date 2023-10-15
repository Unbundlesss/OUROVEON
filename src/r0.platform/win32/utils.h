//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  various win32 bits
//

#pragma once

#if OURO_PLATFORM_WIN

namespace win32 {

// ---------------------------------------------------------------------------------------------------------------------
char const* NameForRootHKEY( const /*HKEY*/void* hKey );

// ---------------------------------------------------------------------------------------------------------------------
struct ScopedInitialiseCOM
{
    ScopedInitialiseCOM();
    ~ScopedInitialiseCOM();
};

} // namespace win32

#endif // OURO_PLATFORM_WIN