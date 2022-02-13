//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  various win32 bits
//

#include "pch.h"

#if OURO_PLATFORM_WIN

#include "win32/utils.h"

#define WIN32_LEAN_AND_MEAN
#include <objbase.h>
#include <windows.h>

namespace win32 {

char const* NameForRootHKEY( const HKEY hKey )
{
    switch ( (ULONG_PTR)hKey )
    {
        case (ULONG_PTR)((LONG)0x80000000)/*HKEY_CLASSES_ROOT*/ :   return "CLASSES_ROOT";
        case (ULONG_PTR)((LONG)0x80000001)/*HKEY_CURRENT_USER*/ :   return "CURRENT_USER";
        case (ULONG_PTR)((LONG)0x80000002)/*HKEY_LOCAL_MACHINE*/ :  return "LOCAL_MACHINE";
        case (ULONG_PTR)((LONG)0x80000003)/*HKEY_USERS*/ :          return "USERS";
    }
    return "UNKNOWN";
}

// ---------------------------------------------------------------------------------------------------------------------
ScopedInitialiseCOM::ScopedInitialiseCOM()
{
    CoInitializeEx( nullptr, COINIT_APARTMENTTHREADED | COINIT_SPEED_OVER_MEMORY );
}

// ---------------------------------------------------------------------------------------------------------------------
ScopedInitialiseCOM::~ScopedInitialiseCOM()
{
    CoUninitialize();
}

} // namespace win32

#endif // OURO_PLATFORM_WIN