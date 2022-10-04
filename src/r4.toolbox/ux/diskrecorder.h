//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//

#pragma once

#include "spacetime/chronicle.h"
#include "base/text.h"

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
   

    const auto recordableUID = ImGui::GetID( recordable.getRecorderName() );
    ImGui::PushID( recordableUID );
    const auto animCycleOffset = recordableUID & 0xff;

    const bool recodingInProgress = recordable.isRecording();

    // recorder may be in a state of flux; if so, display what it's telling us
    if ( recodingInProgress &&
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
        if ( !recodingInProgress )
        {
            bool beginRecording = false;

            beginRecording |= ImGui::Button( ICON_FA_HARD_DRIVE, commonButtonSize );

            ImGui::Scoped::ButtonTextAlignLeft leftAlign;

            ImGui::SameLine( 0.0f, 4.0f );
            beginRecording |= ImGui::Button( recordable.getRecorderName(), ImVec2( ImGui::GetContentRegionAvail().x, 0.0f ) );

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
