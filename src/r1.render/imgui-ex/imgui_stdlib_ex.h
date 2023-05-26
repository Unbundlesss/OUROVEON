#pragma once

#include <string>
#include <string_view>

namespace ImGui
{
    IMGUI_API void   TextUnformatted( const std::string& str );
    IMGUI_API ImVec2 CalcTextSize( const std::string& str );
}
