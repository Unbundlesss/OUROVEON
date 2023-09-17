//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//

#include "pch.h"
#include "base/mathematics.h"

#include "ux/stem.beats.h"

#include "endlesss/toolkit.exchange.h"

namespace ux {

void StemBeats( const char* label, const endlesss::toolkit::Exchange& data, const float beatIndicatorSize, const bool monochrome )
{
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();

    const ImVec2 panelSize = ImGui::GetContentRegionAvail();
    const ImVec2 pos = ImGui::GetCursorScreenPos() + ImVec2( 0, ( panelSize.y * 0.5f ) - ( beatIndicatorSize * 0.5f ) );

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImGui::InvisibleButton( label, panelSize );

    const ImU32 colFG = ImGui::GetColorU32( ImGuiCol_Text );
    const ImU32 colBG = ImGui::GetColorU32( ImVec4( 1.0f, 1.0f, 1.0f, 0.1f ) );
    const ImU32 colCN = ImGui::GetColorU32( ImVec4( 1.0f, 1.0f, 1.0f, 0.025f + data.m_consensusBeat ) );

    std::array<ImVec2, 8> stemCenters;

    const float indicatorSizeHalf   = beatIndicatorSize * 0.5f;
    const float horizontalInset     = beatIndicatorSize * 1.5f;

    // edge size is just based on what looked ok for the default 16x16 block
    const float edgeStrokeSize      = ( beatIndicatorSize / 16.0f ) * 3.0f;
    const float edgeStrokeInset     = beatIndicatorSize - edgeStrokeSize;

    const float outerBeatCircleSize = edgeStrokeInset - edgeStrokeSize;
    const float innerBeatCircleSize = outerBeatCircleSize - 1.0f;

    const float analysisWaitSpinAngle = static_cast<float>( base::fract( ImGui::GetTime() * 0.5 ) );

    const float stemBlockWidth = ( panelSize.x - ( horizontalInset * 2.0f ) ) / 7.0f;
    for ( size_t stemI = 0; stemI < 8; stemI++ )
    {
        const float activeStart = horizontalInset + (stemI * stemBlockWidth);

        const auto stemCenter = ImVec2( pos.x + activeStart, pos.y + indicatorSizeHalf );
        stemCenters[stemI] = stemCenter;

        const auto stemColour   = data.m_stemColour[stemI];

        // inner circle is driven by lower-frequency extraction as a kind of 'energy' meter
        const auto stemPulse = data.m_stemWaveLF[stemI];
        draw_list->AddCircleFilled( stemCenter, outerBeatCircleSize, colBG, 20 );
        draw_list->AddCircleFilled( stemCenter, stemPulse * innerBeatCircleSize, colFG, 20 );

        // illuminate an outer ring with the higher-frequency output, multiplied up as it's often lower
        // magnitude than the rest
        const ImU32 colRing = ImGui::GetColorU32( ImVec4( 1.0f, 1.0f, 1.0f, std::min( data.m_stemWaveHF[stemI] * 1.25f, 1.0f ) ) );

        draw_list->AddCircle( stemCenter, outerBeatCircleSize - 1.0f, colRing, 0, 2.0f );

        if ( data.m_stemGain[stemI] > 0 )
        {
            // handle stems that haven't been analysed yet
            if ( !data.m_stemAnalysed[stemI] )
            {
                const float spinAngle = analysisWaitSpinAngle * constants::f_2pi;
                const float spinDistance = constants::f_quarter_pi * 0.5f;

                draw_list->PathArcTo( stemCenter, edgeStrokeInset, spinAngle - spinDistance, spinAngle + spinDistance, 16 );
                draw_list->PathStroke( colFG, false, edgeStrokeSize * 0.5f );
            }
            // draw beat pulse edges
            else
            {
                const float beatReact = data.m_stemBeat[stemI] * constants::f_half_pi;

                draw_list->PathArcTo( stemCenter, edgeStrokeInset, -beatReact, beatReact, 16 );
                draw_list->PathStroke( monochrome ? colFG : stemColour, false, edgeStrokeSize );

                draw_list->PathArcTo( stemCenter, edgeStrokeInset, constants::f_pi + -beatReact, constants::f_pi + beatReact, 16 );
                draw_list->PathStroke( monochrome ? colFG : stemColour, false, edgeStrokeSize );
            }
        }
    }

    for ( size_t linkI = 0; linkI < 7; linkI++ )
    {
        ImVec2 linkStart = stemCenters[linkI];
        ImVec2 linkEnd   = stemCenters[linkI + 1];

        // get size between the beat indicators
        const float linkGap      = linkEnd.x - linkStart.x;
        const float linkGapInset = beatIndicatorSize * 2.75f;

        // do we have space to draw the "beat link" bits?
        if ( linkGap >= linkGapInset )
        {
            // figure out how big the actual line needs to be
            const float linkageSize = std::ceil( linkGap - linkGapInset );

            // center it
            linkStart.x += linkGap * 0.5f;
            linkEnd.x = linkStart.x;

            linkStart.x -= (linkageSize * 0.5f);
            linkEnd.x += (linkageSize * 0.5f);
            linkEnd.x -= 0.75f;

            draw_list->AddLine( linkStart, linkEnd, colCN, beatIndicatorSize * 0.25f );
        }
    }
}

} // namespace ux
