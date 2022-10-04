//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//

#include "pch.h"

#include "vx/stembeats.h"

#include "endlesss/toolkit.exchange.h"

namespace ImGui {
namespace vx {

void StemBeats( const char* label, const endlesss::toolkit::Exchange& data, const float beatIndicatorSize, const bool monochrome )
{
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();

    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImGui::InvisibleButton( label, ImVec2( width, beatIndicatorSize  ) );

    const ImU32 colBG = ImGui::GetColorU32( ImVec4( 1.0f, 1.0f, 1.0f, 0.1f + data.m_consensusBeat ) );
    const ImU32 colFG = ImGui::GetColorU32( ImGuiCol_Text );

    const float indicatorSizeHalf   = beatIndicatorSize * 0.5f;
    const float horizontalInset     = beatIndicatorSize * 2.0f;

    // edge size is just based on what looked ok for the default 16x16 block
    const float edgeStrokeSize      = ( beatIndicatorSize / 16.0f ) * 3.0f;
    const float edgeStrokeInset     = beatIndicatorSize - edgeStrokeSize;

    const float outerBeatCircleSize = edgeStrokeInset - edgeStrokeSize;
    const float innerBeatCircleSize = outerBeatCircleSize - 1.0f;

    const float stemBlockWidth = ( width - ( horizontalInset * 2.0f ) ) / 7.0f;
    for ( size_t stemI = 0; stemI < 8; stemI++ )
    {
        const float activeStart = horizontalInset + (stemI * stemBlockWidth);

        const auto stemCenter = ImVec2( pos.x + activeStart, pos.y + indicatorSizeHalf );

        const auto stemEnergy = std::clamp( data.m_stemEnergy[stemI], 0.0f, 1.0f );
        const auto stemColour = data.m_stemColour[stemI];

        draw_list->AddCircleFilled( stemCenter, outerBeatCircleSize, colBG, 20 );
        draw_list->AddCircleFilled( stemCenter, stemEnergy * innerBeatCircleSize, colFG, 20 );

        const float energy = data.m_stemPulse[stemI] * 1.45f;

        draw_list->PathArcTo( stemCenter, edgeStrokeInset, -energy, energy, 16 );
        draw_list->PathStroke( monochrome ? colFG : stemColour, false, edgeStrokeSize );

        draw_list->PathArcTo( stemCenter, edgeStrokeInset, 3.14f + -energy, 3.14f + energy, 16 );
        draw_list->PathStroke( monochrome ? colFG : stemColour, false, edgeStrokeSize );
    }
}

} // namespace vx
} // name