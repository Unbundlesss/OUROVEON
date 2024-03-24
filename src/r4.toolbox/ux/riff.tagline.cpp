//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#include "pch.h"
#include "base/eventbus.h"

#include "app/imgui.ext.h"
#include "app/module.frontend.fonts.h"
#include "colour/preset.h"

#include "endlesss/core.types.h"

#include "ux/riff.tagline.h"

namespace ux {

// ---------------------------------------------------------------------------------------------------------------------
struct TagLine::State : TagLineToolProvider
{
    State( base::EventBusClient eventBus )
        : m_eventBusClient( std::move( eventBus ) )
    {
        APP_EVENT_BIND_TO( RiffTagAction );
    }

    ~State()
    {
        APP_EVENT_UNBIND( RiffTagAction );
    }

    void event_RiffTagAction( const events::RiffTagAction* eventData );

    void imgui(
        endlesss::live::RiffPtr& currentRiffPtr,
        const endlesss::toolkit::Warehouse* warehouseAccess,
        TagLineToolProvider& toolProvider );


    base::EventBusClient            m_eventBusClient;

    base::EventListenerID           m_eventLID_RiffTagAction = base::EventListenerID::invalid();

    bool                            m_debugToggleDetails = false;

    struct CurrentData
    {
        endlesss::types::RiffCouchID    m_riffID;
        bool                            m_riffIsTagged = false;
        endlesss::types::RiffTag        m_riffTag;

    }                               m_currentData;
};

// ---------------------------------------------------------------------------------------------------------------------
void TagLine::State::event_RiffTagAction( const events::RiffTagAction* eventData )
{
    if ( eventData->m_tag.m_riff == m_currentData.m_riffID )
    {
        m_currentData = {};
    }
}

// ---------------------------------------------------------------------------------------------------------------------
void TagLine::State::imgui(
    endlesss::live::RiffPtr& currentRiffPtr,
    const endlesss::toolkit::Warehouse* warehouseAccess,
    TagLineToolProvider& toolProvider
)
{
    // take temporary copy of the shared pointer, in case it gets modified mid-tick by the mixer
    endlesss::live::RiffPtr localRiffPtr = currentRiffPtr;

    const auto currentRiff  = localRiffPtr.get();
    const bool currentRiffIsValid = ( currentRiff &&
                                      currentRiff->m_syncState == endlesss::live::Riff::SyncState::Success );

    const bool bAddRiffTaggingTools = ( warehouseAccess != nullptr );

    // on null riff, reset the tag cache contents
    if ( !currentRiffIsValid )
    {
        m_currentData = {};
    }
    else
    {
        // the current riff is not what we were examining last, update ourselves
        if ( m_currentData.m_riffID != currentRiff->m_riffData.riff.couchID )
        {
            m_currentData = {};
            m_currentData.m_riffID = currentRiff->m_riffData.riff.couchID;

            if ( bAddRiffTaggingTools )
            {
                m_currentData.m_riffIsTagged = warehouseAccess->isRiffTagged( m_currentData.m_riffID, &m_currentData.m_riffTag );
            }
            else
            {
                m_currentData.m_riffTag = {};
                m_currentData.m_riffIsTagged = false;
            }

            // if the riff was not already tagged, we have to set up some default tag data from the incoming data
            // so that it inserts correctly later
            if ( !m_currentData.m_riffIsTagged )
            {
                m_currentData.m_riffTag.m_riff      = m_currentData.m_riffID;
                m_currentData.m_riffTag.m_jam       = currentRiff->m_riffData.jam.couchID;
                m_currentData.m_riffTag.m_timestamp = currentRiff->m_riffData.riff.creationTimeUnix;
            }
        }
    }

    {
        static const ImVec2 ChunkyIconButtonSize = ImVec2( 48.0f, 48.0f );

        const bool bIsTaggedAtF0 = currentRiffIsValid && m_currentData.m_riffIsTagged && (m_currentData.m_riffTag.m_favour == 0);
        const bool bIsTaggedAtF1 = currentRiffIsValid && m_currentData.m_riffIsTagged && (m_currentData.m_riffTag.m_favour == 1);

        if ( bAddRiffTaggingTools )
        {
            ImGui::Scoped::Enabled se( currentRiffIsValid );

            {
                ImGui::Scoped::ColourButton tb( colour::shades::tag_lvl_1, bIsTaggedAtF0 );
                if ( ImGui::Button( ICON_FA_ANGLE_UP, ChunkyIconButtonSize ) ||
                    ImGui::Shortcut( ImGuiModFlags_Ctrl, ImGuiKey_1, false ) )
                {
                    if ( bIsTaggedAtF0 )
                    {
                        m_eventBusClient.Send< ::events::RiffTagAction >( m_currentData.m_riffTag, ::events::RiffTagAction::Action::Remove );
                    }
                    else
                    {
                        m_currentData.m_riffTag.m_favour = 0;
                        m_eventBusClient.Send< ::events::RiffTagAction >( m_currentData.m_riffTag, ::events::RiffTagAction::Action::Upsert );
                    }
                }
            }
            ImGui::CompactTooltip( bIsTaggedAtF0 ? "Un-tag this riff" : "Tag this riff with default rating" );
            ImGui::SameLine();

            {
                ImGui::Scoped::ColourButton tb( colour::shades::tag_lvl_2, bIsTaggedAtF1 );
                if ( ImGui::Button( ICON_FA_ANGLES_UP, ChunkyIconButtonSize ) ||
                    ImGui::Shortcut( ImGuiModFlags_Ctrl, ImGuiKey_2, false ) )
                {
                    if ( bIsTaggedAtF1 )
                    {
                        m_eventBusClient.Send< ::events::RiffTagAction >( m_currentData.m_riffTag, ::events::RiffTagAction::Action::Remove );
                    }
                    else
                    {
                        m_currentData.m_riffTag.m_favour = 1;
                        m_eventBusClient.Send< ::events::RiffTagAction >( m_currentData.m_riffTag, ::events::RiffTagAction::Action::Upsert );
                    }
                }
            }
            ImGui::CompactTooltip( bIsTaggedAtF1 ? "Un-tag this riff" : "Tag this riff with higher rating" );
            ImGui::SameLine();
        }

        // use the tool provider to spin out the rest of the buttons; the apps can also therefore modify and extend that
        const uint8_t toolCount = toolProvider.getToolCount();
        for ( uint8_t toolIndex = 0; toolIndex < toolCount; ++toolIndex )
        {
            const auto toolID = static_cast< TagLineToolProvider::ToolID >( toolIndex );
            const bool toolEnabled = toolProvider.isToolEnabled( toolID, currentRiff );
            ImGui::Scoped::Enabled se( toolEnabled && currentRiffIsValid );

            std::string tooltipText;

            if ( ImGui::Button( toolProvider.getToolIcon( toolID, tooltipText ), ChunkyIconButtonSize ) ||
                 toolProvider.checkToolKeyboardShortcut( toolID ) )
            {
                toolProvider.handleToolExecution( toolID, m_eventBusClient, currentRiffPtr );
            }
            if ( !tooltipText.empty() )
                ImGui::CompactTooltip( tooltipText.c_str() );

            ImGui::SameLine();
        }

        // display the usual stream of riff information & a button to yoink the data into the clipboard if desired
        {
            // gap size to locate the copy-to-clipboard button on the far right side
            const float detailTextChildWidth = ImGui::GetContentRegionAvail().x - ChunkyIconButtonSize.x - (ImGui::GetFramePaddingX() * 2.0f);

            ImGui::SameLine( 0, 12.0f );
            if ( ImGui::BeginChild( "riff-ui-details", ImVec2( detailTextChildWidth, ChunkyIconButtonSize.y ) ) )
            {
                if ( currentRiffIsValid )
                {
                    // center pair vertical alignment for the detail text
                    const float alignVertical = (ChunkyIconButtonSize.y - (ImGui::GetTextLineHeight() * 2.0f)) * 0.25f;

                    // compute a time delta so we can format how long ago this riff was subbed
                    const auto riffTimeDelta = spacetime::calculateDeltaFromNow( currentRiff->m_stTimestamp );

                    ImGui::Dummy( ImVec2( 0, alignVertical ) );

                    ImGui::TextUnformatted( currentRiff->m_uiDetails );

                    if ( m_debugToggleDetails )
                    {
                        ImGui::TextUnformatted( currentRiff->m_uiDetailsDebug );
                    }
                    else
                    {
                        ImGui::Text( "%s | %s",
                            currentRiff->m_uiTimestamp.c_str(),
                            riffTimeDelta.asPastTenseString( 3 ).c_str() );
                    }

                    if ( ImGui::IsItemClicked( 0 ) )
                        m_debugToggleDetails = !m_debugToggleDetails;
                }
            }
            ImGui::EndChild();

            ImGui::SameLine( 0, 0 );

            {
                ImGui::Scoped::Enabled se( currentRiffIsValid );
                // hold ALT to enable debug data for the riff copy
                const bool useDebugView = (ImGui::GetMergedModFlags() & ImGuiModFlags_Alt);
                if ( ImGui::Button( useDebugView ? ICON_FA_CALENDAR_PLUS : ICON_FA_CALENDAR, ChunkyIconButtonSize ) )
                {
                    ImGui::SetClipboardText( useDebugView ?
                        currentRiff->generateMetadataReport().c_str() :
                        currentRiff->m_riffData.riff.couchID.c_str()
                    );
                }
                ImGui::CompactTooltip( useDebugView ? "Copy full metadata to clipboard" : "Copy Couch ID to clipboard" );
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
TagLine::TagLine( base::EventBusClient eventBus )
    : m_state( std::make_unique<State>( std::move( eventBus ) ) )
{
}

// ---------------------------------------------------------------------------------------------------------------------
TagLine::~TagLine()
{
}

// ---------------------------------------------------------------------------------------------------------------------
void TagLine::imgui(
    endlesss::live::RiffPtr& currentRiffPtr,
    const endlesss::toolkit::Warehouse* warehouseAccess,
    TagLineToolProvider* toolProvider )
{
    TagLineToolProvider* tools = toolProvider;
    if ( tools == nullptr )
        tools = m_state.get();

    m_state->imgui( currentRiffPtr, warehouseAccess, *tools );
}

// ---------------------------------------------------------------------------------------------------------------------
bool TagLineToolProvider::isToolEnabled( const ToolID id, const endlesss::live::Riff* currentRiff ) const
{
    // check on virtual riffs, we can't do stuff like share or find them
    if ( id == ToolID::NavigateTo || 
         id == ToolID::ShareToFeed )
    {
        if ( currentRiff != nullptr )
        {
            return endlesss::toolkit::Warehouse::isRiffIDVirtual( currentRiff->m_riffData.riff.couchID ) == false;
        }
    }

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
void TagLineToolProvider::handleToolExecution( const ToolID id, base::EventBusClient& eventBusClient, endlesss::live::RiffPtr& currentRiffPtr )
{
    auto localRiffPtr = currentRiffPtr;
    auto currentRiff  = localRiffPtr.get();

    switch ( id )
    {
        case RiffExport:
        {
            endlesss::toolkit::xp::RiffExportAdjustments defaultAdjustments;
            eventBusClient.Send< ::events::ExportRiff >( localRiffPtr, defaultAdjustments );
        }
        break;

        case NavigateTo:
        {
            // dispatch a request to navigate this this riff, if we can find it
            endlesss::types::RiffIdentity riffToNavigate( currentRiff->m_riffData.jam.couchID, currentRiff->m_riffData.riff.couchID );
            eventBusClient.Send< ::events::RequestNavigationToRiff >( riffToNavigate );
        }
        break;

        case ShareToFeed:
        {
            // send an event to trigger sharing to the feed, let the app handle how
            endlesss::types::RiffIdentity riffToNavigate( currentRiff->m_riffData.jam.couchID, currentRiff->m_riffData.riff.couchID );
            eventBusClient.Send< ::events::RequestToShareRiff >( riffToNavigate );
        }
        break;

        default:
            ABSL_ASSERT( 0 );
            break;
    }
}

} // namespace ux
