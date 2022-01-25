//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
// ASIO homebrew SDK
// functions for digging through Win32 registry for registered ASIO drivers
//

#include "pch.h"

#include "app/log.h"
#include "asio/db.h"
#include "win32/utils.h"
#include "win32/errors.h"

#include "fmt/core.h"

#include <shlwapi.h>    // PathFileExists


namespace asio {

static constexpr char const*      c_regSoftwareAsio       = "software\\asio";
static constexpr char const*      c_regClassesCLSID       = "clsid";
static constexpr char const*      c_regDescription        = "description";
static constexpr char const*      c_regInprocServer32     = "InprocServer32";
static constexpr size_t           c_regReadBufferLength   = 1024;

struct ScopedRegKey
{
    ScopedRegKey() = delete;
    ScopedRegKey( const ScopedRegKey& rhs ) = delete;

    explicit ScopedRegKey( HKEY hKey, LPCSTR lpSubKey )
        : m_status( ERROR_INVALID_FUNCTION )
        , m_hKey( nullptr )
    {
        m_request   = fmt::format( "{0}/{1}", win32::NameForRootHKEY( hKey ), lpSubKey );
        m_status    = ::RegOpenKeyExA( 
            hKey,
            lpSubKey,
            REG_NONE,
            KEY_READ,
            &m_hKey );
    }

    ScopedRegKey( const ScopedRegKey& scopedKey, LPCSTR lpSubKey )
        : m_status( ERROR_INVALID_FUNCTION )
        , m_hKey( nullptr )
    {
        m_request = fmt::format( "{0}/{1}", scopedKey.m_request, lpSubKey );
        m_status = ::RegOpenKeyExA(
            scopedKey(),
            lpSubKey,
            REG_NONE,
            KEY_READ,
            &m_hKey );
    }

    ~ScopedRegKey()
    {
        if ( m_hKey )
            ::RegCloseKey( m_hKey );
    }

    inline bool checkOpened( const app::LogHost* logHost )
    {
        if ( isSuccessful() )
            return true;

        logHost->Error( fmt::format( "ERROR | failed to open registry [{0}]", m_request ) );
        logHost->Error( win32::FormatErrorCode( getStatus() ) );
        return false;
    }

    inline bool tryReadStringValue( const char* valueName, std::string& resultString, const app::LogHost* logHost )
    {
        DWORD keyDataBufferLen = c_regReadBufferLength;
        DWORD keyDataType = REG_SZ;
        char keyDataBuffer[c_regReadBufferLength]{ 0 };

        LSTATUS cr = RegQueryValueExA( m_hKey, valueName, nullptr, &keyDataType, (LPBYTE)keyDataBuffer, &keyDataBufferLen );
        if ( cr != ERROR_SUCCESS )
        {
            logHost->Error( fmt::format( "ERROR | unable to read string '{0}' from [{1}]", valueName, m_request ) );
            logHost->Error( win32::FormatErrorCode( cr ) );
            return false;
        }
        
        resultString = keyDataBuffer;
        return true;
    }

    inline bool isSuccessful() const { return getStatus() == ERROR_SUCCESS; }
    inline LSTATUS getStatus() const { return m_status; }

    inline HKEY operator() () const { return m_hKey; }

private:

    std::string     m_request;
    LSTATUS         m_status;
    HKEY            m_hKey;
};

bool DriverDatabase::Load( const app::LogHost* logHost )
{
    // allow this to run as many times as you like, clear out any existing entries
    m_entries.clear();

    // open the root keys we'll be iterating from
    ScopedRegKey hKeyAsioRoot( HKEY_LOCAL_MACHINE, c_regSoftwareAsio );
    ScopedRegKey hKeyClassRoot( HKEY_CLASSES_ROOT, c_regClassesCLSID );

    if ( !hKeyAsioRoot.checkOpened( logHost ) )
        return false;

    if ( !hKeyClassRoot.checkOpened( logHost ) )
        return false;
    
    // walk list of ASIO drivers in the root key
    for ( DWORD index = 0;; index ++ )
    {
        DWORD keyDataBufferLen  = c_regReadBufferLength;
        char keyDataBuffer[c_regReadBufferLength] { 0 };

        if ( ::RegEnumKeyExA( hKeyAsioRoot(), index, keyDataBuffer, &keyDataBufferLen, nullptr, nullptr, nullptr, nullptr ) == ERROR_SUCCESS )
        {
            // open that key to read out values
            ScopedRegKey hKeyDriverRoot( hKeyAsioRoot, keyDataBuffer );

            if ( !hKeyDriverRoot.checkOpened( logHost ) )
                return false;

            Entry newEntry;
            newEntry.m_name = keyDataBuffer;

            if ( !hKeyDriverRoot.tryReadStringValue( c_regDescription, newEntry.m_description, logHost ) )
                return false;

            if ( !hKeyDriverRoot.tryReadStringValue( c_regClassesCLSID, newEntry.m_clsid, logHost ) )
                return false;

            // walk all entries in CLSID index
            for ( DWORD subIndex = 0;; subIndex++ )
            {
                keyDataBufferLen = c_regReadBufferLength;
                if ( ::RegEnumKeyExA( hKeyClassRoot(), subIndex, keyDataBuffer, &keyDataBufferLen, nullptr, nullptr, nullptr, nullptr ) == ERROR_SUCCESS )
                {
                    // found matching GUID?
                    if ( _stricmp( keyDataBuffer, newEntry.m_clsid.c_str() ) == 0 )
                    {
                        // go get the InprocServer32 sub key
                        ScopedRegKey hKeyCOMRoot( hKeyClassRoot, keyDataBuffer );
                        ScopedRegKey hKeyInProc( hKeyCOMRoot, c_regInprocServer32 );

                        if ( !hKeyInProc.checkOpened( logHost ) )
                            break;

                        // fetch the default value, which should be a path to the provider DLL
                        if ( hKeyInProc.tryReadStringValue( nullptr, newEntry.m_path, logHost ) )
                        {
                            // report drivers as we go
                            logHost->Info( fmt::format( "DRIVER | {:<20} | {}", newEntry.m_name, newEntry.m_path ) );

                            // .. check it exists? we could do more here maybe
                            if ( ::PathFileExistsA( newEntry.m_path.c_str() ) )
                            {
                                wchar_t wideCLSID[c_regReadBufferLength]{ 0 };

                                ::MultiByteToWideChar( 
                                    CP_ACP,
                                    0,
                                    (LPCSTR)newEntry.m_clsid.c_str(),
                                    (int)newEntry.m_clsid.size(),
                                    (LPWSTR)wideCLSID,
                                    c_regReadBufferLength );

                                newEntry.m_clsidWide = wideCLSID;

                                m_entries.emplace( newEntry.m_name, std::move(newEntry) );
                            }
                        }

                        break; // hit a matching CLSID
                    }
                }
                else
                    break;  // run out of entries in CLSID hierarchy
            }
        }
        else
            break;  // run out of entries in the ASIO root key
    }

    return true;
}

} // namespace asio