//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  imgui extras
//

#include "pch.h"
#include "app/imgui.ext.h"

static float g_cycleTimer;

namespace ImGui {

// ---------------------------------------------------------------------------------------------------------------------
// https://github.com/ocornut/imgui/wiki/Image-Loading-and-Displaying-Examples
//
bool ImTextureFromFile( const char* filename, ImTexture& texture, bool clampToEdges )
{
    // Load from file
    int image_width = 0;
    int image_height = 0;
    unsigned char* image_data = stbi_load( filename, &image_width, &image_height, NULL, 4 );
    if ( image_data == NULL )
        return false;

    GLuint image_texture;
    glGenTextures( 1, &image_texture );
    glBindTexture( GL_TEXTURE_2D, image_texture );

    auto wrapParam = clampToEdges ? GL_CLAMP_TO_EDGE : GL_REPEAT;

    // Setup filtering parameters for display
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapParam );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapParam );

    // Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
    glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
#endif
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data );
    stbi_image_free( image_data );

    texture.m_handle = image_texture;
    texture.m_width  = image_width;
    texture.m_height = image_height;
    texture.m_size   = ImVec2( (float)image_width, (float)image_height );

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
void BeginDisabledControls( bool cond )
{
    if ( cond ) 
    {
        ImGui::PushItemFlag( ImGuiItemFlags_Disabled, true );
        ImGui::PushStyleVar( ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.25f );
    }
}

// ---------------------------------------------------------------------------------------------------------------------
void EndDisabledControls( bool cond )
{
    if ( cond ) 
    {
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
    }
}

// ---------------------------------------------------------------------------------------------------------------------
ImU32 ParseHexColour( const char* hexColour )
{
    int i[4];
    float f[4];
    i[0] = i[1] = i[2] = i[3] = 0;
    sscanf( hexColour, "%02X%02X%02X%02X", (unsigned int*)&i[0], (unsigned int*)&i[1], (unsigned int*)&i[2], (unsigned int*)&i[3] ); // Treat at unsigned (%X is unsigned)
    for ( int n = 0; n < 4; n++ )
        f[n] = i[n] / 255.0f;
    return ImGui::ColorConvertFloat4ToU32( ImVec4( f[1], f[2], f[3], f[0] ) );
}

// ---------------------------------------------------------------------------------------------------------------------
ImVec4 GetPulseColourVec4()
{
    const auto colour1 = ImGui::GetStyleColorVec4( ImGuiCol_PlotHistogram );
    const auto colour2 = ImGui::GetStyleColorVec4( ImGuiCol_PlotHistogramHovered );
    return lerpVec4( colour1, colour2, g_cycleTimer );
}

ImU32 GetPulseColour()
{
    return ImGui::GetColorU32( GetPulseColourVec4() );
}

// ---------------------------------------------------------------------------------------------------------------------
void CompactTooltip( const char* tip )
{
    if ( ImGui::IsItemHovered() )
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos( ImGui::GetFontSize() * 35.0f );
        ImGui::TextUnformatted( tip );
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

// ---------------------------------------------------------------------------------------------------------------------
void RenderArrowSolid( ImDrawList* draw_list, ImVec2 pos, ImU32 col, ImGuiDir dir, float height )
{
    const float h = height;
    float r = h * 0.40f;
    ImVec2 center = pos + ImVec2( h * 0.50f, 0 );

    ImVec2 a, b, c;
    switch ( dir )
    {
    case ImGuiDir_Up:
    case ImGuiDir_Down:
        if ( dir == ImGuiDir_Up ) r = -r;
        a = ImVec2( +0.000f, +0.750f ) * r;
        b = ImVec2( -0.866f, -0.750f ) * r;
        c = ImVec2( +0.866f, -0.750f ) * r;
        break;

    case ImGuiDir_Left:
    case ImGuiDir_Right:
        if ( dir == ImGuiDir_Left ) r = -r;
        a = ImVec2( +0.750f, +0.000f ) * r;
        b = ImVec2( -0.750f, +0.866f ) * r;
        c = ImVec2( -0.750f, -0.866f ) * r;
        break;

    case ImGuiDir_None:
    case ImGuiDir_COUNT:
        IM_ASSERT( 0 );
        break;
    }
    draw_list->AddTriangleFilled( center + a, center + b, center + c, col );
}

// ---------------------------------------------------------------------------------------------------------------------
bool ClickableText( const char* label )
{
    ImGuiContext& g = *GImGui;
    float backup_padding_y = g.Style.FramePadding.y;
    g.Style.FramePadding.y = 0.0f;

    bool pressed = false;

    {
        ImGuiWindow* window = GetCurrentWindow();

        ImGuiContext& g         = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id        = window->GetID(label);
        const ImVec2 label_size = CalcTextSize(label, NULL, true);

        ImVec2 pos = window->DC.CursorPos;
        if (style.FramePadding.y < window->DC.CurrLineTextBaseOffset)
            pos.y += window->DC.CurrLineTextBaseOffset - style.FramePadding.y;
        ImVec2 size = CalcItemSize(ImVec2(0,0), label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

        const ImRect bb(pos, pos + size);
        ItemSize(size, style.FramePadding.y);
        if (!ItemAdd(bb, id))
        {
            g.Style.FramePadding.y = backup_padding_y;
            return false;
        }

        bool hovered, held;
        pressed = ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_AlignTextBaseLine );

        const ImU32 colFG = GetColorU32((held && hovered) ? ImGuiCol_SeparatorActive : hovered ? ImGuiCol_SeparatorHovered : ImGuiCol_Text );
        RenderNavHighlight(bb, id);

        ImGui::PushStyleColor( ImGuiCol_Text, colFG );
        RenderTextClipped(bb.Min + style.FramePadding, bb.Max - style.FramePadding, label, NULL, &label_size, style.ButtonTextAlign, &bb);
        ImGui::PopStyleColor();
    }

    g.Style.FramePadding.y = backup_padding_y;
    return pressed;
}


// ---------------------------------------------------------------------------------------------------------------------
bool KnobFloat( const char* label, const float outerRadius, float* p_value, float v_min, float v_max, float v_step )
{
    constexpr float cAngleMin       = constants::f_pi * 0.75f;
    constexpr float cAngleMax       = constants::f_pi * 2.25f;

    //@ocornut https://github.com/ocornut/imgui/issues/942
    ImGuiIO& io           = ImGui::GetIO();
    ImGuiStyle& style     = ImGui::GetStyle();

    ImVec2 pos            = ImGui::GetCursorScreenPos();
    ImVec2 center         = ImVec2( pos.x + outerRadius, pos.y + outerRadius + 2.0f );
    float line_height     = ImGui::GetTextLineHeight();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    const float adjustedRadius          = outerRadius - 4.0f;
    const bool hasLabel                 = label[0] != '\0' && label[0] != '#';
    const float labelDrawHeight         = hasLabel ? (style.ItemInnerSpacing.y + 5.0f) : 0.0f;
    const float extendHeightForLabel    = hasLabel ? (line_height + labelDrawHeight) : 0.0f;

    ImGui::InvisibleButton( label, ImVec2( outerRadius * 2, outerRadius * 2 + extendHeightForLabel ) );
    const bool is_active        = ImGui::IsItemActive();
    const bool is_hovered       = ImGui::IsItemHovered();
    const bool is_interactable  = v_step > 0;

    bool value_changed = false;
    if ( is_active && 
         is_interactable &&
         io.MouseDelta.y != 0.0f )
    {
        float step = (v_max - v_min) / v_step;
        *p_value += (-io.MouseDelta.y) * step;

        if ( *p_value < v_min ) 
            *p_value = v_min;
        if ( *p_value > v_max ) 
            *p_value = v_max;
        value_changed = true;
    }
#if 0
    if ( is_active )
    {
        const auto mouseToCenterX = io.MousePos.x - center.x;
        const auto mouseToCenterY = io.MousePos.y - center.y;

        const auto rot = std::atan2( mouseToCenterX, -mouseToCenterY );

        float interp = 0.0f;
        interp = std::clamp( 0.5f + ((rot / cAngleMin) * 0.5f), 0.0f, 1.0f );

        *p_value = v_min + ( (v_max - v_min) * interp );
        value_changed = true;
    }
#endif
    else if ( is_hovered && is_interactable && (io.MouseDoubleClicked[0] || io.MouseClicked[2]) )
    {
        *p_value = (v_max + v_min) * 0.5f;  // reset value
        value_changed = true;
    }

    const auto outerColor = ImGui::GetColorU32( ImGuiCol_FrameBg );
    const auto innerColor = ImGui::GetColorU32( is_active ? ImGuiCol_FrameBgActive : is_hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg );


    float t            = (*p_value - v_min) / (v_max - v_min);
    float angle        = cAngleMin + (cAngleMax - cAngleMin) * t;
    float angle_cos    = std::cos( angle ), angle_sin = std::sin( angle );
    float radius_inner = adjustedRadius * 0.35f;
    draw_list->AddCircleFilled( center, adjustedRadius - 2.0f, outerColor, 24 );
    draw_list->AddCircleFilled( center, radius_inner, innerColor, 24 );

    if ( hasLabel )
        draw_list->AddText( ImVec2( pos.x, pos.y + adjustedRadius * 2 + labelDrawHeight ), ImGui::GetColorU32( ImGuiCol_Text ), label );

    draw_list->PathArcTo( center, adjustedRadius + 2.0f, cAngleMin, cAngleMax, 32 );
    draw_list->PathStroke( innerColor, false, 4.0f );

    draw_list->PathArcTo( center, adjustedRadius + 2.0f, cAngleMin, angle, 32 );
    draw_list->PathLineTo( ImVec2( center.x + angle_cos * radius_inner, center.y + angle_sin * radius_inner ) );
    draw_list->PathStroke( ImGui::GetColorU32( is_interactable ? ImGuiCol_SliderGrabActive : ImGuiCol_PlotHistogram ), false, 2.0f );

    if ( is_active || is_hovered ) 
    {
        ImGui::SetNextWindowPos( ImVec2( pos.x - style.WindowPadding.x, pos.y - line_height - style.ItemInnerSpacing.y - style.WindowPadding.y ) );
        ImGui::BeginTooltip();
        ImGui::Text( "%.0f%%", t * 100.0f );
        ImGui::EndTooltip();
    }

    return value_changed;
}

// ---------------------------------------------------------------------------------------------------------------------
bool Spinner( const char* label, bool active, float radius, float thickness, const ImU32& color )
{
    ImGuiWindow* window = GetCurrentWindow();
    if ( window->SkipItems )
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID( label );

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size( (radius) * 2, (radius + style.FramePadding.y) * 2 );

    const ImRect bb( pos, ImVec2( pos.x + size.x, pos.y + size.y ) );
    ItemSize( bb, style.FramePadding.y );
    if ( !ItemAdd( bb, id ) )
        return false;

    if ( !active )
        return true;

    // Render
    window->DrawList->PathClear();

    int num_segments = 30;
    int start = (int)( std::abs( std::sin( g.Time * 1.8f ) * (float)(num_segments - 5) ) );

    const float a_min = IM_PI * 2.0f * ((float)start) / (float)num_segments;
    const float a_max = IM_PI * 2.0f * ((float)num_segments - 3) / (float)num_segments;

    const ImVec2 centre = ImVec2( 2.0f + pos.x + radius, pos.y + radius + style.FramePadding.y );

    for ( int i = 0; i < num_segments; i++ ) 
    {
        const double a = a_min + ((double)i / (double)num_segments) * (a_max - a_min);
        window->DrawList->PathLineTo( ImVec2( 
            centre.x + (float)std::cos( a + g.Time * 8.0 ) * radius,
            centre.y + (float)std::sin( a + g.Time * 8.0 ) * radius )
        );
    }

    window->DrawList->PathStroke( color, false, thickness );
    return true;
}


// ---------------------------------------------------------------------------------------------------------------------
void BeatSegments(
    const char* label,
    const int numSegments,
    const int activeSegment,
    const float height,
    const ImU32 highlightColour,
    const ImU32 backgroundColour )
{
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();

    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImGui::InvisibleButton( label, ImVec2( width, height ) );

    draw_list->AddRectFilled( pos, pos + ImVec2( width, height ), backgroundColour, 1.0f );

    if ( activeSegment >= 0 )
    {
        const float widthPerSeg = width / (float)numSegments;

        const float activeStart = activeSegment * widthPerSeg;
        const float activeEnd   = activeStart + widthPerSeg;

        draw_list->AddRectFilled( ImVec2( pos.x + activeStart, pos.y), ImVec2( pos.x + activeEnd, pos.y + height ), highlightColour, 2.0f );
    }
}


// ---------------------------------------------------------------------------------------------------------------------
namespace Scoped {

void TickPulses()
{
    g_cycleTimer = (float)(1.0 + std::sin( ImGui::GetTime() * 3.0 )) * 0.5f;
}

// ---------------------------------------------------------------------------------------------------------------------
ButtonTextAlignLeft::ButtonTextAlignLeft()
{
    ImGuiStyle& style = ImGui::GetStyle();
    m_state = style.ButtonTextAlign;
    style.ButtonTextAlign = ImVec2( 0.033f, 0.5f );
}

// ---------------------------------------------------------------------------------------------------------------------
ButtonTextAlignLeft::~ButtonTextAlignLeft()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.ButtonTextAlign = m_state;
}

// ---------------------------------------------------------------------------------------------------------------------
ToggleButton::ToggleButton( bool state, bool pulse /*= false */ ) 
    : m_state( state )
{
    if ( m_state )
    {
        if ( pulse )
        {
            const auto colourButton = GetPulseColour();

            ImGui::PushStyleColor( ImGuiCol_Button, colourButton );
            ImGui::PushStyleColor( ImGuiCol_ButtonHovered, colourButton );
        }
        else
        {
            ImGui::PushStyleColor( ImGuiCol_Button, ImGui::GetColorU32( ImGuiCol_NavHighlight ) );
            ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImGui::GetColorU32( ImGuiCol_ButtonActive ) );
        }

        ImGui::PushStyleColor( ImGuiCol_Text, ImGui::GetColorU32( ImGuiCol_WindowBg ) );
    }
}

// ---------------------------------------------------------------------------------------------------------------------
ToggleButton::~ToggleButton()
{
    if ( m_state )
        ImGui::PopStyleColor( 3 );
}

// ---------------------------------------------------------------------------------------------------------------------
ToggleTab::ToggleTab( bool state, bool pulse /*= false */ )
    : m_state( state )
{
    if ( m_state )
    {
        if ( pulse )
        {
            const auto colourButton = GetPulseColour();

            ImGui::PushStyleColor( ImGuiCol_Tab, colourButton );
            ImGui::PushStyleColor( ImGuiCol_TabHovered, colourButton );
            ImGui::PushStyleColor( ImGuiCol_TabActive, colourButton );
        }
        else
        {
            ImGui::PushStyleColor( ImGuiCol_Tab,        ImGui::GetColorU32( ImGuiCol_NavHighlight ) );
            ImGui::PushStyleColor( ImGuiCol_TabHovered, ImGui::GetColorU32( ImGuiCol_ButtonActive ) );
            ImGui::PushStyleColor( ImGuiCol_TabActive,  ImGui::GetColorU32( ImGuiCol_ButtonActive ) );
        }

        ImGui::PushStyleColor( ImGuiCol_Text, ImGui::GetColorU32( ImGuiCol_WindowBg ) );
    }
}

// ---------------------------------------------------------------------------------------------------------------------
ToggleTab::~ToggleTab()
{
    if ( m_state )
        ImGui::PopStyleColor( 4 );
}


} // namespace Scoped
} // namespace ImGui