//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  cross platform loading of shared libraries (DLLs on windows etc)
//

#pragma once

namespace sys {

// ---------------------------------------------------------------------------------------------------------------------
struct DynLib
{
    using Instance = std::shared_ptr< DynLib >;

    ~DynLib();

    static absl::StatusOr< Instance > loadFromFile( const fs::path& libraryPath );

    template <typename TFunc>
    inline TFunc* resolve( std::string_view symbolName )
    {
        return reinterpret_cast<TFunc*>(resolveSymbol( std::move( symbolName ) ));
    }

protected:

    DynLib() = default;

private:

#if OURO_PLATFORM_WIN
    using LibraryHandle = HINSTANCE;
    using LibrarySymbol = FARPROC;
#else // LINUX / MAC
    using LibraryHandle = void*;
    using LibrarySymbol = void*;
#endif 

    static std::string getLastError();

    LibrarySymbol resolveSymbol( std::string_view symbolName );

    fs::path        m_originalPath;
    LibraryHandle   m_handle = nullptr;
};

} // namespace sys
