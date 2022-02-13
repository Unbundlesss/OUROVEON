//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  turn windows error codes into strings
//

#pragma once

#if OURO_PLATFORM_WIN

namespace win32 {

// ---------------------------------------------------------------------------------------------------------------------
//
std::string FormatLastErrorCode();
std::string FormatErrorCode( const LONG errorCode );

// ---------------------------------------------------------------------------------------------------------------------
// turn WM_* windows message codes into string rep
//
const char* GetStringFromWM( uint32_t wmcode );

} // namespace win32

#endif // OURO_PLATFORM_WIN