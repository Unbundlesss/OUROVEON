#include "imgui.h"
#include "imgui_stdlib_ex.h"

void ImGui::TextUnformatted( const std::string& str )
{
    ImGui::TextUnformatted( str.c_str(), &str.back() + 1 );
}

ImVec2 ImGui::CalcTextSize( const std::string& str )
{
    return ImGui::CalcTextSize( str.c_str(), &str.back() + 1 );
}
