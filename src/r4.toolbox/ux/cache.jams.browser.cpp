//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"

#include "ux/cache.jams.browser.h"

#include "endlesss/cache.jams.h"
#include "app/imgui.ext.h"
#include "app/module.frontend.fonts.h"


namespace ux {

void modalUniversalJamBrowser( 
    const char* title,
    const endlesss::cache::Jams& jamCache,
    const UniversalJamBrowserBehaviour& behaviour )
{
    using namespace endlesss::cache;

    static constexpr std::array< Jams::JamType, Jams::cJamTypeCount > jamTypeTabs = {
        Jams::JamType::UserSubscribed,
        Jams::JamType::PublicJoinIn,
        Jams::JamType::PublicArchive,
    };
    static constexpr std::array< const char*, Jams::cJamTypeCount > jamTypeTitle = {
        " Public Jam Archive ",
        " Current Join-In Jams ",
        " Subscribed / Private Jams ",
    };
    static constexpr std::array< const char*, Jams::cJamTypeCount > jamTimeDesc = {
        "Earliest Stem",
        "",
        "Joined",
    };

    const ImVec2 configWindowSize = ImVec2( 750.0f, 420.0f ); // blaze it
    ImGui::SetNextWindowContentSize( configWindowSize );

    const ImVec4 colourJamDisabled = GImGui->Style.Colors[ImGuiCol_TextDisabled];

    ImGui::PushStyleColor( ImGuiCol_PopupBg, ImGui::GetStyleColorVec4( ImGuiCol_ChildBg ) );

    if ( ImGui::BeginPopupModal( title, nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize ) )
    {
        static ImGuiTextFilter jamNameFilter;
        static int32_t jamSortOption = Jams::eIterateSortByTime;

        bool shouldClosePopup = false;

        // wrap up the search function for filtering between public/private jams
        const auto iterationFn = [&]( const Jams::Data& jamData )
        {
            if ( !jamNameFilter.PassFilter( jamData.m_displayName.c_str() ) )
                return;

            const bool showAsDisabled = ( behaviour.fnIsDisabled && behaviour.fnIsDisabled(jamData.m_jamCID) );

            ImGui::TableNextColumn();

            // icon to show jam description as tooltip hover
            if ( jamData.m_description.empty() )
            {
                ImGui::TextDisabled( "   " );
            }
            else
            {
                ImGui::TextDisabled( "[?]" );
                ImGui::CompactTooltip( jamData.m_description.c_str() );
            }
            ImGui::SameLine();

            if ( showAsDisabled )
            {
                ImGui::PushStyleColor( ImGuiCol_Text, colourJamDisabled );
                ImGui::TextUnformatted( jamData.m_displayName.c_str() );
                ImGui::PopStyleColor();
            }
            else
            {
                if ( ImGui::Selectable( jamData.m_displayName.c_str() ) )
                {
                    if ( behaviour.fnOnSelected )
                        behaviour.fnOnSelected( jamData.m_jamCID );

                    // #HDD TODO make closing-on-selection optional?
                    shouldClosePopup = true;
                }
            }

            ImGui::TableNextColumn();
            if ( jamData.m_riffCount > 0 )
                ImGui::Text( "%u", jamData.m_riffCount );

            ImGui::TableNextColumn();
            if ( !jamData.m_timestampOrderingDescription.empty() )
                ImGui::TextUnformatted( jamData.m_timestampOrderingDescription.c_str(), &jamData.m_timestampOrderingDescription.back() + 1 );
        };

        jamNameFilter.Draw( "##NameFilter", 200.0f );

        ImGui::SameLine( 0, 2.0f );
        if ( ImGui::Button( ICON_FA_TIMES_CIRCLE ) )
            jamNameFilter.Clear();

        ImGui::SameLine();
        ImGui::TextUnformatted( "Name Filter" );

        ImGui::SameLine( 0, 220.0f );
        ImGui::TextUnformatted( "Sort By " );

        ImGui::SameLine();
        ImGui::RadioButton( "Time ", &jamSortOption, Jams::eIterateSortByTime );
        ImGui::SameLine();
        ImGui::RadioButton( "Name", &jamSortOption, Jams::eIterateSortByName );

        ImGui::Spacing();
        ImGui::Spacing();
        
        Jams::JamType activeType = Jams::JamType::UserSubscribed;

        if ( ImGui::BeginTabBar( "##jamTypeTab", ImGuiTabBarFlags_None ) )
        {
            for ( const auto jamType : jamTypeTabs )
            {
                if ( ImGui::BeginTabItem( jamTypeTitle[(size_t)jamType] ) )
                {
                    activeType = jamType;
                    ImGui::EndTabItem();
                }
            }

            ImGui::EndTabBar();
        }

        const auto panelRegionAvailable = ImGui::GetContentRegionAvail();
        if ( ImGui::BeginTable( 
            "##jamTable",
            3,
            ImGuiTableFlags_ScrollY             |
                ImGuiTableFlags_Borders         |
                ImGuiTableFlags_RowBg           |
                ImGuiTableFlags_NoSavedSettings,
            ImVec2( panelRegionAvailable.x, panelRegionAvailable.y - 35.0f ) ) )
        {
            ImGui::TableSetupScrollFreeze( 0, 1 );  // top row always visible
            ImGui::TableSetupColumn( "Name" );
            ImGui::TableSetupColumn( "Riffs", ImGuiTableColumnFlags_WidthFixed, 80.0f );
            ImGui::TableSetupColumn( jamTimeDesc[(size_t)activeType], ImGuiTableColumnFlags_WidthFixed, 200.0f );
            ImGui::TableHeadersRow();

            jamCache.iterateJams( iterationFn, activeType, jamSortOption );

            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::Spacing();

        if ( ImGui::Button( "   Close   " ) || shouldClosePopup )
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
}

} // namespace ux
