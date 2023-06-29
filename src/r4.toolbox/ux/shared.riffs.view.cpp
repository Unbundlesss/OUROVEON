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
#include "base/eventbus.h"
#include "app/imgui.ext.h"
#include "app/module.frontend.fonts.h"
#include "endlesss/core.types.h"
#include "endlesss/toolkit.shares.h"

#include "mix/common.h"

#include "ux/user.selector.h"
#include "ux/shared.riffs.view.h"

using namespace endlesss;

namespace ux {

// ---------------------------------------------------------------------------------------------------------------------
struct SharedRiffView::State
{
    State( api::NetConfiguration::Shared& networkConfig, base::EventBusClient eventBus )
        : m_networkConfiguration( networkConfig )
        , m_eventBusClient( std::move( eventBus ) )
    {
        if ( m_networkConfiguration->hasAccess( api::NetConfiguration::Access::Authenticated ) )
        {
            m_user.setUsername( m_networkConfiguration->auth().user_id );
        }

        m_eventListenerMixerRiffChange = m_eventBusClient.addListener(
            events::MixerRiffChange::ID,
            [this]( const base::IEvent& eventPtr ) { event_MixerRiffChange( eventPtr ); } );
    }

    ~State()
    {
        checkedCoreCall( "~SharedRiffView::State", [this] { return m_eventBusClient.removeListener(m_eventListenerMixerRiffChange); });
    }

    void event_MixerRiffChange( const base::IEvent& eventPtr );


    void imgui( app::CoreGUI& coreGUI );

    void onNewDataFetched( toolkit::Shares::StatusOrData newData );


    api::NetConfiguration::Shared   m_networkConfiguration;
    base::EventBusClient            m_eventBusClient;

    base::EventListenerID           m_eventListenerMixerRiffChange;
    endlesss::types::RiffCouchID    m_currentlyPlayingRiffID;


    ImGui::ux::UserSelector         m_user;

    toolkit::Shares                 m_sharesCache;
    toolkit::Shares::StatusOrData   m_sharesData;
    
    std::string                     m_lastSyncTimestampString;

    std::atomic_bool                m_fetchInProgress = false;
    bool                            m_tryLoadFromCache = true;
    bool                            m_trySaveToCache = false;
};

// ---------------------------------------------------------------------------------------------------------------------
void SharedRiffView::State::event_MixerRiffChange( const base::IEvent& eventPtr )
{
    ABSL_ASSERT( eventPtr.getID() == events::MixerRiffChange::ID );

    const events::MixerRiffChange* riffChangeEvent = dynamic_cast<const events::MixerRiffChange*>(&eventPtr);
    ABSL_ASSERT( riffChangeEvent != nullptr );

    // might be a empty riff, only track actual riffs
    if ( riffChangeEvent->m_riff != nullptr )
        m_currentlyPlayingRiffID = riffChangeEvent->m_riff->m_riffData.riff.couchID;
    else
        m_currentlyPlayingRiffID = endlesss::types::RiffCouchID{};
}

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
            m_user.setUsername( newData->m_username );
            m_lastSyncTimestampString = spacetime::datestampStringFromUnix( newData->m_lastSyncTime );
        }

        m_tryLoadFromCache = false;
    }

    // management window
    if ( ImGui::Begin( ICON_FA_SHARE_FROM_SQUARE " Shared Riffs###shared_riffs" ) )
    {
        // check we have suitable network access to fetch fresh data
        const bool bCanSyncNewData = m_networkConfiguration->hasAccess( api::NetConfiguration::Access::Authenticated );

        const bool bIsFetchingData = m_fetchInProgress;

        {
            ImGui::Scoped::Enabled se( bCanSyncNewData && !bIsFetchingData );
            if ( ImGui::Button( ICON_FA_ARROWS_ROTATE " Fetch Latest" ) )
            {
                m_fetchInProgress = true;

                coreGUI.getTaskExecutor().run( std::move(
                    m_sharesCache.taskFetchLatest(
                        *m_networkConfiguration,
                        m_user.getUsername(),
                        [this]( toolkit::Shares::StatusOrData newData )
                        {
                            onNewDataFetched( newData );
                            m_fetchInProgress = false;
                        } ) 
                ));
            }
            ImGui::SameLine();

            ImGui::TextUnformatted( ICON_FA_USER_LARGE );
            ImGui::SameLine();
            m_user.imgui( coreGUI.getEndlesssPopulation(), 240.0f );
            ImGui::SameLine();
        }

        if ( bIsFetchingData )
        {
            ImGui::Spinner( "##syncing", true, ImGui::GetTextLineHeight() * 0.4f, 3.0f, 1.5f, ImGui::GetColorU32( ImGuiCol_Text ) );
            ImGui::SameLine();
            ImGui::TextUnformatted( " Working ..." );
        }
        else
        {
            if ( m_sharesData.ok() )
            {
                const auto dataPtr = *m_sharesData;

                ImGui::Text( "%i riffs, synced %s", dataPtr->m_count, m_lastSyncTimestampString.c_str() );

                ImGui::Spacing();

                static ImVec2 buttonSizeMidTable( 29.0f, 21.0f );

                if ( ImGui::BeginChild( "##data_child" ) )
                {
                    ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, { 4.0f, 2.0f } );

                    if ( ImGui::BeginTable( "##shared_riff_table", 3,
                        ImGuiTableFlags_ScrollY |
                        ImGuiTableFlags_Borders |
                        ImGuiTableFlags_RowBg |
                        ImGuiTableFlags_NoSavedSettings ) )
                    {
                        ImGui::TableSetupScrollFreeze( 0, 1 );  // top row always visible

                        ImGui::TableSetupColumn( "Play", ImGuiTableColumnFlags_WidthFixed, 32.0f );
                        ImGui::TableSetupColumn( "Name", ImGuiTableColumnFlags_WidthStretch, 0.6f );
                        ImGui::TableSetupColumn( "Jam", ImGuiTableColumnFlags_WidthStretch, 0.4f );
                        ImGui::TableHeadersRow();

                        for ( std::size_t entry = 0; entry < dataPtr->m_count; entry++ )
                        {
                            const bool bIsPrivate  = dataPtr->m_private[entry];
                            const bool bIsPersonal = dataPtr->m_personal[entry];
                            const bool bIsPlaying  = dataPtr->m_riffIDs[entry] == m_currentlyPlayingRiffID;

                            ImGui::PushID( (int32_t)entry );
                            ImGui::TableNextColumn();

                            {
                                ImGui::Scoped::ToggleButton highlightButton( bIsPlaying, true );
                                if ( ImGui::PrecisionButton( ICON_FA_PLAY, buttonSizeMidTable, 1.0f ) )
                                {
                                    coreGUI.getEventBusClient().Send< ::events::EnqueueRiffPlayback >(
                                        endlesss::types::Constants::SharedRiffJam(),
                                        endlesss::types::RiffCouchID{ dataPtr->m_sharedRiffIDs[entry].c_str() } );
                                }
                            }
                            ImGui::TableNextColumn();

                            ImGui::Dummy( ImVec2( 0.0f, 1.0f ) );
                            if ( bIsPrivate )
                            {
                                ImGui::TextUnformatted( ICON_FA_LOCK );
                                ImGui::SameLine();
                            }
                            if ( bIsPersonal )
                            {
                                ImGui::TextUnformatted( ICON_FA_CIRCLE_USER );
                                ImGui::SameLine();
                            }
                            ImGui::TextUnformatted( dataPtr->m_names[entry] );
                            
                            ImGui::TableNextColumn();

                            ImGui::Dummy( ImVec2( 0.0f, 1.0f ) );
                            ImGui::TextUnformatted( dataPtr->m_jamIDs[entry] );

                            ImGui::PopID();
                        }

                        ImGui::EndTable();
                    }

                    ImGui::PopStyleVar();
                }
                ImGui::EndChild();

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
            else
            {
                ImGui::TextColored( ImGui::GetErrorTextColour(), ICON_FA_TRIANGLE_EXCLAMATION " Failed" );
                ImGui::CompactTooltip( m_sharesData.status().ToString().c_str() );
            }
        }
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------------------------------------------------
void SharedRiffView::State::onNewDataFetched( toolkit::Shares::StatusOrData newData )
{
    // snapshot the data, mark it to save on next cycle through the imgui call if it was successful
    m_sharesData = newData;

    if ( m_sharesData.ok() )
    {
        m_lastSyncTimestampString = spacetime::datestampStringFromUnix( (*m_sharesData)->m_lastSyncTime );
        m_trySaveToCache = true;
    }
    else
    {
        m_lastSyncTimestampString.clear();
    }
}

// ---------------------------------------------------------------------------------------------------------------------
SharedRiffView::SharedRiffView( api::NetConfiguration::Shared& networkConfig, base::EventBusClient eventBus )
    : m_state( std::make_unique<State>( networkConfig, std::move( eventBus ) ) )
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
