//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  cross platform loading of shared libraries (DLLs on windows etc)
//

#include "pch.h"
#include "base/construction.h"
#include "sys/dynlib.h"

#if OURO_PLATFORM_WIN

#include "win32/errors.h"

#else // LINUX / MAC

#include <dlfcn.h>

#endif 

namespace sys {

// ---------------------------------------------------------------------------------------------------------------------
absl::StatusOr< DynLib::Instance > DynLib::loadFromFile( const fs::path& libraryPath )
{
    DynLib::Instance instanceResult = base::protected_make_shared<DynLib>();

    instanceResult->m_originalPath = libraryPath;

#if OURO_PLATFORM_WIN
    instanceResult->m_handle = ::LoadLibraryA( libraryPath.string().c_str() );
#else // LINUX / MAC
    instanceResult->m_handle = ::dlopen( libraryPath.string().c_str(), RTLD_LAZY );
#endif 

    if ( instanceResult->m_handle == nullptr )
    {
        return absl::InternalError( getLastError() );
    }

    return instanceResult;
}

// ---------------------------------------------------------------------------------------------------------------------
DynLib::~DynLib()
{
    if ( m_handle != nullptr )
    {
#if OURO_PLATFORM_WIN
        const BOOL bClosed = ::FreeLibrary( m_handle );
        if ( static_cast<int32_t>( bClosed ) == 0 )
#else // LINUX / MAC
        const int32_t bClosed = ::dlclose( m_handle );
        if ( static_cast<int32_t>( bClosed ) != 0 )
#endif 
        {
            blog::error::app( FMTX( "failed to close library [{}]" ), m_originalPath.string() );
            blog::error::app( FMTX( "error was [{}]" ), getLastError() );
        }
        m_handle = nullptr;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
std::string DynLib::getLastError()
{
#if OURO_PLATFORM_WIN
    return win32::FormatLastErrorCode();
#else // LINUX / MAC
    char* lastError = ::dlerror();
    if ( lastError == nullptr )
        return "Unknown";
    return std::string( lastError );
#endif 
}

// ---------------------------------------------------------------------------------------------------------------------
DynLib::LibrarySymbol DynLib::resolveSymbol( std::string_view symbolName )
{
    if ( m_handle == nullptr )
        return nullptr;

#if OURO_PLATFORM_WIN
    return ::GetProcAddress( m_handle, symbolName.data() );
#else // LINUX / MAC
    return ::dlsym( m_handle, symbolName.data() );
#endif 
}

} // namespace sys
