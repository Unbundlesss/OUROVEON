//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#include "pch.h"

#include "base/paging.h"
#include "base/eventbus.h"
#include "base/bimap.h"

#include "app/imgui.ext.h"
#include "app/module.frontend.fonts.h"
#include "colour/preset.h"

#include "xp/open.url.h"

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

        APP_EVENT_BIND_TO( MixerRiffChange );
        APP_EVENT_BIND_TO( BNSWasUpdated );
        APP_EVENT_BIND_TO( OperationComplete );
    }

    ~State()
    {
        APP_EVENT_UNBIND( OperationComplete );
        APP_EVENT_UNBIND( BNSWasUpdated );
        APP_EVENT_UNBIND( MixerRiffChange );
    }

    void event_OperationComplete( const events::OperationComplete* eventData );
    void event_MixerRiffChange( const events::MixerRiffChange* eventData );
    void event_BNSWasUpdated( const events::BNSWasUpdated* eventData );


    void imgui(
        app::CoreGUI& coreGUI,
        endlesss::services::JamNameResolveProvider& jamNameResolver,
        app::IRiffExportDispatcher& riffExportDispatcher );

    void modalBulkExport(
        const char* title,
        app::IRiffExportDispatcher& riffExportDispatcher );


    void onNewDataFetched( toolkit::Shares::StatusOrData newData );
    void onNewDataAssigned();

    void restartJamNameCacheResolution()
    {
        m_jamNameCacheSyncIndex = 0;
        m_jamNameCacheUpdate = !m_jamNameResolvedArray.empty();
    }

    enum class EAttachImage
    {
        AttachImage,
        IgnoreImage
    };

    endlesss::types::RiffIdentity getEntryRiffIdentity( const std::size_t entryIndex, const bool asSharedRiff, const EAttachImage eAttachImage ) const
    {
        const auto dataPtr = *m_sharesData;

        // assemble an appropriate riff identity for this entry in the table; used when queueing playback etc
        // `asSharedRiff` controls which riff/jam identity to use; the 'shared' one is a special handling, the 
        // riff ID and jam ID are specific for resolving the shared data and for playback outside of the origin
        // jam (which may have been private)
        // .. if `asSharedRiff` is false, we use the original riff ID and origin jam ID (for navigation in jam view, for example)
        const bool bIsPersonalJam = dataPtr->m_jamIDs[entryIndex].empty();  // no jam name, it's the users'

        const endlesss::types::JamCouchID originJam( 
            bIsPersonalJam ? 
                dataPtr->m_username : 
                dataPtr->m_jamIDs[entryIndex] );

        const endlesss::types::RiffCouchID sharedRiffToEnqueue( dataPtr->m_sharedRiffIDs[entryIndex].c_str() );
        const endlesss::types::RiffCouchID originRiffToEnqueue( dataPtr->m_riffIDs[entryIndex] );

        endlesss::types::IdentityCustomNaming customNaming;

        // encode an export/display name from the active username that shared the riff
        // .. there is no way to get that info during network resolve, so we have to tag it here
        customNaming.m_jamDisplayName = fmt::format( FMTX( "{}_{}" ), endlesss::types::Constants::SharedRiffJam(), dataPtr->m_username );

        auto identityResult = endlesss::types::RiffIdentity(
            asSharedRiff ? endlesss::types::Constants::SharedRiffJam() : originJam,
            asSharedRiff ? sharedRiffToEnqueue : originRiffToEnqueue,
            std::move( customNaming )
        );

        if ( eAttachImage == EAttachImage::AttachImage )
        {
            // bolt on the image URL in case this riff gets exported and we wanna use it
            identityResult.setAttachedImageURL( dataPtr->m_images[entryIndex] );
        }

        return identityResult;
    }
    

    using SharedRiffExportOperations = base::BiMap< base::OperationID, endlesss::types::SharedRiffCouchID >;


    api::NetConfiguration::Shared   m_networkConfiguration;
    base::EventBusClient            m_eventBusClient;

    std::atomic_bool                m_fetchInProgress = false;
    int8_t                          m_busySpinnerIndex = 0;

    endlesss::types::RiffCouchID    m_currentlyPlayingRiffID;
    endlesss::types::RiffCouchIDSet m_enqueuedRiffIDs;

    base::EventListenerID           m_eventLID_OperationComplete    = base::EventListenerID::invalid();
    base::EventListenerID           m_eventLID_MixerRiffChange      = base::EventListenerID::invalid();
    base::EventListenerID           m_eventLID_BNSWasUpdated        = base::EventListenerID::invalid();

    ImGui::ux::UserSelector         m_user;

    toolkit::Shares                 m_sharesCache;
    toolkit::Shares::StatusOrData   m_sharesData;

    SharedRiffExportOperations      m_sharedRiffExportOperationsMap;
    std::size_t                     m_bulkExportIndex = 0;
    bool                            m_bulkExportIncludeImages = true;
    bool                            m_bulkExportRunning = false;
    base::OperationID               m_bulkExportLastOp = base::OperationID::invalid();

    
    std::string                     m_lastSyncTimestampString;

    std::vector< std::string >      m_jamNameResolvedArray;
    uint64_t                        m_jamNameCacheUpdateChangeIndex = std::numeric_limits<uint64_t>::max();
    std::size_t                     m_jamNameCacheSyncIndex = 0;
    bool                            m_jamNameCacheUpdate = false;

    bool                            m_currentlyPlayingSharedRiff = false;

    bool                            m_tryLoadFromCache = true;
    bool                            m_trySaveToCache = false;
};


// ---------------------------------------------------------------------------------------------------------------------
void SharedRiffView::State::event_OperationComplete( const events::OperationComplete* eventData )
{
    const auto variant = base::Operations::variantFromID( eventData->m_id );

    if ( variant == app::IRiffExportDispatcher::OV_RiffExport )
    {
        if ( m_sharedRiffExportOperationsMap.hasKey( eventData->m_id ) )
        {
            m_sharedRiffExportOperationsMap.remove( eventData->m_id );
        }
        if ( eventData->m_id == m_bulkExportLastOp )
            m_bulkExportLastOp = base::OperationID::invalid();
    }
}

// ---------------------------------------------------------------------------------------------------------------------
void SharedRiffView::State::event_MixerRiffChange( const events::MixerRiffChange* eventData )
{
    // might be a empty riff, only track actual riffs
    if ( eventData->m_riff != nullptr )
    {
        m_currentlyPlayingRiffID = eventData->m_riff->m_riffData.riff.couchID;
        m_enqueuedRiffIDs.erase( m_currentlyPlayingRiffID );
    }
    else
        m_currentlyPlayingRiffID = endlesss::types::RiffCouchID{};
}

// ---------------------------------------------------------------------------------------------------------------------
void SharedRiffView::State::event_BNSWasUpdated( const events::BNSWasUpdated* eventData )
{
    m_jamNameCacheUpdateChangeIndex = eventData->m_changeIndex;
    restartJamNameCacheResolution();
}

// ---------------------------------------------------------------------------------------------------------------------
void SharedRiffView::State::modalBulkExport( const char* title, app::IRiffExportDispatcher& riffExportDispatcher )
{
    const ImVec2 configWindowSize = ImVec2( 700.0f, 180.0f );
    ImGui::SetNextWindowContentSize( configWindowSize );

    const ImVec4 colourJamDisabled = GImGui->Style.Colors[ImGuiCol_TextDisabled];

    ImGui::PushStyleColor( ImGuiCol_PopupBg, ImGui::GetStyleColorVec4( ImGuiCol_ChildBg ) );

    if ( ImGui::BeginPopupModal( title, nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize ) )
    {
        const auto dataPtr = *m_sharesData;

        const ImVec2 buttonSize( 220.0f, 32.0f );

        ImGui::Checkbox( "Include Cover Art Images", &m_bulkExportIncludeImages );
        ImGui::Spacing();
        ImGui::Spacing();

        float dispatchProgress = ( 1.0f / (float)dataPtr->m_count ) * (float)m_bulkExportIndex;
        ImGui::Text( "Bulk Export Progress (%i riffs)", dataPtr->m_count );
        ImGui::ProgressBar( dispatchProgress );

        if ( !m_bulkExportRunning )
        {
            if ( ImGui::Button( "     Begin Export     " ) )
            {
                m_bulkExportIndex = 0;
                m_bulkExportRunning = true;
            }
        }
        else
        {
            ImGui::Scoped::ColourButton cb( colour::shades::errors, colour::shades::white );
            if ( ImGui::Button( "   Cancel & Restart   " ) )
            {
                m_bulkExportIndex = 0;
                m_bulkExportRunning = false;
            }
        }

        if ( m_bulkExportRunning )
        {
            ImGui::SameLine();
            ImGui::Spinner( "##syncing", true, ImGui::GetTextLineHeight() * 0.4f, 3.0f, 0.0f, ImGui::GetColorU32( ImGuiCol_Text ) );

            if ( m_bulkExportLastOp.isValid() == false )
            {
                const EAttachImage eAttachImage = m_bulkExportIncludeImages ? EAttachImage::AttachImage : EAttachImage::IgnoreImage;

                const auto& sharedRiffKey = dataPtr->m_sharedRiffIDs[m_bulkExportIndex];
                const auto operationID = riffExportDispatcher.dispatchRiffExportAsync( getEntryRiffIdentity( m_bulkExportIndex, true, eAttachImage ) );

                m_bulkExportLastOp = operationID;
                m_sharedRiffExportOperationsMap.add( operationID, sharedRiffKey );

                m_bulkExportIndex++;
                if ( m_bulkExportIndex >= dataPtr->m_count )
                {
                    m_bulkExportRunning = false;
                }
            }
        }

        ImGui::Spacing();
        ImGui::Spacing();

        if ( ImGui::BottomRightAlignedButton( m_bulkExportRunning ? "  Close (Pause)  " : "      Close      ", buttonSize ) )
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
}


// ---------------------------------------------------------------------------------------------------------------------
void SharedRiffView::State::imgui(
    app::CoreGUI& coreGUI,
    endlesss::services::JamNameResolveProvider& jamNameResolver,
    app::IRiffExportDispatcher& riffExportDispatcher )
{
    // try to restore from the cache if requested; usually on the first time through, done here as we need CoreGUI / path provider
    if ( m_tryLoadFromCache )
    {
        auto newData = std::make_shared<config::endlesss::SharedRiffsCache>();

        const auto cacheLoadResult = config::load( coreGUI, *newData );

        // only complain if the file was malformed, it being missing is not an error
        if ( cacheLoadResult != config::LoadResult::Success &&
             cacheLoadResult != config::LoadResult::CannotFindConfigFile )
        {
            // not the end of the world but note it regardless
            blog::error::cfg( FMTX( "Unable to load shared riff cache [{}]" ), LoadResultToString( cacheLoadResult ) );
        }
        else
        {
            // stash if loaded ok
            m_sharesData = newData;
            m_user.setUsername( newData->m_username );

            onNewDataAssigned();
        }

        m_tryLoadFromCache = false;
    }

    // iteratively resolve jam names, one a frame
    // this process can be restarted if the jam name cache providers send out a message that new data has arrived
    if ( m_jamNameCacheUpdate && m_sharesData.ok() )
    {
        const auto dataPtr = *m_sharesData;

        // don't try and resolve non band##### IDs
        if ( dataPtr->m_personal[m_jamNameCacheSyncIndex] )
        {
            m_jamNameResolvedArray[m_jamNameCacheSyncIndex] = "[ personal ]";
        }
        else
        {
            // ask jame name services for data
            const auto bJamNameFound = jamNameResolver->lookupJamName(
                dataPtr->m_jamIDs[m_jamNameCacheSyncIndex],
                m_jamNameResolvedArray[m_jamNameCacheSyncIndex]
            );

            // if we get a cache miss, issue a fetch request to go plumb the servers for answers
            if ( bJamNameFound == endlesss::services::IJamNameResolveService::LookupResult::NotFound )
            {
                m_eventBusClient.Send< ::events::BNSCacheMiss >(
                    dataPtr->m_jamIDs[m_jamNameCacheSyncIndex] );
            }
        }

        m_jamNameCacheSyncIndex++;
        if ( m_jamNameCacheSyncIndex >= m_jamNameResolvedArray.size() )
        {
            m_jamNameCacheUpdate = false;
        }
    }

    bool bFoundAPlayingRiffInTable = false;

    // management window
    if ( ImGui::Begin( ICON_FA_SHARE_FROM_SQUARE " Shared Riffs###shared_riffs" ) )
    {
        // check we have suitable network access to fetch fresh data; doesn't have to be fully authenticated, you'll just
        // miss out on your own private shares in that case, the API fetch code will choose what to do
        const bool bCanSyncNewData = m_networkConfiguration->hasAccess( api::NetConfiguration::Access::Public );
        const bool bIsFetchingData = m_fetchInProgress;

        // is the username on screen the one we have data for? if not, note that somehow so the user knows to Fetch Latest for their choice
        const bool bCurrentDataSetIsForTheUsernameInThePicker = (m_sharesData.ok() && (*m_sharesData)->m_username == m_user.getUsername());

        // choose a user and fetch latest data on request
        {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted( ICON_FA_USER );
            ImGui::SameLine();
            m_user.imgui( "username", coreGUI.getEndlesssPopulation(), ImGui::ux::UserSelector::cDefaultWidthForUserSize );
            ImGui::SameLine();
            {
                ImGui::Scoped::Enabled se( bCanSyncNewData && !bIsFetchingData && !m_user.isEmpty() );
                if ( ImGui::Button( " " ICON_FA_ARROWS_ROTATE " Sync Latest " ) )
                {
                    m_fetchInProgress = true;

                    coreGUI.getTaskExecutor().run( 
                        m_sharesCache.taskFetchLatest(
                            *m_networkConfiguration,
                            m_user.getUsername(),
                            [this]( toolkit::Shares::StatusOrData newData )
                            {
                                onNewDataFetched( newData );
                                m_fetchInProgress = false;
                            } )
                    );
                }
            }
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
                // indicate we're doing name resolution by showing busy spinner in the table header
                static constexpr std::array cJamNameWorkerTitles =
                {
                    "Jam###Jam",
                    "Jam (\\)###Jam",
                    "Jam (|)###Jam",
                    "Jam (/)###Jam",
                    "Jam (-)###Jam",
                };

                // use simple increment, mask out some higher bits and loop to keep it 0..3
                m_busySpinnerIndex++;
                const uint8_t loopedSpinnerIndex = 1 + ( ( ( m_busySpinnerIndex & 0b11000 ) >> 3 ) & 0b0011 );

                bool bScrollToPlaying = false;

                const auto dataPtr = *m_sharesData;

                if ( dataPtr->m_count == 0 )
                {
                    ImGui::TextDisabled( "no data downloaded; select user and sync" );
                }
                else
                {
                    if ( bCurrentDataSetIsForTheUsernameInThePicker )
                        ImGui::Text( "%u riffs, synced %s", static_cast<uint32_t>( dataPtr->m_count ), m_lastSyncTimestampString.c_str() );
                    else
                        ImGui::TextColored( colour::shades::callout.neutral(), ICON_FA_CIRCLE_EXCLAMATION " showing data for user '%s', re-sync required", dataPtr->m_username.c_str() );

                    // add button that brings any currently playing riff into view inside the table
                    ImGui::RightAlignSameLine( 100.0f );
                    {
                        if ( ImGui::Button( ICON_FA_FLOPPY_DISK " ALL" ) )
                        {
                            coreGUI.activateModalPopup( "Bulk Export All Shared", [&, this]( const char* title )
                                {
                                    modalBulkExport( title, riffExportDispatcher );
                                });
                        }
                        ImGui::CompactTooltip( "Start exporting ALL the shared riffs" );
                    }
                    ImGui::SameLine(0, 10.0f);
                    {
                        ImGui::Scoped::Enabled scrollButtonAvailable( m_currentlyPlayingSharedRiff );
                        bScrollToPlaying = ImGui::IconButton( ICON_FA_ARROWS_DOWN_TO_LINE );
                        ImGui::CompactTooltip( "Scroll to currently playing riff" );
                    }

                    ImGui::Spacing();

                    static ImVec2 buttonSizeMidTable( 31.0f, 22.0f );

                    if ( ImGui::BeginChild( "##data_child" ) )
                    {
                        ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, { 4.0f, 0.0f } );

                        if ( ImGui::BeginTable( "##shared_riff_table", 6,
                            ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_Borders |
                            ImGuiTableFlags_RowBg   |
                            ImGuiTableFlags_NoSavedSettings ) )
                        {
                            ImGui::TableSetupScrollFreeze( 0, 1 );  // top row always visible

                            ImGui::TableSetupColumn( "Play", ImGuiTableColumnFlags_WidthFixed,  32.0f );
                            ImGui::TableSetupColumn( "Name", ImGuiTableColumnFlags_WidthStretch, 0.5f );
                            ImGui::TableSetupColumn( "Save", ImGuiTableColumnFlags_WidthFixed,  32.0f );    // export to disk
                            ImGui::TableSetupColumn( "Web",  ImGuiTableColumnFlags_WidthFixed,  32.0f );    // launch web player
                            ImGui::TableSetupColumn( "Find", ImGuiTableColumnFlags_WidthFixed,  32.0f );    // instigate navigation in jam view, if possible
                            ImGui::TableSetupColumn( m_jamNameCacheUpdate ? cJamNameWorkerTitles[loopedSpinnerIndex] : cJamNameWorkerTitles[0],
                                                             ImGuiTableColumnFlags_WidthStretch, 0.5f);
                            ImGui::TableHeadersRow();

                            for ( std::size_t entry = 0; entry < dataPtr->m_count; entry++ )
                            {
                                const bool bIsPrivate       = dataPtr->m_private[entry];
                                const bool bIsPersonal      = dataPtr->m_personal[entry];
                                const bool bIsPlaying       = dataPtr->m_riffIDs[entry] == m_currentlyPlayingRiffID;
                                const bool bRiffWasEnqueued = m_enqueuedRiffIDs.contains( dataPtr->m_riffIDs[entry] );



                                // keep track of if any of the shared riffs are considered active, used to enable scroll-to-playing button above
                                // (done backwards due to nature of imguis)
                                bFoundAPlayingRiffInTable |= bIsPlaying;

                                ImGui::PushID( (int32_t)entry );

                                ImGui::TableNextColumn();
                                {
                                    // show some indication that work is in progress for this entry if it's been asked to play
                                    if ( bRiffWasEnqueued )
                                    {
                                        ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg1, ImGui::GetPulseColour( 0.25f ) );
                                    }
                                    ImGui::Dummy( { 0, 0 } );
                                    {
                                        // riff enqueue-to-play button, disabled when in-flight
                                        ImGui::Scoped::Disabled disabledButton( bRiffWasEnqueued );
                                        ImGui::Scoped::ToggleButton highlightButton( bIsPlaying, true );
                                        if ( ImGui::PrecisionButton( bRiffWasEnqueued ? ICON_FA_CIRCLE_CHEVRON_DOWN : ICON_FA_PLAY, buttonSizeMidTable, 1.0f ) )
                                        {
                                            m_eventBusClient.Send< ::events::EnqueueRiffPlayback >( getEntryRiffIdentity( entry, true, EAttachImage::IgnoreImage ) );

                                            // enqueue the riff ID, not the *shared* riff ID as the default riff ID is what will
                                            // be flowing back through "riff now being played" messages
                                            m_enqueuedRiffIDs.emplace( dataPtr->m_riffIDs[entry] );
                                        }

                                        if ( bIsPlaying && bScrollToPlaying )
                                            ImGui::ScrollToItem( ImGuiScrollFlags_KeepVisibleCenterY );
                                    }
                                }
                                ImGui::TableNextColumn();
                                ImGui::AlignTextToFramePadding();
                                {
                                    // draw the riff name
                                    if ( bIsPrivate )
                                    {
                                        ImGui::TextUnformatted( ICON_FA_LOCK );
                                        ImGui::SameLine();
                                    }
                                    ImGui::TextUnformatted( dataPtr->m_names[entry] );
                                }
                                ImGui::TableNextColumn();
                                {
                                    ImGui::Dummy( { 0, 0 } );
                                    
                                    const auto& sharedRiffKey = dataPtr->m_sharedRiffIDs[entry];

                                    ImGui::Scoped::Disabled sd( m_sharedRiffExportOperationsMap.hasValue( sharedRiffKey ) );
                                    if ( ImGui::PrecisionButton( ICON_FA_FLOPPY_DISK, buttonSizeMidTable, 1.0f ) )
                                    {
                                        const auto operationID = riffExportDispatcher.dispatchRiffExportAsync( getEntryRiffIdentity( entry, true, EAttachImage::AttachImage ) );
                                        m_sharedRiffExportOperationsMap.add( operationID, sharedRiffKey );
                                    }
                                }
                                ImGui::TableNextColumn();
                                {
                                    ImGui::Dummy( { 0, 0 } );
                                    if ( ImGui::PrecisionButton( ICON_FA_LINK, buttonSizeMidTable, 1.0f ) )
                                    {
                                        // cross-platform launch a browser to navigate to the Endlesss riff web player
                                        const auto webPlayerURL = fmt::format( FMTX( "https://endlesss.fm/{}/?rifffId={}" ), m_user.getUsername(), dataPtr->m_sharedRiffIDs[entry] );
                                        xpOpenURL( webPlayerURL.c_str() );
                                    }
                                }
                                ImGui::TableNextColumn();
                                {
                                    ImGui::Dummy( { 0, 0 } );
                                    if ( ImGui::PrecisionButton( ICON_FA_GRIP, buttonSizeMidTable, 1.0f ) )
                                    {
                                        // dispatch a request to navigate this this riff, if we can find it
                                        m_eventBusClient.Send< ::events::RequestNavigationToRiff >( getEntryRiffIdentity( entry, false, EAttachImage::IgnoreImage ) );
                                    }
                                }
                                ImGui::TableNextColumn();
                                ImGui::AlignTextToFramePadding();
                                {
                                    // render the jam name, if we have one
                                    {
                                        const auto jamID = dataPtr->m_jamIDs[entry];

                                        // double-wrap tooltip so we only do the time conversion / string build on hover
                                        // shows the share-time using past-tense formatting (personally I find this more useful than the stuff endlesss puts on the website)
                                        ImGui::TextDisabled( ICON_FA_CLOCK );
                                        if ( ImGui::IsItemHovered( ImGuiHoveredFlags_DelayNormal ) )
                                        {
                                            const auto shareTimeUnix = spacetime::InSeconds( std::chrono::seconds{ dataPtr->m_timestamps[entry] } );
                                            const auto cacheTimeDelta = spacetime::calculateDeltaFromNow( shareTimeUnix ).asPastTenseString( 2 );
                                            ImGui::CompactTooltip( cacheTimeDelta );
                                        }
                                        ImGui::SameLine();

                                        // click on ID to copy it into the clipboard for debug purposes
                                        ImGui::TextDisabled( "ID" );
                                        if ( ImGui::IsItemClicked() )
                                        {
                                            ImGui::SetClipboardText( jamID.c_str() );
                                        }
                                        ImGui::CompactTooltip( jamID.c_str() );
                                        ImGui::SameLine( 0, 12.0f );

                                        // origin jam, potentially [username] personal / solo jam
                                        if ( bIsPrivate || bIsPersonal )
                                            ImGui::TextColored( colour::shades::callout.neutral(), "%s", m_jamNameResolvedArray[entry].c_str());
                                        else
                                            ImGui::TextUnformatted( m_jamNameResolvedArray[entry]);
                                    }
                                }
                                ImGui::PopID();
                            }

                            ImGui::EndTable();
                        }

                        ImGui::PopStyleVar();
                    }
                    ImGui::EndChild();
                }

                // if set in onNewDataFetched(), page any new data out to disk to cache the results across sessions
                if ( m_trySaveToCache )
                {
                    const auto cacheSaveResult = config::save( coreGUI, *dataPtr );
                    if ( cacheSaveResult != config::SaveResult::Success )
                    {
                        blog::error::cfg( FMTX( "Unable to save shared riff cache" ) );
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

    m_currentlyPlayingSharedRiff = bFoundAPlayingRiffInTable;
}

// ---------------------------------------------------------------------------------------------------------------------
void SharedRiffView::State::onNewDataFetched( toolkit::Shares::StatusOrData newData )
{
    // snapshot the data, mark it to save on next cycle through the imgui call if it was successful
    m_sharesData = newData;

    onNewDataAssigned();

    if ( m_sharesData.ok() )
    {
        m_trySaveToCache = true;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
void SharedRiffView::State::onNewDataAssigned()
{
    // clear out jam name resolution array, ready to refill
    m_jamNameResolvedArray.clear();

    if ( m_sharesData.ok() )
    {
        m_jamNameResolvedArray.resize( (*m_sharesData)->m_count );
        m_lastSyncTimestampString = spacetime::datestampStringFromUnix( (*m_sharesData)->m_lastSyncTime );
    }
    else
    {
        m_lastSyncTimestampString.clear();
    }

    restartJamNameCacheResolution();
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
void SharedRiffView::imgui(
    app::CoreGUI& coreGUI,
    endlesss::services::JamNameResolveProvider& jamNameResolver,
    app::IRiffExportDispatcher& riffExportDispatcher )
{
    m_state->imgui( coreGUI, jamNameResolver, riffExportDispatcher );
}

} // namespace ux
