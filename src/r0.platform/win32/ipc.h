//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#if OURO_PLATFORM_WIN
#pragma optimize("", off)
namespace win32 {

namespace details {

struct IPC
{
    enum class Access
    {
        Read,
        Write
    };

    void* create( const std::wstring& mapName, const std::wstring& mutexName, const Access requestedAccess, const uint32_t bufferSize );
    void discard( void* memptr );

    void lock();
    void unlock();

    HANDLE              m_hFileMapping  = INVALID_HANDLE_VALUE;
    HANDLE              m_hMutex        = INVALID_HANDLE_VALUE;
};

} // namespace details

// ---------------------------------------------------------------------------------------------------------------------
template<typename _ExchangeType>
struct GlobalSharedMemory : protected details::IPC
{
    GlobalSharedMemory()
        : m_txBuffer( nullptr )
    {}

    ~GlobalSharedMemory()
    {
        discard( m_txBuffer );
    }

    inline bool init( const std::wstring& mapName, const std::wstring& mutexName, const details::IPC::Access requestedAccess )
    {
        m_accessMode = requestedAccess;
        m_txBuffer   = (_ExchangeType*) create( mapName, mutexName, requestedAccess, sizeof( _ExchangeType ) );

        return ( m_txBuffer != nullptr );
    }

    inline bool canWrite() const
    {
        if ( m_txBuffer == nullptr )
            return false;
        if ( m_accessMode != details::IPC::Access::Write )
            return false;

        return true;
    }

    inline bool writeType( const _ExchangeType& data )
    {
        if ( !canWrite() )
            return false;

        lock();
        memcpy( m_txBuffer, &data, sizeof( _ExchangeType ) );
        unlock();

        return true;
    }

    inline bool canRead() const 
    {
        if ( m_txBuffer == nullptr )
            return false;
        if ( m_accessMode != details::IPC::Access::Read )
            return false;

        return true;
    }

    inline bool readType( _ExchangeType& result )
    {
        if ( !canRead() )
            return false;

        lock();
        memcpy( &result, m_txBuffer, sizeof( _ExchangeType ) );
        unlock();

        return true;
    }

protected:
    details::IPC::Access    m_accessMode;
    _ExchangeType*          m_txBuffer = nullptr;
};


} // namespace win32

#endif // OURO_PLATFORM_WIN
