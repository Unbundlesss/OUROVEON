//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
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
struct TagLine::State
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

    void imgui( const endlesss::toolkit::Warehouse& warehouse, endlesss::live::RiffPtr& currentRiffPtr );


    base::EventBusClient            m_eventBusClient;

    base::EventListenerID           m_eventLID_RiffTagAction = base::EventListenerID::invalid();

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
void TagLine::State::imgui( const endlesss::toolkit::Warehouse& warehouse, endlesss::live::RiffPtr& currentRiffPtr )
{
    // take temporary copy of the shared pointer, in case it gets modified mid-tick by the mixer
    endlesss::live::RiffPtr localRiffPtr = currentRiffPtr;

    const auto currentRiff  = localRiffPtr.get();
    bool currentRiffIsValid = ( currentRiff &&
                                currentRiff->m_syncState == endlesss::live::Riff::SyncState::Success );

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
            m_currentData.m_riffIsTagged = warehouse.isRiffTagged( m_currentData.m_riffID, &m_currentData.m_riffTag );

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
        static const ImVec2 ChunkyIconButtonSize        = ImVec2( 48.0f, 48.0f );

        ImGui::Scoped::Enabled se( currentRiffIsValid );

        const bool bIsTaggedAtF0 = currentRiffIsValid && m_currentData.m_riffIsTagged && ( m_currentData.m_riffTag.m_favour == 0 );
        const bool bIsTaggedAtF1 = currentRiffIsValid && m_currentData.m_riffIsTagged && ( m_currentData.m_riffTag.m_favour == 1 );

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

        if ( ImGui::Button( ICON_FA_FLOPPY_DISK, ChunkyIconButtonSize ) ||
             ImGui::Shortcut( ImGuiModFlags_Ctrl, ImGuiKey_E, false ) )
        {
            endlesss::toolkit::xp::RiffExportAdjustments defaultAdjustments;

            m_eventBusClient.Send< ::events::ExportRiff >( localRiffPtr, defaultAdjustments );
        }
        ImGui::CompactTooltip( "Export this riff to disk" );
        ImGui::SameLine();

        // navigate to this riff if we can
        if ( ImGui::Button( ICON_FA_GRIP, ChunkyIconButtonSize ) )
        {
            // dispatch a request to navigate this this riff, if we can find it
            endlesss::types::RiffIdentity riffToNavigate( currentRiff->m_riffData.jam.couchID, currentRiff->m_riffData.riff.couchID );
            m_eventBusClient.Send< ::events::RequestNavigationToRiff >( riffToNavigate );
        }
        ImGui::CompactTooltip( "Navigate to this riff" );
        ImGui::SameLine();

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

        // display the usual stream of riff information
        if ( currentRiffIsValid )
        {
            const float alignVertical = ( ChunkyIconButtonSize.y - ( ImGui::GetTextLineHeight() * 2.0f ) ) * 0.25f;

            ImGui::SameLine(0, 16.0f);
            if ( ImGui::BeginChild( "riff-ui-details", ImVec2(0.0f, ChunkyIconButtonSize.y ) ) )
            {
                // compute a time delta so we can format how long ago this riff was subbed
                const auto riffTimeDelta = spacetime::calculateDeltaFromNow( currentRiff->m_stTimestamp );

                ImGui::Dummy( ImVec2( 0, alignVertical ) );
                ImGui::TextUnformatted( currentRiff->m_uiDetails );
                ImGui::Text( "v.%u | %s | %s",
                    currentRiff->m_riffData.riff.appVersion,
                    currentRiff->m_uiTimestamp.c_str(),
                    riffTimeDelta.asPastTenseString( 3 ).c_str() );
            }
            ImGui::EndChild();
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
void TagLine::imgui( const endlesss::toolkit::Warehouse& warehouse, endlesss::live::RiffPtr& currentRiffPtr )
{
    m_state->imgui( warehouse, currentRiffPtr );
}

} // namespace ux
