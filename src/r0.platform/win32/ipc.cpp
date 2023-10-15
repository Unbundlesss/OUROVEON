//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#include "pch.h"

#if OURO_PLATFORM_WIN

#include "win32/ipc.h"

#include <sddl.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <VersionHelpers.h>

namespace win32 {

// ---------------------------------------------------------------------------------------------------------------------
//
// http://web.archive.org/web/20151215210112/http://blogs.msdn.com/b/winsdk/archive/2009/11/10/access-denied-on-a-mutex.aspx
//
PSECURITY_DESCRIPTOR MakeAllowAllSecurityDescriptor(void)
{
    const WCHAR *pszStringSecurityDescriptor;
    if ( ::IsWindowsVistaOrGreater() )
        pszStringSecurityDescriptor = L"D:(A;;GA;;;WD)(A;;GA;;;AN)S:(ML;;NW;;;ME)";
    else
        pszStringSecurityDescriptor = L"D:(A;;GA;;;WD)(A;;GA;;;AN)";
    
    PSECURITY_DESCRIPTOR pSecDesc;
    if ( !::ConvertStringSecurityDescriptorToSecurityDescriptor(pszStringSecurityDescriptor, SDDL_REVISION_1, &pSecDesc, nullptr) )
        return nullptr;
    
    return pSecDesc;
}


// ---------------------------------------------------------------------------------------------------------------------
void* details::IPC::create( const std::wstring& mapName, const std::wstring& mutexName, const Access requestedAccess, const uint32_t bufferSize )
{
    m_hFileMapping = ::CreateFileMapping(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        bufferSize,
        mapName.c_str() );

    DWORD mapAccess = FILE_MAP_WRITE;
    if ( requestedAccess == Access::Read )
        mapAccess = FILE_MAP_READ;

    uint8_t* memoryBuffer = (uint8_t*)::MapViewOfFile( m_hFileMapping, mapAccess, 0, 0, bufferSize );

    if ( requestedAccess == Access::Write )
        memset( memoryBuffer, 0, bufferSize );

    auto pSecDesc = MakeAllowAllSecurityDescriptor();
    if ( pSecDesc )
    {
        SECURITY_ATTRIBUTES SecAttr;
        SecAttr.nLength              = sizeof( SECURITY_ATTRIBUTES );
        SecAttr.lpSecurityDescriptor = pSecDesc;
        SecAttr.bInheritHandle       = FALSE;

        m_hMutex = ::CreateMutex( &SecAttr, FALSE, mutexName.c_str() );
        auto dwError = ::GetLastError();

        // TODO handle dwError

        ::LocalFree( pSecDesc );
    }

    return memoryBuffer;
}

// ---------------------------------------------------------------------------------------------------------------------
void details::IPC::discard( void* memptr )
{
    if ( m_hMutex != INVALID_HANDLE_VALUE )
        ::CloseHandle( m_hMutex );

    if ( memptr != nullptr )
        ::UnmapViewOfFile( memptr );

    if ( m_hFileMapping != INVALID_HANDLE_VALUE )
        ::CloseHandle( m_hFileMapping );
}

// ---------------------------------------------------------------------------------------------------------------------
void details::IPC::lock()
{
    WaitForSingleObject( m_hMutex, INFINITE );
}

// ---------------------------------------------------------------------------------------------------------------------
void details::IPC::unlock()
{
    ReleaseMutex( m_hMutex );
}

} // namespace win32

#endif // OURO_PLATFORM_WIN