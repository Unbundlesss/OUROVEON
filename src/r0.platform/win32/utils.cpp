//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  various win32 bits
//

#include "pch.h"

#include "win32/utils.h"
#include <objbase.h>

namespace win32 {

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
