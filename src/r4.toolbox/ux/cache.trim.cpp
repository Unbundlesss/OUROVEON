//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#include "pch.h"
#include "ux/cache.trim.h"
#include "app/imgui.ext.h"

#include "filesys/fsutil.h"
#include "xp/open.url.h"

#include "endlesss/cache.stems.h"

namespace ux {

// ---------------------------------------------------------------------------------------------------------------------
struct CacheTrimState
{
    using recursive_iterator = fs::recursive_directory_iterator;

    CacheTrimState( const fs::path& cacheCommonRootPath )
        : m_cacheRoot( cacheCommonRootPath / endlesss::cache::Stems::getCachePathRoot( endlesss::cache::Stems::CacheVersion::Version2 ) )
    {
    }

    void imgui();

    fs::path            m_cacheRoot;
    recursive_iterator  m_iterator;
    
    uint32_t            m_filesTouched = 0;

    bool                m_initialisedSearch = false;
    bool                m_runningSearch = false;
};

// ---------------------------------------------------------------------------------------------------------------------
void CacheTrimState::imgui()
{
    const ImVec2 buttonSize( 240.0f, 32.0f );

    if ( !fs::exists( m_cacheRoot ) )
    {
        ImGui::TextWrapped( "Could not locate stem cache path. Perhaps you have never downloaded anything?" );
        ImGui::TextColored( colour::shades::errors.light(), "%s", m_cacheRoot.string().c_str() );
    }
    else
    {
        if ( m_initialisedSearch == false )
        {
            m_iterator = CacheTrimState::recursive_iterator( m_cacheRoot, std::filesystem::directory_options::skip_permission_denied );
            m_initialisedSearch = true;
        }
        else
        {
            if ( m_runningSearch == false )
            {
                if ( ImGui::Button( "Begin Analysis", buttonSize ) )
                {
                    m_runningSearch = true;
                }
            }
            else
            {
                ImGui::Text( "Files processed: %u", m_filesTouched );

                int32_t filesPerTick = 100;

                try
                {
                    std::error_code osError;
                    auto i = fs::begin( m_iterator );
                    for ( ; i != fs::end( m_iterator ); i = i.increment( osError ) )
                    {
                        if ( osError )
                            break;

                        if ( i->is_directory() )
                            continue;

                        if ( filesPerTick <= 0 )
                            break;
                        filesPerTick--;

                        const std::string filename = i->path().string();

                        m_filesTouched++;

                        int fd;
                        if ( (fd = open( filename.c_str(), O_RDONLY)) < 0 )
                        {
                            blog::error::cache( FMTX( "unable to open file : {}" ), filename );
                            m_runningSearch = false;
                        }

                        static constexpr std::size_t headerByteCount = 4;
                        char headerBytes[headerByteCount];
                        const auto bytesRead = read( fd, headerBytes, headerByteCount );
                        close( fd );

                        if ( bytesRead != headerByteCount )
                        {
                            blog::error::cache( FMTX( "unable to read 4 byte header from file : {}" ), filename );
                            m_runningSearch = false;
                        }

                        // yoinked from live.stem.cpp
                        const bool stemIsFLAC = (headerBytes[0] == 'f' && headerBytes[1] == 'L' && headerBytes[2] == 'a' && headerBytes[3] == 'C');
                        const bool stemIsOGG  = (headerBytes[0] == 'O' && headerBytes[1] == 'g' && headerBytes[2] == 'g' && headerBytes[3] == 'S');

                        if ( !stemIsFLAC && !stemIsOGG )
                        {
                            blog::cache( FMTX( "found invalid stem : {}" ), filename );
                        }
                    }
                    bool bHitEnd = i == fs::end( m_iterator );
                    if ( bHitEnd )
                        m_runningSearch = false;
                }
                catch ( std::exception& cEx )
                {
                    blog::error::cache( FMTX( "stopping cache walk on exception : {}" ), cEx.what() );
                    m_runningSearch = false;
                }
            }
        }
    }

    if ( ImGui::BottomRightAlignedButton( "Close", buttonSize ) )
    {
        ImGui::CloseCurrentPopup();
    }
}

// ---------------------------------------------------------------------------------------------------------------------
std::shared_ptr< CacheTrimState > createCacheTrimState( const fs::path& cacheCommonRootPath )
{
    return std::make_shared< CacheTrimState >( cacheCommonRootPath );
}

// ---------------------------------------------------------------------------------------------------------------------
void modalCacheTrim( const char* title, CacheTrimState& jamValidateState )
{
    const ImVec2 configWindowSize = ImVec2( 830.0f, 120.0f );
    ImGui::SetNextWindowContentSize( configWindowSize );

    ImGui::PushStyleColor( ImGuiCol_PopupBg, ImGui::GetStyleColorVec4( ImGuiCol_ChildBg ) );

    if ( ImGui::BeginPopupModal( title, nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize ) )
    {
        jamValidateState.imgui();

        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
}

} // namespace ux
