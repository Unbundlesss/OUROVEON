//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"
#include "ux/jam.validate.h"

#include "spacetime/chronicle.h"

#include "app/imgui.ext.h"

#include "endlesss/cache.stems.h"
#include "endlesss/live.stem.h"
#include "endlesss/toolkit.warehouse.h"

namespace ux {

// ---------------------------------------------------------------------------------------------------------------------
void modalJamValidate(
    const char* title,
    JamValidateState& jamValidateState,
    const struct endlesss::toolkit::Warehouse& warehouse,
    const endlesss::api::NetConfiguration::Shared& netCfg,
    tf::Executor& taskExecutor )
{
    const ImVec2 configWindowSize = ImVec2( 830.0f, 240.0f );
    ImGui::SetNextWindowContentSize( configWindowSize );

    ImGui::PushStyleColor( ImGuiCol_PopupBg, ImGui::GetStyleColorVec4( ImGuiCol_ChildBg ) );

    if ( ImGui::BeginPopupModal( title, nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize ) )
    {
        jamValidateState.imgui( warehouse, netCfg, taskExecutor );

        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
}

// ---------------------------------------------------------------------------------------------------------------------
void JamValidateState::imgui(
    const endlesss::toolkit::Warehouse& warehouse,
    const endlesss::api::NetConfiguration::Shared& netCfg,
    tf::Executor& taskExecutor )
{
    const ImVec2 buttonSize( 240.0f, 32.0f );

    ImGui::TextUnformatted( "Not Available Yet!" );

    if ( ImGui::BottomRightAlignedButton( "Close", buttonSize ) )
    {
        ImGui::CloseCurrentPopup();
    }
}

} // namespace ux
