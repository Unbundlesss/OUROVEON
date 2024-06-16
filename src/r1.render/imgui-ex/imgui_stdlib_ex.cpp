#include "imgui.h"
#include "imgui_stdlib_ex.h"

void ImGui::TextUnformatted( const std::string_view str )
{
    if ( str.empty() )
        ImGui::TextUnformatted( "" );
    else
        ImGui::TextUnformatted( str.data(), &str.back() + 1 );
}

void ImGui::TextColoredUnformatted( const ImVec4& col, const std::string_view str )
{
    PushStyleColor( ImGuiCol_Text, col );
    if ( str.empty() )
        ImGui::TextUnformatted( "" );
    else
        ImGui::TextUnformatted( str.data(), &str.back() + 1 );
    PopStyleColor();
}

ImVec2 ImGui::CalcTextSize( const std::string_view str )
{
    return ImGui::CalcTextSize( str.data(), &str.back() + 1 );
}
