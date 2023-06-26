//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"

#include "base/paging.h"
#include "app/imgui.ext.h"
#include "app/module.frontend.fonts.h"
#include "endlesss/cache.shares.h"

#include "ux/userbox.h"
#include "ux/cache.shares.view.h"

using namespace endlesss;

namespace ux {

// ---------------------------------------------------------------------------------------------------------------------
struct SharedRiffView::State
{
    State( const std::string_view defaultUser )
        : m_user( defaultUser )
    {}

    void imgui( app::CoreGUI& coreGUI );

    void onNewDataFetched( cache::Shares::StatusOrData newData );

    ImGui::ux::UserBox          m_user;

    cache::Shares               m_sharesCache;
    cache::Shares::StatusOrData m_sharesData;

    std::atomic_bool            m_fetchInProgress = false;
    bool                        m_tryLoadFromCache = true;
    bool                        m_trySaveToCache = false;
};

// ---------------------------------------------------------------------------------------------------------------------
void SharedRiffView::State::imgui( app::CoreGUI& coreGUI )
{
    // try to restore from the cache if requested; usually on the first time through
    if ( m_tryLoadFromCache )
    {
        auto newData = std::make_shared<config::endlesss::SharedRiffsCache>();

        const auto cacheLoadResult = config::load( coreGUI, *newData );

        // only complain if the file was malformed, it being missing is not an error
        if ( cacheLoadResult != config::LoadResult::Success &&
             cacheLoadResult != config::LoadResult::CannotFindConfigFile )
        {
            // not the end of the world but note it regardless
            blog::error::cfg( "Unable to load shared riff cache" );
        }
        else
        {
            // stash if loaded ok
            m_sharesData = newData;
        }

        m_tryLoadFromCache = false;
    }

    // management window
    if ( ImGui::Begin( ICON_FA_SHARE_FROM_SQUARE " Shared Riffs###shared_riffs" ) )
    {
        // check we have online capabilities
        bool bCanSyncNewData = true;
        const auto& netConfigOpt = coreGUI.getEndlesssNetConfig();
        if ( !netConfigOpt.has_value() || !netConfigOpt->hasValidEndlesssAuth() )
        {
            bCanSyncNewData = false;
        }

        const bool bIsFetchingData = m_fetchInProgress;

        {
            ImGui::Scoped::Enabled se( bCanSyncNewData && !bIsFetchingData );
            if ( ImGui::Button( ICON_FA_ARROWS_ROTATE " Fetch Latest" ) )
            {
                m_fetchInProgress = true;

                m_sharesCache.fetchAsync(
                    netConfigOpt.value(),
                    m_user.getUsername(),
                    coreGUI.getTaskExecutor(),
                    [this]( cache::Shares::StatusOrData newData )
                    {
                        onNewDataFetched( newData );
                        m_fetchInProgress = false;
                    } );
            }
            ImGui::SameLine();

            ImGui::TextUnformatted( ICON_FA_USER_LARGE );
            ImGui::SameLine();
            m_user.imgui( coreGUI.getEndlesssPopulation() );
        }

        if ( bIsFetchingData )
        {
            ImGui::Spinner( "##syncing", true, ImGui::GetTextLineHeight() * 0.3f, 3.0f, 1.5f, ImGui::GetColorU32( ImGuiCol_Text ) );
            ImGui::SameLine();
            ImGui::TextUnformatted( "Fetching latest data from Endlesss ..." );
        }
        else
        {
            if ( m_sharesData.ok() )
            {
                const auto dataPtr = *m_sharesData;
                for ( std::size_t entry = 0; entry < dataPtr->m_count; entry++ )
                {
                    if ( dataPtr->m_private[entry] )
                    {
                        ImGui::TextUnformatted( ICON_FA_LOCK );
                        ImGui::SameLine();
                    }

                    ImGui::TextUnformatted( dataPtr->m_names[entry] );
                    // dataRef.m_names[entry]
                    // dataRef.m_images[entry]
                    // dataRef.m_riffIDs[entry]
                    // dataRef.m_jamIDs[entry]
                    // dataRef.m_timestamps[entry]
                    // dataRef.m_timestampDeltas[entry]
                }

                if ( m_trySaveToCache )
                {
                    const auto cacheSaveResult = config::save( coreGUI, *dataPtr );
                    if ( cacheSaveResult != config::SaveResult::Success )
                    {
                        blog::error::cfg( "Unable to save shared riff cache" );
                    }
                    m_trySaveToCache = false;
                }
            }
        }
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------------------------------------------------
void SharedRiffView::State::onNewDataFetched( cache::Shares::StatusOrData newData )
{
    // snapshot the data, mark it to save on next cycle through the imgui call if it was successful
    m_sharesData = newData;

    if ( m_sharesData.ok() )
        m_trySaveToCache = true;
}

// ---------------------------------------------------------------------------------------------------------------------
SharedRiffView::SharedRiffView( const std::string_view defaultUser )
    : m_state( std::make_unique<State>( defaultUser ) )
{
}

// ---------------------------------------------------------------------------------------------------------------------
SharedRiffView::~SharedRiffView()
{
}

// ---------------------------------------------------------------------------------------------------------------------
void SharedRiffView::imgui( app::CoreGUI& coreGUI )
{
    m_state->imgui( coreGUI );
}

} // namespace ux
