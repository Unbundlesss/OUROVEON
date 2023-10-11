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
#include "app/module.frontend.fonts.h"

static float g_cycleTimerSlow;
static float g_cycleTimerFast;

namespace ImGui {

void StandardFilterBox( ImGuiTextFilter& hostFilter, const char* label, const float width )
{
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted( ICON_FA_MAGNIFYING_GLASS );
    ImGui::SameLine();
    hostFilter.Draw( label, width );
    ImGui::SameLine( 0, 2.0f );
    if ( ImGui::Button( ICON_FA_CIRCLE_XMARK ) )
        hostFilter.Clear();
}

bool BeginStatusBar()
{
    ImGuiContext& g = *GImGui;
    ImGuiViewportP* viewport = (ImGuiViewportP*)(void*)GetMainViewport();

    // Notify of viewport change so GetFrameHeight() can be accurate in case of DPI change
    SetCurrentViewport( NULL, viewport );

    // For the main menu bar, which cannot be moved, we honor g.Style.DisplaySafeAreaPadding to ensure text can be visible on a TV set.
    // FIXME: This could be generalized as an opt-in way to clamp window->DC.CursorStartPos to avoid SafeArea?
    // FIXME: Consider removing support for safe area down the line... it's messy. Nowadays consoles have support for TV calibration in OS settings.
    g.NextWindowData.MenuBarOffsetMinVal = ImVec2( g.Style.DisplaySafeAreaPadding.x, ImMax( g.Style.DisplaySafeAreaPadding.y - g.Style.FramePadding.y, 0.0f ) );
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;
    float height = GetFrameHeight();
    bool is_open = BeginViewportSideBar( "##OvStatusBar", viewport, ImGuiDir_Down, height, window_flags );
    g.NextWindowData.MenuBarOffsetMinVal = ImVec2( 0.0f, 0.0f );

    if ( is_open )
        BeginMenuBar();
    else
        End();
    return is_open;
}

void EndStatusBar()
{
    EndMenuBar();

    // When the user has left the menu layer (typically: closed menus through activation of an item), we restore focus to the previous window
    // FIXME: With this strategy we won't be able to restore a NULL focus.
    ImGuiContext& g = *GImGui;
    if ( g.CurrentWindow == g.NavWindow && g.NavLayer == ImGuiNavLayer_Main && !g.NavAnyRequest )
        FocusTopMostWindowUnderOne( g.NavWindow, NULL, NULL, ImGuiFocusRequestFlags_UnlessBelowModal | ImGuiFocusRequestFlags_RestoreFocusedChild );

    End();
}

bool IconButton( const char* label, const bool visible )
{
    ImGuiWindow* window = GetCurrentWindow();
    if ( window->SkipItems )
        return false;

    static const ImVec2 customPadding(  2.0f,  0.0f );
    static const ImVec2 customInset(    4.0f, -0.5f );
    static const ImVec2 customOffset(   3.0f,  3.0f );

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID( label );
    const ImVec2 label_size = CalcTextSize( label, NULL, true );

    ImVec2 pos = window->DC.CursorPos;
           pos.y += window->DC.CurrLineTextBaseOffset - customPadding.y;
    ImVec2 size = CalcItemSize( ImVec2( 0, 0 ), label_size.x + customPadding.x * 2.0f, label_size.y + customPadding.y * 2.0f );

    const ImRect bb( pos - customOffset, pos + size + customOffset );
    ItemSize( size, customPadding.y );
    if ( !ItemAdd( bb, id ) )
        return false;

    if ( !visible )
        return false;

    ImGuiButtonFlags flags = ImGuiButtonFlags_None;
    if ( g.LastItemData.InFlags & ImGuiItemFlags_ButtonRepeat )
        flags |= ImGuiButtonFlags_Repeat;
    bool hovered, held;
    bool pressed = ButtonBehavior( bb, id, &hovered, &held, flags );

    // Render
    const ImU32 col = GetColorU32( (held && hovered) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button );
    RenderNavHighlight( bb, id );
    RenderFrame( bb.Min, bb.Max, col, true, style.FrameRounding );
    RenderTextClipped( bb.Min + customInset, bb.Max - customPadding, label, NULL, &label_size, style.ButtonTextAlign, &bb );


    IMGUI_TEST_ENGINE_ITEM_INFO( id, label, g.LastItemData.StatusFlags );
    return pressed;
}


bool PrecisionButton( const char* label, const ImVec2& size, const float adjust_x, const float adjust_y )
{
    ImGuiWindow* window = GetCurrentWindow();
    if ( window->SkipItems )
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID( label );
    const ImVec2 label_size = CalcTextSize( label, NULL, true );

    ImVec2 pos = window->DC.CursorPos;

    const ImRect bb( pos, pos + size );
    ItemSize( size, 0 );
    if ( !ItemAdd( bb, id ) )
        return false;

    ImGuiButtonFlags flags = ImGuiButtonFlags_None;
    if ( g.LastItemData.InFlags & ImGuiItemFlags_ButtonRepeat )
        flags |= ImGuiButtonFlags_Repeat;
    bool hovered, held;
    bool pressed = ButtonBehavior( bb, id, &hovered, &held, flags );

    static const ImVec2 outerPad( 1.0f, 1.0f );
    const ImVec2 adjust( adjust_x, adjust_y );

    // Render
    const ImU32 col = GetColorU32( (held && hovered) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button );
    RenderNavHighlight( bb, id );
    RenderFrame( bb.Min + outerPad, bb.Max - outerPad, col, true, style.FrameRounding );
    RenderTextClipped( bb.Min + adjust, bb.Max + adjust, label, NULL, &label_size, style.ButtonTextAlign, &bb );


    IMGUI_TEST_ENGINE_ITEM_INFO( id, label, g.LastItemData.StatusFlags );
    return pressed;
}


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
bool BottomRightAlignedButton( const char* label, const ImVec2& size)
{
    const auto panelRegionAvail = ImGui::GetContentRegionAvail();
    {
        ImGui::Dummy( { 0, panelRegionAvail.y - size.y - 6.0f } );
    }
    ImGui::Dummy( { 0,0 } );
    ImGui::SameLine( 0, panelRegionAvail.x - size.x - 6.0f );
    return ImGui::Button( label, size );
}

// ---------------------------------------------------------------------------------------------------------------------
void BeginDisabledControls( const bool isDisabled )
{
    if ( isDisabled )
    {
        ImGui::PushItemFlag( ImGuiItemFlags_Disabled, true );
        ImGui::PushStyleVar( ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.25f );
    }
}

// ---------------------------------------------------------------------------------------------------------------------
void EndDisabledControls( const bool isDisabled )
{
    if ( isDisabled )
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

ImVec4 GetPulseColourVec4( float alpha )
{
    const auto colour1 = ImGui::GetStyleColorVec4( ImGuiCol_PlotHistogram );
    const auto colour2 = ImGui::GetStyleColorVec4( ImGuiCol_PlotHistogramHovered );
    auto pulse = lerpVec4( colour1, colour2, g_cycleTimerSlow );
    pulse.w = alpha;
    return pulse;
}

ImU32 GetPulseColour( float alpha )
{
    return ImGui::GetColorU32( GetPulseColourVec4( alpha ) );
}

ImU32 GetSyncBusyColour( float alpha )
{
    const auto colour1 = colour::shades::blue_gray.neutral( alpha );
    const auto colour2 = colour::shades::slate.dark( alpha );
    auto pulse = lerpVec4( colour1, colour2, g_cycleTimerSlow );

    return ImGui::GetColorU32( pulse );
}

// ---------------------------------------------------------------------------------------------------------------------
void CompactTooltip( const std::string_view tip )
{
    if ( ImGui::IsItemHovered( ImGuiHoveredFlags_DelayNormal ) )
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

void AddTextCentered( ImDrawList* DrawList, ImVec2 top_center, ImU32 col, const char* text_begin, const char* text_end ) {
    float txt_ht = ImGui::GetTextLineHeight();
    const char* title_end = ImGui::FindRenderedTextEnd( text_begin, text_end );
    ImVec2 text_size;
    float  y = 0;
    while ( const char* tmp = (const char*)memchr( text_begin, '\n', title_end - text_begin ) ) {
        text_size = ImGui::CalcTextSize( text_begin, tmp, true );
        DrawList->AddText( ImVec2( top_center.x - text_size.x * 0.5f, top_center.y + y ), col, text_begin, tmp );
        text_begin = tmp + 1;
        y += txt_ht;
    }
    text_size = ImGui::CalcTextSize( text_begin, title_end, true );
    DrawList->AddText( ImVec2( top_center.x - text_size.x * 0.5f, top_center.y + y ), col, text_begin, title_end );
}

// ---------------------------------------------------------------------------------------------------------------------
void CenteredText( const char* text )
{
    ImVec2 textSize = ImGui::CalcTextSize( text );
    const float currentRegionWidth = ImGui::GetContentRegionAvail().x;

    ImGui::Dummy( ImVec2( (currentRegionWidth * 0.5f ) - (textSize.x * 0.5f ), 0.0f ) );
    ImGui::SameLine();
    ImGui::TextUnformatted( text );
}

// ---------------------------------------------------------------------------------------------------------------------
void CenteredColouredText( const ImVec4& col, const char* text )
{
    ImGui::PushStyleColor( ImGuiCol_Text, col );
    ImGui::CenteredText( text );
    ImGui::PopStyleColor();
}

// ---------------------------------------------------------------------------------------------------------------------
void RightAlignSameLine( float objectSize )
{
    ImGui::Dummy( ImVec2( ImGui::GetContentRegionAvail().x - objectSize, 0.0f ) );
    ImGui::SameLine( 0, 0 );
}

// ---------------------------------------------------------------------------------------------------------------------
bool KnobFloat(
    const char* label,
    const float outerRadius,
    float* p_value,
    float v_min,
    float v_max,
    float v_notches,
    float default_value,
    const std::function< std::string ( const float percentage01, const float value ) >& tooltipCallback
    )
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
    const bool is_interactable  = v_notches > 0;

    bool value_changed = false;
    float input_step = 0;

    if ( is_hovered &&
         io.MouseWheel != 0 )
    {
        input_step = io.MouseWheel * 10.0f;
    }
    if ( is_active &&
         is_interactable &&
         io.MouseDelta.y != 0 )
    {
        input_step = -io.MouseDelta.y;
    }

    // shift to move quicker
    if ( io.KeyShift )
    {
        input_step *= 4.0f;
    }

    if ( input_step != 0 )
    {
        float step = (v_max - v_min) / v_notches;
        *p_value += input_step * step;

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
    
    // reset on double/right click
    if ( is_hovered && is_interactable && (io.MouseDoubleClicked[0] || io.MouseClicked[2]) )
    {
        *p_value = default_value;
        value_changed = true;
    }

    const auto outerColor = ImGui::GetColorU32( ImGuiCol_FrameBg );
    const auto innerColor = ImGui::GetColorU32( is_active ? ImGuiCol_FrameBgActive : is_hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg );


    float pct01        = (*p_value - v_min) / (v_max - v_min);
    float angle        = cAngleMin + (cAngleMax - cAngleMin) * pct01;
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
        {
            if ( tooltipCallback )
                ImGui::TextUnformatted( tooltipCallback( pct01, *p_value ).c_str() );
            else
                ImGui::Text( "%.0f%%", pct01 * 100.0f );    // default is "<value>%" 
        }
        ImGui::EndTooltip();
    }

    return value_changed;
}

// ---------------------------------------------------------------------------------------------------------------------
bool Spinner( const char* label, bool active, float radius, float thickness, float yOffset, const ImU32& color )
{
    ImGuiWindow* window = GetCurrentWindow();
    if ( window->SkipItems )
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID( label );

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size( (radius) * 2, (radius + style.FramePadding.y) * 2 );

    const ImRect bb( pos, ImVec2( pos.x + size.x, pos.y + size.y + yOffset ) );
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

    const ImVec2 centre = ImVec2( 2.0f + pos.x + radius, pos.y + radius + style.FramePadding.y + yOffset );

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
    const int activeEdge,
    const float height,
    const ImU32 highlightColour,
    const ImU32 backgroundColour )
{
    const ImVec2 pos = ImGui::GetCursorScreenPos();

    const float width       = ImGui::GetContentRegionAvail().x;
    const float widthPerSeg = width / (float)numSegments;

    // bail out without enough space to work
    if ( width <= 10.0f )
        return;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImGui::InvisibleButton( label, ImVec2( width, height ) );

    draw_list->AddRectFilled( pos, pos + ImVec2( width, height ), backgroundColour, 1.0f );

    if ( activeSegment >= 0 )
    {
        const float activeStart = activeSegment * widthPerSeg;
        const float activeEnd   = activeStart + widthPerSeg;

        draw_list->AddRectFilled( ImVec2( pos.x + activeStart, pos.y), ImVec2( pos.x + activeEnd, pos.y + height ), highlightColour, 2.0f );
    }
    if ( activeEdge >= 0 )
    {
        const float activeStart = activeEdge * widthPerSeg;

        draw_list->AddLine( ImVec2( pos.x + activeStart, pos.y - 1 ), ImVec2( pos.x + activeStart, pos.y + height ), IM_COL32_WHITE, 1.0f );
    }
}

// ---------------------------------------------------------------------------------------------------------------------
void VerticalProgress( const char* label, const float progress, const ImU32 highlightColour )
{
    const float totalHeight = (1.0f - std::clamp( progress, 0.0f, 1.0f ));

    ImGuiWindow* window = GetCurrentWindow();
    if ( window->SkipItems || progress <= 0.0f )
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID( label );

    const ImVec2 padding( 2.0f, 2.0f );
    const ImVec2 pos    = window->DC.CursorPos + padding;
    const ImVec2 size   = ImVec2( ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeight() + ( style.FramePadding.y * 2 ) ) - ( padding * 2.0f );

    const ImRect bb( 
        ImVec2( pos.x,          pos.y + size.y * totalHeight ),
        ImVec2( pos.x + size.x, pos.y + size.y ) );

    ItemSize( bb, style.FramePadding.y );
    if ( !ItemAdd( bb, id ) )
        return;


    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled( bb.Min, bb.Max, highlightColour, 1.0f );
}

// ---------------------------------------------------------------------------------------------------------------------
namespace Scoped {

void TickPulses()
{
    g_cycleTimerSlow = (float)(1.0 + std::sin( ImGui::GetTime() * 3.0 )) * 0.5f;
    g_cycleTimerFast = (float)(1.0 + std::sin( ImGui::GetTime() * 6.0 )) * 0.5f;
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

// ---------------------------------------------------------------------------------------------------------------------
ToggleButtonLit::ToggleButtonLit( const bool active, const uint32_t colourU32 )
    : m_active( active )
{
    if ( m_active )
    {
        ImGui::PushStyleColor( ImGuiCol_Button, colourU32 );
        ImGui::PushStyleColor( ImGuiCol_ButtonHovered, colourU32 );
        ImGui::PushStyleColor( ImGuiCol_ButtonActive, colourU32 );
        ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32_BLACK );
    }
}

// ---------------------------------------------------------------------------------------------------------------------
ToggleButtonLit::~ToggleButtonLit()
{
    if ( m_active )
        ImGui::PopStyleColor( 4 );
}

// ---------------------------------------------------------------------------------------------------------------------
FluxButton::FluxButton( const State& state, const ImVec4& buttonColourOn, const ImVec4& textColourOn )
    : m_stylesToPop( 0 )
{
    switch ( state )
    {
        default:
        case State::Off:
            break;

        case State::Flux:
        {
            const float clampedCycleTimer = std::clamp( g_cycleTimerFast, 0.1f, 0.9f );

            const auto buttonColour = lerpVec4(
                ImGui::GetStyleColorVec4( ImGuiCol_ButtonHovered ),
                buttonColourOn,
                clampedCycleTimer );

            const auto textColour = lerpVec4(
                ImGui::GetStyleColorVec4( ImGuiCol_Text ),
                textColourOn,
                clampedCycleTimer );

            ImGui::PushStyleColor( ImGuiCol_Button, buttonColour );
            ImGui::PushStyleColor( ImGuiCol_ButtonHovered, buttonColour );
            ImGui::PushStyleColor( ImGuiCol_ButtonActive, buttonColour );
            ImGui::PushStyleColor( ImGuiCol_Text, textColour );
            m_stylesToPop = 4;
        }
        break;

        case State::On:
            ImGui::PushStyleColor( ImGuiCol_Button, buttonColourOn );
            ImGui::PushStyleColor( ImGuiCol_ButtonHovered, buttonColourOn );
            ImGui::PushStyleColor( ImGuiCol_ButtonActive, buttonColourOn );
            ImGui::PushStyleColor( ImGuiCol_Text, textColourOn );
            m_stylesToPop = 4;
            break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
FluxButton::~FluxButton()
{
    if ( m_stylesToPop > 0 )
        ImGui::PopStyleColor( m_stylesToPop );
}

// ---------------------------------------------------------------------------------------------------------------------
FloatTextRight::FloatTextRight( const char* text )
{
    m_savedCursor = GImGui->CurrentWindow->DC.CursorPos;
    const auto textSize = ImGui::CalcTextSize( text );
    GImGui->CurrentWindow->DC.CursorPos += ImVec2( ImGui::GetContentRegionAvail().x - textSize.x, 0 );
}

// ---------------------------------------------------------------------------------------------------------------------
FloatTextRight::~FloatTextRight()
{
    GImGui->CurrentWindow->DC.CursorPos = m_savedCursor;
}

} // namespace Scoped


namespace Scoped {

Disabled::Disabled( const bool bIsDisabled /* = true */ )
    : m_isDisabled( bIsDisabled )
{
    if ( m_isDisabled )
    {
        PushItemFlag( ImGuiItemFlags_Disabled, true );
        PushStyleVar( ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.25f );
    }
}

Disabled::~Disabled()
{
    if ( m_isDisabled )
    {
        PopItemFlag();
        PopStyleVar();
    }
}

Enabled::Enabled( const bool bIsEnabled /* = true */ )
    : m_isEnabled( bIsEnabled )
{
    if ( !m_isEnabled )
    {
        PushItemFlag( ImGuiItemFlags_Disabled, true );
        PushStyleVar( ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.25f );
    }
}
Enabled::~Enabled()
{
    if ( !m_isEnabled )
    {
        PopItemFlag();
        PopStyleVar();
    }
}

ColourButton::ColourButton( const colour::Preset& buttonColour, const bool bIsColoured )
    : m_isColoured( bIsColoured )
{
    if ( m_isColoured )
    {
        ImGui::PushStyleColor( ImGuiCol_Button,         buttonColour.neutral() );
        ImGui::PushStyleColor( ImGuiCol_ButtonHovered,  buttonColour.light() );
        ImGui::PushStyleColor( ImGuiCol_ButtonActive,   buttonColour.dark() );
        ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32_BLACK );
    }
}
ColourButton::ColourButton( const colour::Preset& buttonColour, const colour::Preset& textColour, const bool bIsColoured )
    : m_isColoured( bIsColoured )
{
    if ( m_isColoured )
    {
        ImGui::PushStyleColor( ImGuiCol_Button,         buttonColour.neutral() );
        ImGui::PushStyleColor( ImGuiCol_ButtonHovered,  buttonColour.light() );
        ImGui::PushStyleColor( ImGuiCol_ButtonActive,   buttonColour.dark() );
        ImGui::PushStyleColor( ImGuiCol_Text,           textColour.neutral() );
    }
}

ColourButton::~ColourButton()
{
    if ( m_isColoured )
        ImGui::PopStyleColor( 4 );
}

} // namespace Scoped

} // namespace ImGui
