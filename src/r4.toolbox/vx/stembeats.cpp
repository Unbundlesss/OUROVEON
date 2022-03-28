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

void StemBeats( const char* label, const endlesss::Exchange& data )
{
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();

    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    const float height = 32.0f;

    ImGui::InvisibleButton( label, ImVec2( width, height ) );

    const ImU32 colBG = ImGui::GetColorU32( ImVec4( 1.0f, 1.0f, 1.0f, 0.1f + data.m_consensusBeat ) );
    const ImU32 colFG = ImGui::GetColorU32( ImGuiCol_Text );

    float stemBlockWidth = (width - 32.0f) / 7.0f;
    for ( size_t stemI = 0; stemI < 8; stemI++ )
    {
        const float activeStart = 16.0f + (stemI * stemBlockWidth);

        const auto stemCenter = ImVec2( pos.x + activeStart, pos.y + 8.0f );

        const auto stemEnergy = std::clamp( data.m_stemEnergy[stemI], 0.0f, 1.0f );

        draw_list->AddCircleFilled( stemCenter, 10.0f, colBG, 20 );
        draw_list->AddCircleFilled( stemCenter, stemEnergy * 9.0f, colFG, 20 );

        const float energy = data.m_stemPulse[stemI] * 1.45f;

        draw_list->PathArcTo( stemCenter, 13.0f, -energy, energy, 16 );
        draw_list->PathStroke( colFG, false, 3.0f );

        draw_list->PathArcTo( stemCenter, 13.0f, 3.14f + -energy, 3.14f + energy, 16 );
        draw_list->PathStroke( colFG, false, 3.0f );
    }
}

} // namespace vx
} // name