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

#include "base/text.h"
#include "base/text.transform.h"

#include "app/core.h"
#include "app/imgui.ext.h"
#include "app/module.frontend.fonts.h"

#include "endlesss/cache.jams.h"
#include "endlesss/config.h"

#include "ux/user.selector.h"


namespace ux {

// ---------------------------------------------------------------------------------------------------------------------
struct UniversalJamBrowserState
{
    using DiveUserContributions = absl::flat_hash_map< endlesss::types::JamCouchID, uint32_t >;

    struct ValidationState
    {
        enum class Mode
        {
            NotValidated,
            Validated,
            FailedToValidate
        }               m_mode = Mode::NotValidated;
        std::string     m_data;
        std::string     m_lastMsg;

        void restart( const std::string& data )
        {
            m_mode = Mode::NotValidated;
            m_data = data;
            m_lastMsg.clear();
        }
    };

    UniversalJamBrowserState() = default;


    void populationLoad( const config::IPathProvider& pathProvider )
    {
        m_diveProcessing = true;

        m_populationDataLoadResult = config::load( pathProvider, m_populationData );

        if ( hasPopulationData() )
        {
            // clear out and reserve some space
            m_diveUserContributions.clear();
            m_diveJamIDs.clear();
            m_diveJamNames.clear();
            m_diveUserRiffCounts.clear();
            m_diveUserPercentage.clear();
            m_diveTotalRiffCounts.clear();
            {
                const auto reserveSize = m_populationData.jampop.size();
                m_diveUserContributions.reserve( reserveSize );
                m_diveJamIDs.reserve( reserveSize );
                m_diveJamNames.reserve( reserveSize );
                m_diveUserRiffCounts.reserve( reserveSize );
                m_diveUserPercentage.reserve( reserveSize );
                m_diveTotalRiffCounts.reserve( reserveSize );
            }

            const std::string& userSearch = m_diveUser.getUsername();

            for ( const auto& jamPair : m_populationData.jampop )
            {
                for ( const auto& userRiffPair : jamPair.second.user_and_riff_count )
                {
                    if ( userRiffPair.first == userSearch )
                    {
                        const float riffCountF = static_cast<float>( jamPair.second.riff_scanned );
                        const float userCountF = static_cast<float>( userRiffPair.second );

                        const float userPercentage = ( 100.0f / riffCountF ) * userCountF;

                        m_diveUserContributions.emplace( jamPair.first, userRiffPair.second );
                        m_diveJamIDs.emplace_back( jamPair.first );
                        m_diveJamNames.emplace_back( jamPair.second.jam_name );
                        m_diveUserRiffCounts.emplace_back( userRiffPair.second );
                        m_diveUserPercentage.emplace_back( userPercentage );
                        m_diveTotalRiffCounts.emplace_back( jamPair.second.riff_scanned );
                    }
                }
            }

            // build index arrays to then presort on the various data we have to offer
            {
                m_diveIndexSortedByName.clear();
                m_diveIndexSortedByRiff.clear();
                m_diveIndexSortedByContrib.clear();
                m_diveIndexSortedByTotals.clear();

                const auto reserveSize = m_diveJamIDs.size();
                m_diveIndexSortedByName.reserve( reserveSize );
                m_diveIndexSortedByRiff.reserve( reserveSize );
                m_diveIndexSortedByContrib.reserve( reserveSize );
                m_diveIndexSortedByTotals.reserve( reserveSize );
            }
            for ( size_t idx = 0; idx < m_diveJamIDs.size(); idx++ )
            {
                m_diveIndexSortedByName.push_back( idx );
                m_diveIndexSortedByRiff.push_back( idx );
                m_diveIndexSortedByContrib.push_back( idx );
                m_diveIndexSortedByTotals.push_back( idx );
            }

            // sort the indices by the data in question
            std::sort( m_diveIndexSortedByName.begin(), m_diveIndexSortedByName.end(),
                [&]( const size_t lhs, const size_t rhs ) -> bool
                {
                    return base::StrToLwrExt( m_diveJamNames[lhs] ) < base::StrToLwrExt( m_diveJamNames[rhs] );
                });
            std::sort( m_diveIndexSortedByRiff.begin(), m_diveIndexSortedByRiff.end(),
                [&]( const size_t lhs, const size_t rhs ) -> bool
                {
                    return m_diveUserRiffCounts[lhs] > m_diveUserRiffCounts[rhs];
                });
            std::sort( m_diveIndexSortedByContrib.begin(), m_diveIndexSortedByContrib.end(),
                [&]( const size_t lhs, const size_t rhs ) -> bool
                {
                    return m_diveUserPercentage[ lhs ] > m_diveUserPercentage[ rhs ];
                });
            std::sort( m_diveIndexSortedByTotals.begin(), m_diveIndexSortedByTotals.end(),
                [&]( const size_t lhs, const size_t rhs ) -> bool
                {
                    return m_diveTotalRiffCounts[ lhs ] > m_diveTotalRiffCounts[ rhs ];
                });
        }

        m_diveConfigureInitialSort = true;
        m_diveProcessing = false;
    }

    bool hasPopulationData() const { return m_populationDataLoadResult == config::LoadResult::Success; }


    void commonValidationImgui( ValidationState& validationState, const std::string& currentData )
    {
        // if ID changes, restart validation process
        if ( validationState.m_data != currentData )
        {
            validationState.restart( currentData );
        }
        switch ( validationState.m_mode )
        {
        case UniversalJamBrowserState::ValidationState::Mode::NotValidated:
        {
            ImGui::SameLine( 0, 32.0f );
            ImGui::TextColored( colour::shades::callout.neutral(), ICON_FA_CIRCLE_INFO " Awaiting Validation" );
        }
        break;

        case UniversalJamBrowserState::ValidationState::Mode::Validated:
        {
            ImGui::SameLine( 0, 32.0f );
            ImGui::TextColored( colour::shades::green.light(), ICON_FA_CIRCLE_CHECK " Access Validated OK" );
        }
        break;

        case UniversalJamBrowserState::ValidationState::Mode::FailedToValidate:
        {
            ImGui::SameLine( 0, 32.0f );
            ImGui::TextColored( colour::shades::errors.light(), ICON_FA_TRIANGLE_EXCLAMATION " Not Available" );
        }
        break;
        }

        ImGui::Spacing();
    }

    enum CustomMode
    {
        PersonalJam,
        InternalBandID,
        ExternalInviteCode,
    }                                           m_addCustomMode;
    std::string                                 m_customBandID;
    std::string                                 m_customInviteCode;

    ValidationState                             m_customBandIDValidation;
    ValidationState                             m_customInviteCodeValidation;


    ImGui::ux::UserSelector                     m_diveUser;
    std::atomic_bool                            m_diveProcessing = false;

    config::endlesss::PopulationPublics         m_populationData;
    config::LoadResult                          m_populationDataLoadResult;

    // data pages for the examined deep dive user
    DiveUserContributions                       m_diveUserContributions;
    std::vector< endlesss::types::JamCouchID >  m_diveJamIDs;
    std::vector< std::string >                  m_diveJamNames;
    std::vector< uint32_t >                     m_diveUserRiffCounts;
    std::vector< float >                        m_diveUserPercentage;
    std::vector< uint32_t >                     m_diveTotalRiffCounts;
    std::vector< std::size_t >                  m_diveIndexSortedByName;
    std::vector< std::size_t >                  m_diveIndexSortedByRiff;
    std::vector< std::size_t >                  m_diveIndexSortedByContrib;
    std::vector< std::size_t >                  m_diveIndexSortedByTotals;
    bool                                        m_diveConfigureInitialSort = true; // setup imgui table sort on first time through post-examine
};

// ---------------------------------------------------------------------------------------------------------------------
UniversalJamBrowserBehaviour::UniversalJamBrowserBehaviour()
    : m_state( std::make_unique< UniversalJamBrowserState >() )
{

}

// ---------------------------------------------------------------------------------------------------------------------
UniversalJamBrowserBehaviour::~UniversalJamBrowserBehaviour()
{}

// ---------------------------------------------------------------------------------------------------------------------
void modalUniversalJamBrowser(
    const char* title,
    const endlesss::cache::Jams& jamCache,
    const UniversalJamBrowserBehaviour& behaviour,
    app::ICoreServices& coreServices )
{
    ABSL_ASSERT( behaviour.m_state != nullptr );
    UniversalJamBrowserState& browserState = *behaviour.m_state;

    // fill in username if it's empty and we have one
    if ( browserState.m_diveUser.isEmpty() )
    {
        auto netCfg = coreServices.getNetworkConfiguration();
        if ( netCfg->hasAccess( endlesss::api::NetConfiguration::Access::Authenticated ) )
        {
            browserState.m_diveUser.setUsername( netCfg->auth().user_id );
        }
    }

    using namespace endlesss::cache;

    enum class ExtraPanels
    {
        None,
        DeepDive,
        CustomID
    };

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

    const ImVec2 configWindowSize = ImVec2( 900.0f, 465.0f );
    ImGui::SetNextWindowContentSize( configWindowSize );

    const ImVec4 colourJamDisabled = GImGui->Style.Colors[ImGuiCol_TextDisabled];

    ImGui::PushStyleColor( ImGuiCol_PopupBg, ImGui::GetStyleColorVec4( ImGuiCol_ChildBg ) );

    if ( ImGui::BeginPopupModal( title, nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize ) )
    {
        const ImVec2 buttonSize( 220.0f, 32.0f );

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
            ImGui::PushID( jamData.m_jamCID.c_str() );

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

            ImGui::PopID();
        };

        jamNameFilter.Draw( "##NameFilter", 200.0f );

        ImGui::SameLine( 0, 2.0f );
        if ( ImGui::Button( ICON_FA_CIRCLE_XMARK ) )
            jamNameFilter.Clear();

        ImGui::SameLine();
        ImGui::TextUnformatted( "Name Filter" );

        ImGui::SameLine( 0, 275.0f );
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
        ExtraPanels currentPanel = ExtraPanels::None;

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
            if ( ImGui::BeginTabItem( " Deep Dive " ) )
            {
                currentPanel = ExtraPanels::DeepDive;
                ImGui::EndTabItem();
            }
            if ( ImGui::BeginTabItem( " Custom ") )
            {
                currentPanel = ExtraPanels::CustomID;
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        const ImVec2 panelRegionAvailable = ImGui::GetContentRegionAvail();
        const ImVec2 panelRegionFill( panelRegionAvailable.x, panelRegionAvailable.y - 35.0f );

        if ( currentPanel == ExtraPanels::None )
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
        else if ( currentPanel == ExtraPanels::DeepDive )
        {
            if ( ImGui::BeginChild( "###customPanel", panelRegionFill ) )
            {
                ImGui::Spacing();
                ImGui::Spacing();

                ImGui::TextWrapped( ICON_FA_BINOCULARS " Deep Dive searches an extensive dataset covering all known public jams and shows which ones the given user has participated in, how many riffs are known about, etc.\n\nNote this data is generated offline (as it takes a while) and therefore is not always up-to-date. Consider it a guide to help dig through the top-level jam data for your hidden riff memories." );
                ImGui::Spacing();
                {
                    browserState.m_diveUser.imgui(
                        "username",
                        coreServices.getEndlesssPopulation(),
                        ImGui::ux::UserSelector::cDefaultWidthForUserSize );

                    ImGui::SameLine();
                    if ( ImGui::Button( " Examine Username " ) )
                    {
                        coreServices.getTaskExecutor().silent_async( [&]()
                            {
                                browserState.populationLoad( coreServices );
                            });
                    }

                    if ( browserState.m_diveProcessing )
                    {
                        ImGui::SameLine();
                        ImGui::AlignTextToFramePadding();
                        ImGui::Spinner( "##syncing", true, ImGui::GetTextLineHeight() * 0.3f, 3.0f, 0.0f, ImGui::GetColorU32( ImGuiCol_Text ) );
                    }
                    else
                    {
                        if ( browserState.hasPopulationData() )
                        {
                            const ImVec2 subRegionAvailable = ImGui::GetContentRegionAvail();
                            const ImVec2 subRegionFill( subRegionAvailable.x, subRegionAvailable.y - 10.0f );

                            if ( ImGui::BeginTable(
                                "##ddJamTable",
                                4,
                                ImGuiTableFlags_Sortable       |
                                ImGuiTableFlags_ScrollY        |
                                ImGuiTableFlags_Borders        |
                                ImGuiTableFlags_RowBg          |
                                ImGuiTableFlags_NoSavedSettings,
                                subRegionFill ) )
                            {
                                ImGui::TableSetupColumn( "Name" );
                                ImGui::TableSetupColumn( "User Riffs",  ImGuiTableColumnFlags_WidthFixed, 110.0f );
                                ImGui::TableSetupColumn( "Of Total",    ImGuiTableColumnFlags_WidthFixed, 110.0f );
                                ImGui::TableSetupColumn( "Total",       ImGuiTableColumnFlags_WidthFixed, 110.0f );
                                ImGui::TableSetupScrollFreeze( 0, 1 );  // top row always visible
                                ImGui::TableHeadersRow();

                                // plug in default sort
                                if ( browserState.m_diveConfigureInitialSort )
                                {
                                    ImGui::TableSetColumnSortDirection( 1, 1, false );
                                    browserState.m_diveConfigureInitialSort = false;
                                }
                                const ImGuiTableSortSpecs* sortingSpec = ImGui::TableGetSortSpecs();

                                const std::size_t totalDiveJams = browserState.m_diveJamIDs.size();
                                for ( std::size_t index = 0; index < totalDiveJams; index++ )
                                {
                                    // default to plain index but usually we have a sort index from the table columns
                                    std::size_t sortedIndex = index;
                                    if ( sortingSpec != nullptr && sortingSpec->SpecsCount == 1 )
                                    {
                                        // handle sort direction by inverting lookup index
                                        std::size_t sortIndexDirection = index;
                                        if ( sortingSpec->Specs[0].SortDirection == 2 )
                                        {
                                            sortIndexDirection = ( totalDiveJams - 1 ) - index;
                                        }

                                        switch ( sortingSpec->Specs[0].ColumnIndex )
                                        {
                                            default:
                                                ABSL_ASSERT( 0 );
                                            case 0: sortedIndex = browserState.m_diveIndexSortedByName[sortIndexDirection]; break;
                                            case 1: sortedIndex = browserState.m_diveIndexSortedByRiff[sortIndexDirection]; break;
                                            case 2: sortedIndex = browserState.m_diveIndexSortedByContrib[sortIndexDirection]; break;
                                            case 3: sortedIndex = browserState.m_diveIndexSortedByTotals[sortIndexDirection]; break;
                                        }
                                    }

                                    const auto& jamID           = browserState.m_diveJamIDs[sortedIndex];
                                    const auto& jamName         = browserState.m_diveJamNames[sortedIndex];
                                    const uint32_t userRiffs    = browserState.m_diveUserRiffCounts[sortedIndex];
                                    const float userPct         = browserState.m_diveUserPercentage[sortedIndex];
                                    const uint32_t totalRiffs   = browserState.m_diveTotalRiffCounts[sortedIndex];

                                    const bool showAsDisabled   = (behaviour.fnIsDisabled && behaviour.fnIsDisabled( jamID ));

                                    ImGui::TableNextColumn();
                                    ImGui::PushID( (int32_t)index );

                                    if ( showAsDisabled )
                                    {
                                        ImGui::PushStyleColor( ImGuiCol_Text, colourJamDisabled );
                                        ImGui::TextUnformatted( jamName );
                                        ImGui::PopStyleColor();
                                    }
                                    else
                                    {
                                        if ( ImGui::Selectable( jamName.c_str() ) )
                                        {
                                            if ( behaviour.fnOnSelected )
                                                behaviour.fnOnSelected( jamID );

                                            // #HDD TODO make closing-on-selection optional?
                                            shouldClosePopup = true;
                                        }
                                    }

                                    ImGui::TableNextColumn();
                                    ImGui::Text( "%u", userRiffs );

                                    ImGui::TableNextColumn();
                                    ImGui::Text( "%.3f %%", userPct );

                                    ImGui::TableNextColumn();
                                    ImGui::Text( "%u", totalRiffs );

                                    ImGui::PopID();
                                }
                                ImGui::EndTable();
                            }
                        }
                    }
                }
                ImGui::Spacing();
            }
            ImGui::EndChild();
        }
        else if ( currentPanel == ExtraPanels::CustomID )
        {
            if ( ImGui::BeginChild( "###customPanel", panelRegionFill ) )
            {
                const bool bHasFullAccess = coreServices.getNetworkConfiguration()->hasAccess( endlesss::api::NetConfiguration::Access::Authenticated );

                ImGui::Spacing();
                ImGui::Spacing();

                if ( !bHasFullAccess )
                {
                    ImGui::TextUnformatted( "Adding custom jams requires full Endlesss authentication" );
                }
                else
                {
                    int32_t currentCustomMode = browserState.m_addCustomMode;
                    {
                        ImGui::RadioButton( "Personal Jam", &currentCustomMode, UniversalJamBrowserState::PersonalJam );
                        ImGui::RadioButton( "Internal Band ID", &currentCustomMode, UniversalJamBrowserState::InternalBandID );
                        ImGui::RadioButton( "External Invite Code", &currentCustomMode, UniversalJamBrowserState::ExternalInviteCode );
                        ImGui::SeparatorBreak();
                    }
                    browserState.m_addCustomMode = (UniversalJamBrowserState::CustomMode)currentCustomMode;

                    switch ( currentCustomMode )
                    {
                        // ---------------------------------------------------------------------------------------------
                        case UniversalJamBrowserState::PersonalJam:
                        {
                            ImGui::TextWrapped( "This is your Solo / private jam. Add and enjoy." );
                            ImGui::Spacing();
                            ImGui::Spacing();

                            if ( bHasFullAccess )
                            {
                                const auto currentUser = coreServices.getNetworkConfiguration()->auth().user_id;

                                ImGui::Text( "Jam ID '%s'", currentUser.c_str() );
                                ImGui::Spacing();

                                if ( ImGui::Button( "Add", buttonSize ) )
                                {
                                    if ( behaviour.fnOnSelected )
                                        behaviour.fnOnSelected( endlesss::types::JamCouchID( currentUser ) );

                                    shouldClosePopup = true;
                                }
                            }
                        }
                        break;

                        // ---------------------------------------------------------------------------------------------
                        case UniversalJamBrowserState::InternalBandID:
                        {
                            auto& validationState = browserState.m_customBandIDValidation;

                            ImGui::TextWrapped( "Add a jam given the internal database ID; this looks like 'band' followed by a series of hex digits, eg. " );
                            ImGui::TextColored( colour::shades::toast.light(), "band52a81b5ecd" );
                            ImGui::Spacing();
                            ImGui::Spacing();

                            ImGui::SetNextItemWidth( buttonSize.x );
                            if ( ImGui::InputText( "Jam ID", &browserState.m_customBandID ) )
                            {
                                // trim whitespace on entry
                                base::trim( browserState.m_customBandID, " " );
                            }
                            
                            browserState.commonValidationImgui( validationState, browserState.m_customBandID );

                            const endlesss::types::JamCouchID jamToValidate( validationState.m_data );

                            {
                                ImGui::Scoped::Disabled sd( validationState.m_data.empty() );
                                if ( ImGui::Button( "Validate", buttonSize ) )
                                {
                                    // trigger a BNS request (even if it doesn't need one); this ensures that the name 
                                    // is propogated nicely to all the various caches and name lookups
                                    coreServices.getEventBusClient().Send< ::events::BNSCacheMiss >( jamToValidate );

                                    // test-run access to whatever they're asking for to avoid adding garbage to the warehouse
                                    // and then also drag the name back to display, help people know what they're about to get
                                    endlesss::api::JamLatestState testPermissions;
                                    endlesss::api::BandPermalinkMeta jamMeta;
                                    if ( testPermissions.fetch( *coreServices.getNetworkConfiguration(), jamToValidate ) &&
                                         jamMeta.fetch( *coreServices.getNetworkConfiguration(), jamToValidate ) )
                                    {
                                        validationState.m_lastMsg = fmt::format( FMTX("\"{}\" can be synced"), jamMeta.data.band_name );
                                        validationState.m_mode = UniversalJamBrowserState::ValidationState::Mode::Validated;
                                    }
                                    else
                                    {
                                        validationState.m_lastMsg = "Failed to fetch jam metadata, cannot sync.";
                                        validationState.m_mode = UniversalJamBrowserState::ValidationState::Mode::FailedToValidate;
                                    }
                                }
                            }

                            // show any status messages
                            if ( !validationState.m_lastMsg.empty() )
                            {
                                ImGui::Spacing();
                                ImGui::SeparatorBreak();
                                ImGui::TextUnformatted( validationState.m_lastMsg );
                                ImGui::Spacing();
                            }

                            if ( validationState.m_mode == UniversalJamBrowserState::ValidationState::Mode::Validated )
                            {
                                if ( ImGui::Button( "Add", buttonSize ) )
                                {
                                    if ( behaviour.fnOnSelected )
                                        behaviour.fnOnSelected( jamToValidate );

                                    shouldClosePopup = true;
                                }
                            }
                        }
                        break;

                        // ---------------------------------------------------------------------------------------------
                        case UniversalJamBrowserState::ExternalInviteCode:
                        {
                            auto& validationState = browserState.m_customInviteCodeValidation;

                            ImGui::TextWrapped( "Resolve an invite code to an internal band ID.\nInvite codes are the long string of hex between /jam/ and /join in (eg.)\n" );
                            ImGui::TextColored( colour::shades::toast.light(), "https://endlesss.fm/jam/7ac545ceaf413321133cf06a735a5cc3a48d7f9234bbf84dcdd808027d5d9b78/join" );
                            ImGui::TextWrapped( "\nPast just the code on its own, so in this case that would be" );
                            ImGui::TextColored( colour::shades::toast.light(), "7ac545ceaf413321133cf06a735a5cc3a48d7f9234bbf84dcdd808027d5d9b78" );
                            ImGui::TextWrapped( "\nOnce resolved, we will switch to the \"Internal Band ID\" page to Validate and add the result.");
                            ImGui::Spacing();
                            ImGui::Spacing();

                            ImGui::SetNextItemWidth( buttonSize.x * 2.5f );
                            if ( ImGui::InputText( "Invite Code", &browserState.m_customInviteCode ) )
                            {
                                // trim whitespace on entry
                                base::trim( browserState.m_customInviteCode, " " );
                            }

                            browserState.commonValidationImgui( validationState, browserState.m_customInviteCode );

                            {
                                ImGui::Scoped::Disabled sd( validationState.m_data.empty() );
                                if ( ImGui::Button( "Resolve", buttonSize ) )
                                {
                                    endlesss::api::BandNameFromExtendedID bandNameResolve;
                                    if ( bandNameResolve.fetch( *coreServices.getNetworkConfiguration(), validationState.m_data ) )
                                    {
                                        // clear the request now we're done with it
                                        browserState.m_customInviteCode.clear();
                                        validationState.m_lastMsg.clear();
                                        validationState.m_mode = UniversalJamBrowserState::ValidationState::Mode::Validated;

                                        // switch back to the jam ID validation phase / add mode
                                        browserState.m_customBandID = bandNameResolve.data.legacy_id;
                                        browserState.m_customBandIDValidation.restart( bandNameResolve.data.legacy_id );
                                        browserState.m_addCustomMode = UniversalJamBrowserState::InternalBandID;
                                    }
                                    else
                                    {
                                        validationState.m_lastMsg = fmt::format( FMTX("Failed to resolve invite code ({})"), bandNameResolve.message );
                                        validationState.m_mode = UniversalJamBrowserState::ValidationState::Mode::FailedToValidate;
                                    }
                                }
                            }
                            // show any status messages
                            if ( !validationState.m_lastMsg.empty() )
                            {
                                ImGui::Spacing();
                                ImGui::SeparatorBreak();
                                ImGui::TextUnformatted( validationState.m_lastMsg );
                                ImGui::Spacing();
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
