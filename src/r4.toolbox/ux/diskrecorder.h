//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//

#pragma once

#include "spacetime/chronicle.h"
#include "base/text.h"

#include "xp/open.url.h"

namespace ux {
namespace widget {

// ---------------------------------------------------------------------------------------------------------------------
void DiskRecorder( rec::IRecordable& recordable, const fs::path& recordingRootPath )
{
    const ImVec2 commonButtonSize = ImVec2( 26.0f, ImGui::GetFrameHeight() );

    static constexpr std::array< const char*, 10 > animProgress{
        "[`-.___] ",
        "[-.___,] ",
        "[.___,-] ",
        "[___,-'] ",
        "[__,-'\"] ",
        "[_,-'\"`] ",
        "[,-'\"`-] ",
        "[-'\"`-.] ",
        "['\"`-._] ",
        "[\"`-.__] ",
    };

    // modifier to jump open the output directory in Explorer etc
    const bool bOpenOutputDirectoryOnClick = ( ImGui::GetMergedModFlags() & ImGuiModFlags_Alt );
   
    const auto recordableName   = recordable.getRecorderName();
    const auto recordableUID    = ImGui::GetID( recordableName.data() );
    const auto animCycleOffset  = recordableUID & 0xff;

    const bool bRecodingInProgress = recordable.isRecording();

    ImGui::PushID( recordableUID );

    {
        const auto hostContainerPosSS = ImGui::GetWindowPos();
        const auto hostContainerSize  = ImGui::GetWindowSize();

        // when the mouse is over the host container and the modifier is down,
        // early out with a button that takes the user to where the recordings are stored
        if ( ImGui::IsMouseHoveringRect( hostContainerPosSS, hostContainerPosSS + hostContainerSize ) &&
             bOpenOutputDirectoryOnClick )
        {
            const auto pathToOpen = recordingRootPath.string();

            if ( ImGui::Button( ICON_FA_UP_RIGHT_FROM_SQUARE, commonButtonSize ) )
            {
                xpOpenURL( pathToOpen.data() );
            }

            ImGui::SameLine( 0.0f, 4.0f );
            ImGui::PushItemWidth( ImGui::GetContentRegionAvail().x );
            ImGui::TextUnformatted( " Open Output Directory ..." );
            ImGui::CompactTooltip( pathToOpen );
            ImGui::PopItemWidth();

            ImGui::PopID();
            return;
        }
    }

    // recorder may be in a state of flux; if so, display what it's telling us
    if ( bRecodingInProgress &&
         recordable.getFluxState() != nullptr )
    {
        {
            ImGui::Scoped::ToggleButton toggled( true, true );
            if ( ImGui::Button( ICON_FA_HOURGLASS, commonButtonSize ) )
            {
                recordable.stopRecording();
            }
        }

        ImGui::SameLine( 0.0f, 4.0f );
        ImGui::PushItemWidth( ImGui::GetContentRegionAvail().x );
        ImGui::TextUnformatted( recordable.getFluxState() );
        ImGui::PopItemWidth();
    }
    // otherwise it will be recording or not, so handle those default states
    else
    {
        if ( !bRecodingInProgress )
        {
            bool beginRecording = false;

            beginRecording |= ImGui::Button( ICON_FA_HARD_DRIVE, commonButtonSize );

            ImGui::Scoped::ButtonTextAlignLeft leftAlign;

            ImGui::SameLine( 0.0f, 4.0f );
            beginRecording |= ImGui::Button( recordableName.data(), ImVec2(ImGui::GetContentRegionAvail().x, 0.0f));


            if ( beginRecording )
            {
                const auto timestampPrefix = spacetime::createPrefixTimestampForFile();
                recordable.beginRecording( recordingRootPath, timestampPrefix );
            }
        }
        else
        {
            int32_t animCycle = ImGui::GetFrameCount() + animCycleOffset;

            const uint64_t bytesRecorded = recordable.getRecordingDataUsage();
            const auto humanisedBytes = base::humaniseByteSize( animProgress[ (animCycle / 4) % 10 ], bytesRecorded );

            {
                ImGui::Scoped::ToggleButton toggled( true, true );
                if ( ImGui::Button( ICON_FA_HARD_DRIVE, commonButtonSize ) )
                {
                    recordable.stopRecording();
                }
            }
            ImGui::SameLine( 0.0f, 4.0f );
            ImGui::PushItemWidth( ImGui::GetContentRegionAvail().x );
            ImGui::TextUnformatted( humanisedBytes.c_str() );
            ImGui::PopItemWidth();
        }
    }

    ImGui::PopID();
}

} // namespace widget
} // namespace ux
