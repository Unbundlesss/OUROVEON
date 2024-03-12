//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#include "pch.h"

#include "base/paging.h"
#include "base/eventbus.h"

#include "app/imgui.ext.h"
#include "app/module.frontend.fonts.h"
#include "colour/preset.h"

#include "math/rng.h"

#include "endlesss/core.constants.h"
#include "endlesss/core.types.h"

#include "mix/common.h"

#include "ux/proc.weaver.h"

using namespace endlesss;

namespace ux {

// ---------------------------------------------------------------------------------------------------------------------
struct Weaver::State
{
    // a list of presets that I don't really want to hear from anymore
    static constexpr std::array cDreadfulPresetsThatIHate
    {
        "Eardrop",
        "Cortado",
        "Reveal",
        "Strait",
        "Hopes",
        "Justme",
        "Dreamkiss",
        "Spring",
        "Clunnn",
        "Pianabot",
        "Digisect",
        "Strip",
    };
    absl::flat_hash_set< std::string_view > m_dreadfulPresetsThatIHate;


    State( base::EventBusClient eventBus )
        : m_eventBusClient( std::move( eventBus ) )
    {
        {
            math::RNG32 rng( 12345 );
            regenerateSeedText( rng );
        }
        APP_EVENT_BIND_TO( MixerRiffChange );

        m_generatedChannelLock.fill( false );
        m_generatedChannelClearOut.fill( false );

        // stash list of discardable presets as a hash set
        m_dreadfulPresetsThatIHate.reserve( cDreadfulPresetsThatIHate.size() );
        for ( const auto& preset : cDreadfulPresetsThatIHate )
            m_dreadfulPresetsThatIHate.emplace( preset );
    }

    ~State()
    {
        APP_EVENT_UNBIND( MixerRiffChange );
    }

    void event_MixerRiffChange( const events::MixerRiffChange* eventData );


    void imgui(
        app::CoreGUI& coreGUI,
        endlesss::toolkit::Warehouse& warehouse );

    void regenerateSeedText( math::RNG32& rng )
    {
        m_proceduralSeed.clear();
        for ( auto aI = 0; aI < 3; aI++ )
        {
            if ( aI > 0 )
                m_proceduralSeed += '-';

            for ( auto rI = 0; rI < 4; rI++ )
            {
                m_proceduralSeed += (char)rng.genInt32( 'a', 'z' );
            }
        }
    }

    // expand root search using circle-of-fifths
    static void addAdjacentRootsFromCoT( endlesss::toolkit::Warehouse::RiffKeySearchParameters& keySearch, int32_t initialRoot )
    {
        keySearch.m_root.emplace_back( endlesss::constants::cRoot_CoT_CW[initialRoot] );
        keySearch.m_root.emplace_back( endlesss::constants::cRoot_CoT_CCW[initialRoot] );
    }

    using PerChannelIdentities = std::array< endlesss::types::RiffIdentity, 8 >;


    base::EventBusClient            m_eventBusClient;

    endlesss::types::RiffCouchID    m_currentlyPlayingRiffID;
    endlesss::types::RiffCouchIDSet m_enqueuedRiffIDs;

    base::EventListenerID           m_eventLID_MixerRiffChange = base::EventListenerID::invalid();


    endlesss::types::VirtualRiff    m_virtualRiff;

    int32_t                         m_searchOrdering = 1;

    uint32_t                        m_searchRoot = 0;
    uint32_t                        m_searchScale = 0;

    std::vector< endlesss::toolkit::Warehouse::BPMCountTuple >
                                    m_bpmCounts;
    std::vector< std::string >      m_bpmCountsTitles;
    std::size_t                     m_bpmSelection = 0;

    std::string                     m_proceduralSeed;
    std::string                     m_proceduralLog;

    PerChannelIdentities            m_generatedChannelIdentities;
    std::array< std::string, 8 >    m_generatedChannelRef;
    std::array< std::string, 8 >    m_generatedChannelDesc;
    std::array< bool, 8 >           m_generatedChannelLock;
    std::array< bool, 8 >           m_generatedChannelClearOut;

    bool                            m_awaitingBpmSearch = true;
    bool                            m_addAdjacentKeys = true;
    bool                            m_ignoreAnnoyingPresets = true;
};


// ---------------------------------------------------------------------------------------------------------------------
void Weaver::State::event_MixerRiffChange( const events::MixerRiffChange* eventData )
{
    // might be a empty riff, only track actual riffs
    if ( eventData->m_riff != nullptr )
    {
        m_currentlyPlayingRiffID = eventData->m_riff->m_riffData.riff.couchID;
        m_enqueuedRiffIDs.erase( m_currentlyPlayingRiffID );
    }
    else
        m_currentlyPlayingRiffID = endlesss::types::RiffCouchID{};
}

// ---------------------------------------------------------------------------------------------------------------------
void Weaver::State::imgui(
    app::CoreGUI& coreGUI,
    endlesss::toolkit::Warehouse& warehouse )
{
    const float subgroupInsetSize = 16.0f;

    const auto resetSelection = [this]()
        {
            m_bpmCounts.clear();
            m_bpmSelection = 0;
            m_awaitingBpmSearch = true;
        };

    if ( ImGui::Begin( ICON_FA_ARROWS_TO_DOT " Weaver###proc_weaver" ) )
    {
        ImGui::TextColored( colour::shades::callout.light(), "Procedural Search Space" );

        ImGui::SeparatorBreak();
        ImGui::Indent( subgroupInsetSize );

        {
            ImGui::TextUnformatted( "Result Ordering" );

            ImGui::Indent( subgroupInsetSize );
            if ( ImGui::RadioButton( "by BPM, Descending", &m_searchOrdering, 0 ) )
                resetSelection();
            if ( ImGui::RadioButton( "by Riff Count, Descending", &m_searchOrdering, 1 ) )
                resetSelection();
            ImGui::Unindent( subgroupInsetSize );
            ImGui::Dummy( { 0, 10.0f } );
        }

        ImGui::PushItemWidth( 80.0f );

        std::string rootPreview = ImGui::ValueArrayPreviewString(
            endlesss::constants::cRootNames,
            endlesss::constants::cRootValues,
            m_searchRoot );

        if ( ImGui::ValueArrayComboBox( "Root :", "##p_root",
            endlesss::constants::cRootNames,
            endlesss::constants::cRootValues,
            m_searchRoot,
            rootPreview,
            12.0f ) )
        {
            resetSelection();
        }

        ImGui::PopItemWidth();
        ImGui::SameLine( 0, 4.0f );
        if ( ImGui::Checkbox( " With Adjacent Keys", &m_addAdjacentKeys ) )
        {
            resetSelection();
        }

        ImGui::SameLine( 0, 50.0f );
        ImGui::PushItemWidth( 220.0f );

        std::string scalePreview = ImGui::ValueArrayPreviewString(
            endlesss::constants::cScaleNames,
            endlesss::constants::cScaleValues,
            m_searchScale );

        if ( ImGui::ValueArrayComboBox( "Scale :", "##p_scale",
            endlesss::constants::cScaleNames,
            endlesss::constants::cScaleValues,
            m_searchScale,
            scalePreview,
            .0f ) )
        {
            resetSelection();
        }

        ImGui::PopItemWidth();

        if ( m_awaitingBpmSearch )
        {
            endlesss::toolkit::Warehouse::RiffKeySearchParameters keySearch;
            keySearch.m_root.emplace_back( m_searchRoot );
            keySearch.m_scale.emplace_back( m_searchScale );
            keySearch.m_ignoreAnnoyingPresets = m_ignoreAnnoyingPresets;

            if ( m_addAdjacentKeys )
                addAdjacentRootsFromCoT( keySearch, m_searchRoot );

            warehouse.filterRiffsByBPM(
                keySearch,
                ( m_searchOrdering == 0 ) ? toolkit::Warehouse::BPMCountSort::ByBPM : toolkit::Warehouse::BPMCountSort::ByCount,
                m_bpmCounts );

            // precreate the display titles for each BPM/count reference
            m_bpmCountsTitles.clear();
            m_bpmCountsTitles.reserve( m_bpmCounts.size() );
            for ( const auto& bpmCount : m_bpmCounts )
            {
                m_bpmCountsTitles.emplace_back( fmt::format( FMTX( " {0:4} BPM - {1} riffs" ), bpmCount.m_BPM, bpmCount.m_count ) );
            }

            m_awaitingBpmSearch = false;
        }

        if ( !m_bpmCounts.empty() )
        {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted( " BPM :");
            ImGui::SameLine();
            ImGui::SetNextItemWidth( 250.0f );

            bool bpmChoiceChanged = false;
            if ( ImGui::BeginCombo( "##p_bpm", m_bpmCountsTitles[m_bpmSelection].c_str()) )
            {
                for ( std::size_t optI = 0; optI < m_bpmCountsTitles.size(); optI++ )
                {
                    const bool selected = optI == m_bpmSelection;
                    if ( ImGui::Selectable( m_bpmCountsTitles[optI].c_str(), selected) )
                    {
                        m_bpmSelection = optI;
                        bpmChoiceChanged = true;
                    }
                    if ( selected )
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::SameLine( 0, 60.0f );
            ImGui::Checkbox( " Ignore Tired Presets", &m_ignoreAnnoyingPresets );


            ImGui::Unindent( subgroupInsetSize );
            ImGui::Spacing();
            ImGui::SeparatorBreak();
            ImGui::Spacing();

            if ( ImGui::Button( " Formulate with Seed : " ) )
            {
                const auto newSeed = absl::Hash<std::string>{}(m_proceduralSeed);
                m_proceduralLog = fmt::format( FMTX( "Procedural generation seeded from '{}' => {}\n" ), m_proceduralSeed, newSeed );

                math::RNG32 rng( base::reduce64To32( newSeed ) );
                regenerateSeedText( rng );

                // choose how many stems to pick
                int32_t channelsToFill = rng.genInt32( 2, 8 );

                // setup new vriff basis
                m_virtualRiff.user      = "procgen";
                m_virtualRiff.root      = m_searchRoot;
                m_virtualRiff.scale     = m_searchScale;
                m_virtualRiff.barLength = 4;
                m_virtualRiff.BPMrnd    = static_cast< float >( m_bpmCounts[m_bpmSelection].m_BPM );

                // clean out any unlocked channels
                for ( uint32_t chI = 0; chI < 8; chI++ )
                {
                    if ( m_generatedChannelLock[chI] == false || m_generatedChannelClearOut[chI] == true )
                    {
                        m_virtualRiff.stemsOn[chI]          = false;
                        m_virtualRiff.stemBarLengths[chI]   = 4;
                        m_virtualRiff.gains[chI]            = 0;
                        m_virtualRiff.stems[chI]            = {};

                        m_generatedChannelIdentities[chI]   = {};
                        m_generatedChannelRef[chI]          = {};
                        m_generatedChannelDesc[chI]         = {};
                    }
                    // if not removing them, still reconsider bar length of any locked ones
                    else
                    {
                        m_virtualRiff.barLength = std::max( m_virtualRiff.barLength, m_virtualRiff.stemBarLengths[chI] );
                    }
                }

                endlesss::types::StemCouchIDSet usedStemIDs;
                endlesss::types::StemCouchIDs   potentialStemIDs;
                std::vector< float >            potentialStemGains;
                std::vector< float >            potentialStemBarLength;
                
                endlesss::toolkit::Warehouse::RiffKeySearchParameters keySearch;
                keySearch.m_root.emplace_back( m_virtualRiff.root );
                keySearch.m_scale.emplace_back( m_virtualRiff.scale );
                keySearch.m_ignoreAnnoyingPresets = m_ignoreAnnoyingPresets;

                if ( m_addAdjacentKeys )
                    addAdjacentRootsFromCoT( keySearch, m_virtualRiff.root );

                for ( uint32_t chI = 0; chI < 8; chI++ )
                {
                    endlesss::types::RiffComplete randomRiff;
                    if ( warehouse.fetchRandomRiffBySeed(
                        keySearch,
                        m_bpmCounts[m_bpmSelection].m_BPM,
                        rng.genInt32(),
                        randomRiff ) )
                    {
                        potentialStemIDs.clear();
                        potentialStemGains.clear();
                        potentialStemBarLength.clear();

                        for ( std::size_t stemI = 0; stemI < 8; stemI ++ )
                        {
                            if ( randomRiff.riff.stemsOn[stemI] && 
                                 usedStemIDs.contains( randomRiff.riff.stems[stemI] ) == false )
                            {
                                if ( m_ignoreAnnoyingPresets && m_dreadfulPresetsThatIHate.contains( randomRiff.stems[stemI].preset ) )
                                {
                                    blog::database( FMTX( "ignoring shit preset: {}" ), randomRiff.stems[stemI].preset );
                                    continue;
                                }

                                potentialStemIDs.emplace_back( randomRiff.riff.stems[stemI] );
                                potentialStemGains.emplace_back( randomRiff.riff.gains[stemI] );
                                potentialStemBarLength.emplace_back( randomRiff.stems[stemI].barLength );
                            }
                        }

                        int32_t chosenStemIndex = 0;
                        if ( potentialStemIDs.size() > 1 )
                        {
                            chosenStemIndex = rng.genInt32( 0, (int32_t)potentialStemIDs.size() - 1 );
                        }

                        if ( !potentialStemIDs.empty() )
                        {
                            // go find an available unlocked channel to write a new random choice into
                            int32_t availableChannelIndex = -1;
                            for ( int32_t chI = 0; chI < 8; chI++ )
                            {
                                if ( m_virtualRiff.stems[chI].empty() &&
                                     m_generatedChannelClearOut[chI] == false &&
                                     m_generatedChannelLock[chI]     == false )     // allow the lock value to keep an empty channel empty too
                                {
                                    availableChannelIndex = chI;
                                    break;
                                }
                            }
                            // .. if we have a free slot
                            if ( availableChannelIndex != -1 )
                            {
                                m_virtualRiff.barLength = std::max( m_virtualRiff.barLength, potentialStemBarLength[chosenStemIndex] );

                                // write new stem
                                m_virtualRiff.stemsOn[availableChannelIndex]        = true;
                                m_virtualRiff.stems[availableChannelIndex]          = potentialStemIDs[chosenStemIndex];
                                m_virtualRiff.stemBarLengths[availableChannelIndex] = potentialStemBarLength[chosenStemIndex];
                                m_virtualRiff.gains[availableChannelIndex]          = rng.genFloat(
                                    potentialStemGains[chosenStemIndex] * 0.8f,
                                    potentialStemGains[chosenStemIndex] );

                                usedStemIDs.emplace( potentialStemIDs[chosenStemIndex] );

                                // stash data about it for display on the UI
                                const auto exportTimeUnix  = spacetime::InSeconds( std::chrono::seconds( static_cast<uint64_t>(randomRiff.riff.creationTimeUnix) ) );
                                const auto exportTimeDelta = spacetime::calculateDeltaFromNow( exportTimeUnix ).asPastTenseString( 3 );

                                m_generatedChannelIdentities[availableChannelIndex] = { randomRiff.jam.couchID, randomRiff.riff.couchID };

                                m_generatedChannelRef[availableChannelIndex] = fmt::format( FMTX( "[R:{}]\n[S:{}]" ),
                                    randomRiff.riff.couchID,
                                    m_virtualRiff.stems[availableChannelIndex] );

                                m_generatedChannelDesc[availableChannelIndex] = fmt::format( FMTX( "{:36} {:2} | {}" ),
                                    randomRiff.jam.displayName,
                                    endlesss::constants::cRootNames[ randomRiff.riff.root ],
                                    exportTimeDelta );
                            }
                        }
                    }

                    channelsToFill--;
                    if ( channelsToFill <= 0 )
                        break;
                }

                const auto newVirtualIdentity = warehouse.createNewVirtualRiff( m_virtualRiff );
                m_eventBusClient.Send< ::events::EnqueueRiffPlayback >( newVirtualIdentity );
            }

            ImGui::SameLine( 0, 6.0f );
            ImGui::SetNextItemWidth( 300.0f );
            ImGui::InputText( "##rng_seed", &m_proceduralSeed );
            ImGui::SameLine( 0, 6.0f );
            if ( ImGui::Button( ICON_FA_ARROWS_ROTATE ) )
            {
                math::RNG32 ndrng;
                regenerateSeedText( ndrng );
            }

            ImGui::Spacing();
            ImGui::Spacing();

            if ( ImGui::BeginTable( "###generation_results", 4, ImGuiTableFlags_NoSavedSettings) )
            {
                ImGui::TableSetupColumn( "Lock",        ImGuiTableColumnFlags_WidthFixed, 22 );
                ImGui::TableSetupColumn( "Tooltip",     ImGuiTableColumnFlags_WidthFixed, 22 );
                ImGui::TableSetupColumn( "Description", ImGuiTableColumnFlags_WidthStretch, 1.0f );
                ImGui::TableSetupColumn( "ClearOut",    ImGuiTableColumnFlags_WidthFixed, 22 );

                // build some iconographic headers
                {
                    ImGui::TableNextColumn();
                    ImGui::AlignTextToFramePadding();
                    ImGui::Dummy( { 4, 0 } );
                    ImGui::SameLine( 0, 0 );
                    ImGui::TextUnformatted( ICON_FA_LOCK );
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    ImGui::AlignTextToFramePadding();
                    ImGui::Dummy( { 3, 0 } );
                    ImGui::SameLine( 0, 0 );
                    ImGui::TextUnformatted( ICON_FA_BAN );
                }

                // render the generated channel stats and controls
                for ( uint32_t chI = 0; chI < 8; chI++ )
                {
                    ImGui::PushID( (int32_t)chI );

                    ImGui::TableNextColumn();
                    ImGui::Checkbox( "##channel_lock", &m_generatedChannelLock[chI] );

                    ImGui::TableNextColumn();
                    {
                        ImGui::Scoped::Disabled sd( m_generatedChannelRef[chI].empty() );
                        if ( ImGui::Button( ICON_FA_GRIP, {-1, 0}) )
                        {
                            m_eventBusClient.Send< ::events::RequestNavigationToRiff >( m_generatedChannelIdentities[chI] );
                        }
                        ImGui::CompactTooltip( m_generatedChannelRef[chI] );
                    }

                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth( -FLT_MIN );
                    ImGui::InputText( "##gen_data", &m_generatedChannelDesc[chI], ImGuiInputTextFlags_ReadOnly );

                    ImGui::TableNextColumn();
                    ImGui::Checkbox( "##channel_clear", &m_generatedChannelClearOut[chI] );

                    ImGui::PopID();
                }

                ImGui::EndTable();
            }
        }

    }
    ImGui::End();
}



// ---------------------------------------------------------------------------------------------------------------------
Weaver::Weaver( base::EventBusClient eventBus )
    : m_state( std::make_unique<State>( std::move( eventBus ) ) )
{
}

// ---------------------------------------------------------------------------------------------------------------------
Weaver::~Weaver()
{
}

// ---------------------------------------------------------------------------------------------------------------------
void Weaver::imgui( app::CoreGUI& coreGUI, endlesss::toolkit::Warehouse& warehouse )
{
    m_state->imgui( coreGUI, warehouse );
}

} // namespace ux
