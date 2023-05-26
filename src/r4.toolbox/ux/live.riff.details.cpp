//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"

#include "app/core.h"
#include "app/imgui.ext.h"
#include "app/module.frontend.fonts.h"

#include "endlesss/toolkit.riff.export.h"


namespace ImGui {
namespace ux {

// ---------------------------------------------------------------------------------------------------------------------
void RiffDetails(
    endlesss::live::RiffPtr& riffPtr,
    base::EventBusClient& eventBusClient,
    const endlesss::toolkit::xp::RiffExportAdjustments* adjustments = nullptr )
{
    const auto* currentRiff = riffPtr.get();
    if ( currentRiff )
    {
        // provide default riff export adjustments if none were passed in
        endlesss::toolkit::xp::RiffExportAdjustments defaultAdjustments;
        if ( adjustments != nullptr )
            defaultAdjustments = *adjustments;

        // compute a time delta so we can format how long ago this riff was subbed
        const auto riffTimeDelta = spacetime::calculateDeltaFromNow( currentRiff->m_stTimestamp );

        // print the state & changes data
        if ( ImGui::IconButton( ICON_FA_FLOPPY_DISK ) || 
             ImGui::Shortcut( ImGuiModFlags_Ctrl, ImGuiKey_E, false ) )
        {
            eventBusClient.Send< ::events::ExportRiff >( riffPtr, defaultAdjustments );
        }
        ImGui::CompactTooltip( "Export this riff to disk" );
        ImGui::SameLine( 0, 10.0f );
        ImGui::TextUnformatted( currentRiff->m_uiDetails );

        ImGui::Spacing();

        // hold ALT to enable debug data for the riff copy
        const bool useDebugView = ( ImGui::GetMergedModFlags() & ImGuiModFlags_Alt );
        if ( ImGui::IconButton( useDebugView ? ICON_FA_CALENDAR_PLUS : ICON_FA_CALENDAR ) )
        {
            ImGui::SetClipboardText( useDebugView ?
                currentRiff->generateMetadataReport().c_str() :
                currentRiff->m_riffData.riff.couchID.c_str()
            );
        }
        ImGui::CompactTooltip( useDebugView ? "Copy full metadata to clipboard" : "Copy Couch ID to clipboard" );
        ImGui::SameLine( 0, 10.0f );
        ImGui::Text( "%s | %s", currentRiff->m_uiTimestamp.c_str(), riffTimeDelta.asPastTenseString(3).c_str() );
    }
    else
    {
        // matching-sized empty space in case that's important
        ImGui::IconButton( ICON_FA_FLOPPY_DISK, false );
        ImGui::Spacing();
        ImGui::IconButton( ICON_FA_CALENDAR, false );
    }
}

} // namespace ux
} // namespace ImGui

