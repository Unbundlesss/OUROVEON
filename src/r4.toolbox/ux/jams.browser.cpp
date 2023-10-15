//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#include "pch.h"

#include "ux/jams.browser.h"

#include "endlesss/cache.jams.h"
#include "app/imgui.ext.h"
#include "app/module.frontend.fonts.h"


namespace ux {

void modalUniversalJamBrowser( 
    const char* title,
    const endlesss::cache::Jams& jamCache,
    const UniversalJamBrowserBehaviour& behaviour,
    endlesss::api::NetConfiguration::Shared netConfig )
{
    using namespace endlesss::cache;

    static constexpr std::array< Jams::JamType, Jams::cJamTypeCount > jamTypeTabs = {
        Jams::JamType::UserSubscribed,
        Jams::JamType::PublicJoinIn,
        Jams::JamType::PublicArchive,
        Jams::JamType::Collectible,
    };
    static constexpr std::array< const char*, Jams::cJamTypeCount > jamTypeTitle = {
        " Public Jam Archive ",
        " Current Join-In Jams ",
        " Subscribed / Private Jams ",
        " Collectibles ",
    };
    static constexpr std::array< const char*, Jams::cJamTypeCount > jamTimeDesc = {
        "Earliest Riff",
        "",
        "Joined",
        "Latest Riff",
    };

    const ImVec2 configWindowSize = ImVec2( 820.0f, 460.0f );
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
        if ( ImGui::Button( ICON_FA_CIRCLE_XMARK ) )
            jamNameFilter.Clear();

        ImGui::SameLine();
        ImGui::TextUnformatted( "Name Filter" );

        ImGui::SameLine( 0, 200.0f );
        ImGui::TextUnformatted( "Sort By " );

        ImGui::SameLine();
        ImGui::RadioButton( "Time ", &jamSortOption, Jams::eIterateSortByTime );
        ImGui::SameLine();
        ImGui::RadioButton( "Name", &jamSortOption, Jams::eIterateSortByName );
        ImGui::SameLine();
        ImGui::RadioButton( "Riff", &jamSortOption, Jams::eIterateSortByRiffs );

        ImGui::Spacing();
        ImGui::Spacing();
        
        Jams::JamType activeType = Jams::JamType::UserSubscribed;
        bool bShowCustomPanel = false;

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
            if ( ImGui::BeginTabItem( " Custom ") )
            {
                bShowCustomPanel = true;
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        const ImVec2 panelRegionAvailable = ImGui::GetContentRegionAvail();
        const ImVec2 panelRegionFill( panelRegionAvailable.x, panelRegionAvailable.y - 35.0f );

        if ( !bShowCustomPanel )
        {
            if ( ImGui::BeginTable(
                "##jamTable",
                3,
                ImGuiTableFlags_ScrollY |
                ImGuiTableFlags_Borders |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_NoSavedSettings,
                panelRegionFill ) )
            {
                ImGui::TableSetupScrollFreeze( 0, 1 );  // top row always visible
                ImGui::TableSetupColumn( "Name" );
                ImGui::TableSetupColumn( "Riffs", ImGuiTableColumnFlags_WidthFixed, 80.0f );
                ImGui::TableSetupColumn( jamTimeDesc[(size_t)activeType], ImGuiTableColumnFlags_WidthFixed, 200.0f );
                ImGui::TableHeadersRow();

                jamCache.iterateJams( iterationFn, activeType, jamSortOption );

                ImGui::EndTable();
            }
        }
        else
        {
            if ( ImGui::BeginChild( "###customPanel", panelRegionFill ) )
            {
                static constexpr auto c_validationRequiredMessage = "validate access before adding";
                enum
                {
                    PersonalJam,
                    CustomID
                };
                static std::string customJamName;
                static std::string customJamValidation = c_validationRequiredMessage;
                static int32_t customOption = 0;

                const bool bHasFullAccess = netConfig->hasAccess( endlesss::api::NetConfiguration::Access::Authenticated );

                ImGui::Spacing();
                ImGui::Spacing();

                if ( !bHasFullAccess )
                {
                    ImGui::TextUnformatted( "Adding custom jams requires full Endlesss authentication" );
                }
                else
                {
                    const ImVec2 buttonSize( 200.0f, 32.0f );

                    ImGui::RadioButton( "Personal Jam", &customOption, PersonalJam );
                    ImGui::RadioButton( "Custom Jam ID", &customOption, CustomID );
                    ImGui::SeparatorBreak();

                    switch ( customOption )
                    {
                        case PersonalJam:
                        {
                            if ( bHasFullAccess )
                            {
                                ImGui::Text( "Jam ID '%s'", netConfig->auth().user_id.c_str() );
                                ImGui::Spacing();

                                if ( ImGui::Button( "Add", buttonSize ) )
                                {
                                    if ( behaviour.fnOnSelected )
                                        behaviour.fnOnSelected( endlesss::types::JamCouchID( netConfig->auth().user_id ) );

                                    shouldClosePopup = true;
                                }
                            }
                        }
                        break;

                        case CustomID:
                        {
                            ImGui::SetNextItemWidth( buttonSize.x );
                            if ( ImGui::InputText( "Jam ID", &customJamName ) )
                            {
                                customJamValidation = c_validationRequiredMessage;
                            }
                            if ( !customJamValidation.empty() )
                            {
                                ImGui::SameLine( 0, 16.0f );
                                ImGui::TextColored( ImGui::GetErrorTextColour(), ICON_FA_TRIANGLE_EXCLAMATION " %s", customJamValidation.c_str() );
                            }
                            ImGui::Spacing();

                            ImGui::Scoped::Disabled sd( customJamName.empty() );
                            if ( ImGui::Button( "Validate", buttonSize ) )
                            {
                                // test-run access to whatever they're asking for to avoid adding garbage to the warehouse
                                endlesss::api::JamLatestState testPermissions;
                                if ( testPermissions.fetch( *netConfig, endlesss::types::JamCouchID( customJamName ) ) )
                                {
                                    customJamValidation.clear();
                                }
                                else
                                {
                                    customJamValidation = "failed to validate jam access";
                                }
                            }

                            // if validation has no error message, continue
                            ImGui::Scoped::Enabled se( customJamValidation.empty() );
                            if ( ImGui::Button( "Add", buttonSize ) )
                            {
                                if ( behaviour.fnOnSelected )
                                    behaviour.fnOnSelected( endlesss::types::JamCouchID( customJamName ) );

                                shouldClosePopup = true;
                            }
                        }
                        break;
                    }
                }
            }
            ImGui::EndChild();
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
