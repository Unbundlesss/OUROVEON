#include "imgui.h"
#include "imgui_stdlib_ex.h"

void ImGui::TextUnformatted( const std::string_view& str )
{
    ImGui::TextUnformatted( str.data(), &str.back() + 1 );
}

ImVec2 ImGui::CalcTextSize( const std::string_view& str )
{
    return ImGui::CalcTextSize( str.data(), &str.back() + 1 );
}
