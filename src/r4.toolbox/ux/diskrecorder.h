//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//

#include "spacetime/chronicle.h"

namespace ux {
namespace widget {

// ---------------------------------------------------------------------------------------------------------------------
void DiskRecorder( rec::IRecordable& recordable, const fs::path& recordingRootPath )
{
    const ImVec2 commonButtonSize = ImVec2( 26.0f, ImGui::GetFrameHeight() );

    static constexpr std::array< const char*, 5 > animProgress{
        "[_.~\"~] ",
        "[~_.~\"] ",
        "[\"~_.~] ",
        "[~\"~_.] ",
        "[.~\"~_] "
    };

    ImGui::PushID( recordable.getRecorderName() );

    const bool recodingInProgress = recordable.isRecording();

    // recorder may be in a state of flux; if so, display what it's telling us
    if ( recodingInProgress &&
         recordable.getFluxState() != nullptr )
    {
        {
            ImGui::Scoped::ToggleButton toggled( true, true );
            if ( ImGui::Button( ICON_FA_HOURGLASS_HALF, commonButtonSize ) )
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

            beginRecording |= ImGui::Button( ICON_FA_HDD, commonButtonSize );

            ImGui::SameLine( 0.0f, 4.0f );
            beginRecording |= ImGui::Button( recordable.getRecorderName(), ImVec2( ImGui::GetContentRegionAvail().x, 0.0f ) );

            if ( beginRecording )
            {
                const auto timestampPrefix = spacetime::createPrefixTimestampForFile();
                recordable.beginRecording( recordingRootPath.string(), timestampPrefix );
            }
        }
        else
        {
            // #HDD grim!
            static int32_t animCycle = 0;
            animCycle++;

            const uint64_t bytesRecorded = recordable.getRecordingDataUsage();
            const auto humanisedBytes = base::humaniseByteSize( animProgress[(animCycle / 8) % 5], bytesRecorded );

            {
                ImGui::Scoped::ToggleButton toggled( true, true );
                if ( ImGui::Button( ICON_FA_HDD, commonButtonSize ) )
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
