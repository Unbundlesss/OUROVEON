//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  imgui extras
//

#pragma once
#include "base/utils.h"


// ---------------------------------------------------------------------------------------------------------------------
inline ImVec4 lerpVec4( const ImVec4& from, const ImVec4& to, const float t )
{
    return ImVec4(
        base::lerp( from.x, to.x, t ),
        base::lerp( from.y, to.y, t ),
        base::lerp( from.z, to.z, t ),
        base::lerp( from.w, to.w, t )
    );
}

// ---------------------------------------------------------------------------------------------------------------------
namespace ImGui {

template < typename _HandleType >
struct _ImTexture
{
    ImVec2          m_size;
    int32_t         m_width;
    int32_t         m_height;
    _HandleType     m_handle;
};

using ImTexture = _ImTexture<uint32_t>;
bool ImTextureFromFile( const char* filename, ImTexture& texture, bool clampToEdges );


inline void SeparatorBreak()
{
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
}
inline void ColumnSeparatorBreak()
{
    ImGui::Spacing();
    ImGui::ColumnSeparator();
    ImGui::Spacing();
}

void BeginDisabledControls( bool cond );
void EndDisabledControls( bool cond );

// ---------------------------------------------------------------------------------------------------------------------
ImU32 ParseHexColour( const char* hexColour );

// ---------------------------------------------------------------------------------------------------------------------
ImVec4 GetPulseColourVec4();
ImU32 GetPulseColour();

inline ImVec4 GetErrorTextColour() { return ImVec4(0.981f, 0.074f, 0.178f, 0.985f); }

// ---------------------------------------------------------------------------------------------------------------------
void CompactTooltip( const char* tip );

// ---------------------------------------------------------------------------------------------------------------------
bool KnobFloat( const char* label, const float outerRadius, float* p_value, float v_min, float v_max, float v_step );

// ---------------------------------------------------------------------------------------------------------------------
bool Spinner( const char* label, bool active, float radius, float thickness, const ImU32& color );

// ---------------------------------------------------------------------------------------------------------------------
void BeatSegments(
    const char* label,
    const int numSegments,
    const int activeSegment,
    const float height = 5.0f,
    const ImU32 highlightColour = ImGui::GetColorU32( ImGuiCol_TextDisabled ),
    const ImU32 backgroundColour = ImGui::GetColorU32( ImGuiCol_FrameBg ) );

// ---------------------------------------------------------------------------------------------------------------------
void RenderArrowSolid( ImDrawList* draw_list, ImVec2 pos, ImU32 col, ImGuiDir dir, float height );

// ---------------------------------------------------------------------------------------------------------------------
bool ClickableText( const char* label );


template< typename _T, int32_t _Size >
struct RingBufferedGraph
{
    _T      data[_Size];
    int32_t writeIndex = 0;

    RingBufferedGraph( const _T defaults )
    {
        for ( auto i = 0; i < _Size; i++ )
            data[i] = defaults;
    }

    void append( const _T v )
    {
        data[writeIndex] = v;
        writeIndex++;
        if ( writeIndex > _Size )
            writeIndex = 0;
    }

    void imgui( const char* label )
    {
        ImPlot::PlotBars( label, data, _Size, 0.6, 0, writeIndex );
    }
};

namespace Scoped {

void TickPulses();

// ---------------------------------------------------------------------------------------------------------------------
struct ButtonTextAlignLeft
{
    ButtonTextAlignLeft();
    ~ButtonTextAlignLeft();
private:
    ImVec2  m_state;
};

// ---------------------------------------------------------------------------------------------------------------------
struct ToggleButton
{
    ToggleButton( bool state, bool pulse = false );
    ~ToggleButton();

private:
    bool    m_state;
};

// ---------------------------------------------------------------------------------------------------------------------
struct ToggleTab
{
    ToggleTab( bool state, bool pulse = false );
    ~ToggleTab();

private:
    bool    m_state;
};

} // namespace Scoped

} // namespace ImGui