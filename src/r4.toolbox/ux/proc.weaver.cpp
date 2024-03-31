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
    absl::flat_hash_set< std::string_view > m_dreadfulPresetsThatIHate;


    State( const config::IPathProvider& pathProvider, base::EventBusClient eventBus )
        : m_eventBusClient( std::move( eventBus ) )
    {
        {
            math::RNG32 rng( 12345 );
            regenerateSeedText( rng );
        }
        APP_EVENT_BIND_TO( MixerRiffChange );

        m_generatedChannelLock.fill( false );
        m_generatedChannelClearOut.fill( false );

        // load the weaver config file if we can
        const auto dataLoad = config::load( pathProvider, m_weaverConfig );
        if ( dataLoad == config::LoadResult::Success )
        {
            // stash list of discardable presets as a hash set
            m_dreadfulPresetsThatIHate.reserve( m_weaverConfig.presetsWeHate.size() );
            for ( const auto& preset : m_weaverConfig.presetsWeHate )
                m_dreadfulPresetsThatIHate.emplace( preset );
        }
        else
        {
            blog::app( FMTX( "weaver : unable to load {}" ), m_weaverConfig.StorageFilename );
        }
    }

    ~State()
    {
        APP_EVENT_UNBIND( MixerRiffChange );
    }

    void event_MixerRiffChange( const events::MixerRiffChange* eventData );

    void imgui(
        app::CoreGUI& coreGUI,
        endlesss::live::RiffPtr& currentRiffPtr,
        net::bond::RiffPushClient& bondClient,
        endlesss::toolkit::Warehouse& warehouse );

    // using the given RNG, generate a new simple seed string
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

    // run the generation code, produce a new virtual riff, enqueue and play it / send over bond etc if required
    void generateNewRiff(
        app::CoreGUI& coreGUI,
        net::bond::RiffPushClient& bondClient,
        endlesss::toolkit::Warehouse& warehouse,
        int32_t generateSingleChannelAtIndex = -1 );    // dynamic mask - can ask to just re-roll a single channel without modifying the other flags

    void buildVirtualRiffFromLive(
        const endlesss::live::Riff* liveRiff );

    // take the current state of m_generatedResult, bundle it into the DB and send it out to be played
    void finaliseAndSendVirtualRiff( endlesss::toolkit::Warehouse& warehouse );


    using PerChannelIdentities = std::array< endlesss::types::RiffIdentity, 8 >;

    struct GeneratedResult
    {
        template<class Archive>
        inline void serialize( Archive& archive )
        {
            archive( CEREAL_NVP( m_virtualRiff )
                   , CEREAL_NVP( m_identities )
                   , CEREAL_NVP( m_stemRef )
                   , CEREAL_NVP( m_stemJamName )
                   , CEREAL_NVP( m_stemTimeDelta )
                   , CEREAL_NVP( m_stemRoot )
                   , CEREAL_NVP( m_stemScale )
            );
        }

        // generated riff
        endlesss::types::VirtualRiff    m_virtualRiff;

        // ui data
        PerChannelIdentities            m_identities;
        std::array< std::string, 8 >    m_stemRef;
        std::array< std::string, 8 >    m_stemJamName;
        std::array< std::string, 8 >    m_stemTimeDelta;
        std::array< int32_t, 8 >        m_stemRoot;
        std::array< int32_t, 8 >        m_stemScale;

        void clearChannel( std::size_t index )
        {
            m_virtualRiff.stemsOn[index]        = false;
            m_virtualRiff.stemBarLengths[index] = 4;
            m_virtualRiff.gains[index]          = 0;
            m_virtualRiff.stems[index]          = {};

            m_identities[index]         = {};
            m_stemRef[index]            = {};
            m_stemJamName[index]        = {};
            m_stemTimeDelta[index]      = {};
            m_stemRoot[index]           = {};
            m_stemScale[index]          = {};
        }
    };


    config::Weaver                  m_weaverConfig;

    base::EventBusClient            m_eventBusClient;

    endlesss::types::RiffCouchID    m_currentlyPlayingRiffID;
    endlesss::types::RiffCouchIDSet m_enqueuedRiffIDs;
    endlesss::types::RiffCouchIDSet m_enqueuedRiffIDToAutoSendToBOND;

    base::EventListenerID           m_eventLID_MixerRiffChange = base::EventListenerID::invalid();


    int32_t                         m_searchOrdering = 1;

    uint32_t                        m_searchRoot = 0;
    uint32_t                        m_searchScale = 0;
    int64_t                         m_deferredBPMSearch = -1;               // set to >0 to do a latent match on a searched root/scale in the next tick

    std::vector< endlesss::toolkit::Warehouse::BPMCountTuple >
                                    m_bpmCounts;
    std::vector< std::string >      m_bpmCountsTitles;
    std::size_t                     m_bpmSelection = 0;

    std::string                     m_proceduralSeed;
    std::string                     m_proceduralPreviousSeed;

    GeneratedResult                 m_generatedResult;

    std::array< bool, 8 >           m_generatedChannelLock;
    std::array< bool, 8 >           m_generatedChannelClearOut;


    endlesss::constants::HarmonicSearch::Enum                               // other key/modes to include in a search to expand the potential space
                                    m_harmonicSearch            = endlesss::constants::HarmonicSearch::CloselyRelated;

    bool                            m_awaitingVirualJamClear    = true;     // if true, purge all the virtual riff records from the warehouse to avoid it bloating infinitely
    bool                            m_awaitingBpmSearch         = true;     // BPM search runs each time we change search space parameters
    bool                            m_ignoreAnnoyingPresets     = true;     // fuck off, Eardrop
    bool                            m_autoSendToBOND            = false;    // automatically send committed riffs across BOND once we have word that they got loaded & dequeued
    std::atomic_bool                m_generationInProgress      = false;
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
void Weaver::State::generateNewRiff(
    app::CoreGUI& coreGUI,
    net::bond::RiffPushClient& bondClient,
    endlesss::toolkit::Warehouse& warehouse,
    int32_t generateSingleChannelAtIndex /*= -1*/ )
{
    const auto newSeed = absl::Hash<std::string>{}(m_proceduralSeed);
    blog::app( FMTX( "Procedural generation seeded from '{}' => {}\n" ), m_proceduralSeed, newSeed );

    m_generationInProgress = true;
    coreGUI.getTaskExecutor().silent_async( [this, newSeed, generateSingleChannelAtIndex, &warehouse]
        {
            math::RNG32 rng( base::reduce64To32( newSeed ) );

            // stash current seed to display on screen, re-roll it automatically
            m_proceduralPreviousSeed = m_proceduralSeed;
            regenerateSeedText( rng );

            // choose how many stems to pick to scatter about
            int32_t channelsToFill = rng.genInt32( 2, 8 );

            // .. just generating one?
            if ( generateSingleChannelAtIndex >= 0 )
                channelsToFill = 1;


            // setup new vriff basis
            m_generatedResult.m_virtualRiff.user = "[weaver]";
            m_generatedResult.m_virtualRiff.root = m_searchRoot;
            m_generatedResult.m_virtualRiff.scale = m_searchScale;
            m_generatedResult.m_virtualRiff.barLength = 4;
            m_generatedResult.m_virtualRiff.BPMrnd = static_cast<float>(m_bpmCounts[m_bpmSelection].m_BPM);

            // clean out any unlocked channels
            for ( uint32_t chI = 0; chI < 8; chI++ )
            {
                bool clearOutChannel = false;
                // force clear out of a specific channel?
                if ( generateSingleChannelAtIndex >= 0 )
                {
                    if ( generateSingleChannelAtIndex == chI )
                        clearOutChannel = true;
                }
                // clean it if we aren't locking this channel or the specific clear-out flag is set
                else
                {
                    clearOutChannel = ( m_generatedChannelLock[chI] == false || m_generatedChannelClearOut[chI] == true );
                }

                if ( clearOutChannel )
                {
                    m_generatedResult.clearChannel( chI );
                }
                // if not removing them, still reconsider bar length of any locked ones
                else
                {
                    m_generatedResult.m_virtualRiff.barLength = std::max(
                        m_generatedResult.m_virtualRiff.barLength,
                        m_generatedResult.m_virtualRiff.stemBarLengths[chI]
                    );
                }
            }

            endlesss::types::StemCouchIDSet usedStemIDs;
            endlesss::types::StemCouchIDs   potentialStemIDs;
            std::vector< float >            potentialStemGains;
            std::vector< float >            potentialStemBarLength;

            // setup a search query
            endlesss::constants::RootScalePair initialRootScale{ m_generatedResult.m_virtualRiff.root, m_generatedResult.m_virtualRiff.scale };
            endlesss::constants::RootScalePairs adjacents;
            {
                adjacents.searchMode = m_harmonicSearch;
                adjacents.pairs.emplace_back( initialRootScale );
                endlesss::constants::computeTonalAdjacents(
                    initialRootScale,
                    adjacents );
            }

            for ( uint32_t chI = 0; chI < 8; chI++ )
            {
                endlesss::types::RiffComplete randomRiff;
                if ( warehouse.fetchRandomRiffBySeed(
                    adjacents,
                    m_bpmCounts[m_bpmSelection].m_BPM,
                    rng.genInt32(),
                    randomRiff ) )
                {
                    potentialStemIDs.clear();
                    potentialStemGains.clear();
                    potentialStemBarLength.clear();

                    for ( std::size_t stemI = 0; stemI < 8; stemI++ )
                    {
                        if ( randomRiff.riff.stemsOn[stemI] &&
                             usedStemIDs.contains( randomRiff.riff.stems[stemI] ) == false )
                        {
                            if ( m_ignoreAnnoyingPresets && m_dreadfulPresetsThatIHate.contains( randomRiff.stems[stemI].preset ) )
                            {
                                blog::app( FMTX( "weaver : ignoring shit preset: {}" ), randomRiff.stems[stemI].preset );
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
                        int32_t availableChannelIndex = -1;

                        // with a specific channel choice, just target that one (assuming 'clear out' isn't selected on it, leave it blank if so)
                        if ( generateSingleChannelAtIndex >= 0 )
                        {
                            if ( m_generatedChannelClearOut[generateSingleChannelAtIndex] == false )
                                availableChannelIndex = generateSingleChannelAtIndex;
                        }
                        // go find an available unlocked channel to write a new random choice into
                        else
                        {
                            for ( int32_t chI = 0; chI < 8; chI++ )
                            {
                                if ( m_generatedResult.m_virtualRiff.stems[chI].empty() &&
                                    m_generatedChannelClearOut[chI] == false &&
                                    m_generatedChannelLock[chI] == false )     // allow the lock value to keep an empty channel empty too
                                {
                                    availableChannelIndex = chI;
                                    break;
                                }
                            }
                        }

                        // .. if we have a free slot
                        if ( availableChannelIndex != -1 )
                        {
                            m_generatedResult.m_virtualRiff.barLength = std::max(
                                m_generatedResult.m_virtualRiff.barLength,
                                potentialStemBarLength[chosenStemIndex]
                            );

                            // write new stem
                            m_generatedResult.m_virtualRiff.stemsOn[availableChannelIndex] = true;
                            m_generatedResult.m_virtualRiff.stems[availableChannelIndex] = potentialStemIDs[chosenStemIndex];
                            m_generatedResult.m_virtualRiff.stemBarLengths[availableChannelIndex] = potentialStemBarLength[chosenStemIndex];
                            m_generatedResult.m_virtualRiff.gains[availableChannelIndex] = rng.genFloat(
                                potentialStemGains[chosenStemIndex] * 0.8f,
                                potentialStemGains[chosenStemIndex] );

                            m_generatedResult.m_identities[availableChannelIndex] = { randomRiff.jam.couchID, randomRiff.riff.couchID };

                            usedStemIDs.emplace( potentialStemIDs[chosenStemIndex] );

                            // stash data about it for display on the UI
                            m_generatedResult.m_stemRef[availableChannelIndex] = fmt::format( FMTX( "[R:{}]\n[S:{}]" ),
                                randomRiff.riff.couchID,
                                m_generatedResult.m_virtualRiff.stems[availableChannelIndex] );

                            m_generatedResult.m_stemJamName[availableChannelIndex] = randomRiff.jam.displayName;

                            const auto exportTimeUnix = spacetime::InSeconds( std::chrono::seconds( static_cast<uint64_t>(randomRiff.riff.creationTimeUnix) ) );
                            m_generatedResult.m_stemTimeDelta[availableChannelIndex] = spacetime::calculateDeltaFromNow( exportTimeUnix ).asPastTenseString( 2 );

                            m_generatedResult.m_stemRoot[availableChannelIndex] = randomRiff.riff.root;
                            m_generatedResult.m_stemScale[availableChannelIndex] = randomRiff.riff.scale;
                        }
                    }
                }

                channelsToFill--;
                if ( channelsToFill <= 0 )
                    break;
            }

            finaliseAndSendVirtualRiff( warehouse );

            m_generationInProgress = false;
        });
}

// ---------------------------------------------------------------------------------------------------------------------
void Weaver::State::buildVirtualRiffFromLive(
    const endlesss::live::Riff* liveRiff )
{
    ABSL_ASSERT( liveRiff );

    m_generatedResult.m_virtualRiff.user        = liveRiff->m_riffData.riff.user;
    m_generatedResult.m_virtualRiff.root        = liveRiff->m_riffData.riff.root;
    m_generatedResult.m_virtualRiff.scale       = liveRiff->m_riffData.riff.scale;
    m_generatedResult.m_virtualRiff.barLength   = liveRiff->m_riffData.riff.barLength;
    m_generatedResult.m_virtualRiff.BPMrnd      = liveRiff->m_riffData.riff.BPMrnd;

    // adopt the root/scale etc to match this incoming riff
    m_searchRoot    = liveRiff->m_riffData.riff.root;
    m_searchScale   = liveRiff->m_riffData.riff.scale;

    // ask for a search and match on the next UI tick
    m_deferredBPMSearch = (int64_t)std::round( liveRiff->m_riffData.riff.BPMrnd );

    for ( uint32_t chI = 0; chI < 8; chI++ )
    {
        if ( liveRiff->m_riffData.riff.stemsOn[chI] )
        {
            m_generatedResult.m_virtualRiff.stemsOn[chI]        = true;
            m_generatedResult.m_virtualRiff.stems[chI]          = liveRiff->m_riffData.riff.stems[chI];
            m_generatedResult.m_virtualRiff.stemBarLengths[chI] = liveRiff->m_riffData.stems[chI].barLength;
            m_generatedResult.m_virtualRiff.gains[chI]          = liveRiff->m_riffData.riff.gains[chI];

            m_generatedResult.m_identities[chI] = { liveRiff->m_riffData.jam.couchID, liveRiff->m_riffData.riff.couchID };

            m_generatedResult.m_stemRef[chI] = fmt::format( FMTX( "[R:{}]\n[S:{}]" ),
                liveRiff->m_riffData.riff.couchID,
                m_generatedResult.m_virtualRiff.stems[chI] );


            m_generatedResult.m_stemJamName[chI] = liveRiff->m_riffData.jam.displayName;

            const auto exportTimeUnix = spacetime::InSeconds( std::chrono::seconds( static_cast<uint64_t>(liveRiff->m_riffData.stems[chI].creationTimeUnix) ) );
            m_generatedResult.m_stemTimeDelta[chI] = spacetime::calculateDeltaFromNow( exportTimeUnix ).asPastTenseString( 2 );

            m_generatedResult.m_stemRoot[chI] = liveRiff->m_riffData.riff.root;
            m_generatedResult.m_stemScale[chI] = liveRiff->m_riffData.riff.scale;
        }
        else
        {
            m_generatedResult.clearChannel( chI );
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
void Weaver::State::finaliseAndSendVirtualRiff( endlesss::toolkit::Warehouse& warehouse )
{
    // insert new virtual riff into warehouse, returning a new bespoke riff ID
    const auto newVirtualIdentity = warehouse.createNewVirtualRiff( m_generatedResult.m_virtualRiff );

    // log our requests to play this new riff and stash a note to send it over BOND if possible, if selected
    m_enqueuedRiffIDs.emplace( newVirtualIdentity.getRiffID() );
    if ( m_autoSendToBOND )
        m_enqueuedRiffIDToAutoSendToBOND.emplace( newVirtualIdentity.getRiffID() );

    // ping out to play it
    m_eventBusClient.Send< ::events::EnqueueRiffPlayback >( newVirtualIdentity );
}

// ---------------------------------------------------------------------------------------------------------------------
void Weaver::State::imgui(
    app::CoreGUI& coreGUI,
    endlesss::live::RiffPtr& currentRiffPtr,
    net::bond::RiffPushClient& bondClient,
    endlesss::toolkit::Warehouse& warehouse )
{
    // take temporary copy of the shared pointer, in case it gets modified mid-tick by the mixer
    // fetch a workable pointer to the currently playing riff, if we have one
    endlesss::live::RiffPtr localRiffPtr = currentRiffPtr;
    const auto currentRiff  = localRiffPtr.get();
    const bool currentRiffIsValid = ( currentRiff &&
                                      currentRiff->m_syncState == endlesss::live::Riff::SyncState::Success );


    const float subgroupInsetSize = 16.0f;

    const auto resetSelection = [this]()
        {
            m_bpmCounts.clear();
            m_bpmSelection = 0;
            m_awaitingBpmSearch = true;
        };

    // clean out virtual jam from warehouse, otherwise can expand endlessly
    if ( m_awaitingVirualJamClear )
    {
        warehouse.clearOutVirtualJamStorage();
        m_awaitingVirualJamClear = false;
    }

    // check on auto-bond-send requests
    const bool BONDConnectionLive = (bondClient.getState() == net::bond::Connected);
    if ( m_enqueuedRiffIDToAutoSendToBOND.contains( m_currentlyPlayingRiffID ) )
    {
        m_enqueuedRiffIDToAutoSendToBOND.erase( m_currentlyPlayingRiffID );
        if ( BONDConnectionLive )
        {
            bondClient.pushRiffById( 
                endlesss::types::JamCouchID{ endlesss::toolkit::Warehouse::cVirtualJamName },
                m_currentlyPlayingRiffID,
                std::nullopt );
        }
    }

    if ( ImGui::Begin( ICON_FA_ARROWS_TO_DOT " Weaver###proc_weaver" ) )
    {
        ImGui::TextColored( colour::shades::callout.light(), "Stem Search Space" );

        ImGui::SeparatorBreak();
        ImGui::Indent( subgroupInsetSize );

        {
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
        }
        ImGui::SameLine( 0, 12.0f );
        {
            ImGui::PushItemWidth( 233.0f );

            std::string scalePreview = ImGui::ValueArrayPreviewString(
                endlesss::constants::cScaleNames,
                endlesss::constants::cScaleValues,
                m_searchScale );

            if ( ImGui::ValueArrayComboBox( nullptr, "##p_scale",
                endlesss::constants::cScaleNames,
                endlesss::constants::cScaleValues,
                m_searchScale,
                scalePreview,
                .0f ) )
            {
                resetSelection();
            }

            ImGui::PopItemWidth();
        }


        {
            ImGui::SameLine( 0, 20.0f );
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted( "Harmonic Search Mode :" );
            ImGui::SameLine();
            ImGui::TextDisabled( "[?]" );
            ImGui::CompactTooltip( "Include closely related keys or other interesting key/modes" );
        }



        if ( m_awaitingBpmSearch || m_deferredBPMSearch > 0 )
        {
            endlesss::constants::RootScalePair initialRootScale{ m_searchRoot, m_searchScale };
            endlesss::constants::RootScalePairs adjacents;
            {
                adjacents.searchMode = m_harmonicSearch;
                adjacents.pairs.emplace_back( initialRootScale );
                endlesss::constants::computeTonalAdjacents(
                    initialRootScale,
                    adjacents );

            }

            warehouse.filterRiffsByBPM(
                adjacents,
                ( m_searchOrdering == 0 ) ? toolkit::Warehouse::BPMCountSort::ByBPM : toolkit::Warehouse::BPMCountSort::ByCount,
                m_bpmCounts );

            // precreate the display titles for each BPM/count reference
            m_bpmCountsTitles.clear();
            m_bpmCountsTitles.reserve( m_bpmCounts.size() );
            for ( const auto& bpmCount : m_bpmCounts )
            {
                m_bpmCountsTitles.emplace_back( fmt::format( FMTX( " {0:4} BPM ( {1} riffs, {2} jams )" ), bpmCount.m_BPM, bpmCount.m_riffCount, bpmCount.m_jamCount ) );
                if ( m_deferredBPMSearch == bpmCount.m_BPM )
                {
                    m_bpmSelection = m_bpmCountsTitles.size() - 1;
                }
            }

            m_awaitingBpmSearch = false;
            m_deferredBPMSearch = 0;
        }

        {
            ImGui::Spacing();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted( " BPM :");
            ImGui::SameLine();
            ImGui::SetNextItemWidth( 325.0f );

            bool bpmChoiceChanged = false;
            if ( !m_bpmCounts.empty() )
            {
                if ( ImGui::BeginCombo( "##p_bpm", m_bpmCountsTitles[m_bpmSelection].c_str() ) )
                {
                    for ( std::size_t optI = 0; optI < m_bpmCountsTitles.size(); optI++ )
                    {
                        const bool selected = optI == m_bpmSelection;
                        if ( ImGui::Selectable( m_bpmCountsTitles[optI].c_str(), selected ) )
                        {
                            m_bpmSelection = optI;
                            bpmChoiceChanged = true;
                        }
                        if ( selected )
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }
            else
            {
                std::string customNote = " - No Riffs Found - ";
                ImGui::InputText( "###note", &customNote, ImGuiInputTextFlags_ReadOnly );
            }

            {
                ImGui::SameLine( 0, 18.0f );
                ImGui::PushItemWidth( 280.0f );
                if ( endlesss::constants::HarmonicSearch::ImGuiCombo( "##harmonic", m_harmonicSearch ) )
                {
                    resetSelection();
                }
                ImGui::PopItemWidth();
            }

            {
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted( "       Sort by" );

                ImGui::SameLine();
                if ( ImGui::RadioButton( "BPM", &m_searchOrdering, 0 ) )
                    resetSelection();
                ImGui::SameLine();
                if ( ImGui::RadioButton( "Riff Count", &m_searchOrdering, 1 ) )
                    resetSelection();
            }



            ImGui::Unindent( subgroupInsetSize );

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::SeparatorBreak();
            ImGui::Spacing();
            ImGui::Spacing();

            ImGui::TextColored( colour::shades::callout.light(), "Generation" );
            {
                const float cSeedButtonWidth = 280.0f;
                ImGui::RightAlignSameLine( cSeedButtonWidth );
                ImGui::Scoped::Enabled se( currentRiffIsValid );
                if ( ImGui::Button( ICON_FA_MAGNET " Seed from Currently Playing ", { cSeedButtonWidth, 0.0f }) )
                {
                    buildVirtualRiffFromLive( currentRiff );
                }
            }
            ImGui::Spacing();
            {
                ImGui::Checkbox( " Ignore Tired Presets", &m_ignoreAnnoyingPresets );
            }

            {
                ImGui::Scoped::Disabled sd( m_generationInProgress.load() );
                if ( ImGui::Button( " Formulate with Seed : " ) )
                {
                    generateNewRiff( coreGUI, bondClient, warehouse );
                }

                ImGui::SameLine( 0, 6.0f );
                ImGui::SetNextItemWidth( 200.0f );
                ImGui::InputText( "##rng_seed", &m_proceduralSeed );
                ImGui::SameLine( 0, 6.0f );
                if ( ImGui::Button( ICON_FA_ARROWS_ROTATE ) )
                {
                    math::RNG32 ndrng;
                    regenerateSeedText( ndrng );
                }
                {
                    ImGui::Scoped::Enabled se( BONDConnectionLive );

                    ImGui::SameLine( 0, 6.0f );
                    ImGui::Checkbox( "AutoBOND", &m_autoSendToBOND );

                    // mask out auto-bond option if we have no connection
                    m_autoSendToBOND &= BONDConnectionLive;
                }
                if ( m_generationInProgress || !m_enqueuedRiffIDs.empty() )
                {
                    const float SpinnerSize = ImGui::GetTextLineHeight() * 0.5f;
                    ImGui::RightAlignSameLine( SpinnerSize * 3.0f );
                    ImGui::Spinner( "##playback_queued", true, SpinnerSize, 4.0f, 0.0f,
                        m_generationInProgress ? colour::shades::slate.lightU32() : colour::shades::orange.lightU32() );
                }
            }

            ImGui::Spacing();
            ImGui::Spacing();

            if ( ImGui::BeginTable( "###generation_results", 8, ImGuiTableFlags_NoSavedSettings) )
            {
                ImGui::TableSetupColumn( "Lock",        ImGuiTableColumnFlags_WidthFixed, 22 );
                ImGui::TableSetupColumn( "ClearOut",    ImGuiTableColumnFlags_WidthFixed, 22 );
                ImGui::TableSetupColumn( "Navigate",    ImGuiTableColumnFlags_WidthFixed, 22 );
                ImGui::TableSetupColumn( "Key",         ImGuiTableColumnFlags_WidthFixed, 22 );
                ImGui::TableSetupColumn( "Description", ImGuiTableColumnFlags_WidthStretch, 0.6f );
                ImGui::TableSetupColumn( "Timeline",    ImGuiTableColumnFlags_WidthStretch, 0.4f );
                ImGui::TableSetupColumn( "ReRoll",      ImGuiTableColumnFlags_WidthFixed, 22 );
                ImGui::TableSetupColumn( "Delete",      ImGuiTableColumnFlags_WidthFixed, 22 );

                // build some iconographic headers
                {
                    ImGui::PushStyleVar( ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.4f );

                    ImGui::TableNextColumn();
                    {
                        ImGui::AlignTextToFramePadding();
                        ImGui::Dummy( { 4, 0 } );
                        ImGui::SameLine( 0, 0 );
                        ImGui::TextUnformatted( ICON_FA_LOCK );
                    }
                    ImGui::TableNextColumn();
                    {
                        ImGui::AlignTextToFramePadding();
                        ImGui::Dummy( { 3, 0 } );
                        ImGui::SameLine( 0, 0 );
                        ImGui::TextUnformatted( ICON_FA_BAN );
                    }
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    {
                        ImGui::AlignTextToFramePadding();
                        ImGui::Dummy( { 3, 0 } );
                        ImGui::SameLine( 0, 0 );
                        ImGui::TextUnformatted( ICON_FA_MUSIC );
                    }
                    ImGui::TableNextColumn();
                    {
                        ImGui::SetNextItemWidth( -FLT_MIN );
                        ImGui::InputText( "##previous_seed", &m_proceduralPreviousSeed, ImGuiInputTextFlags_ReadOnly );
                    }
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    {
                        ImGui::AlignTextToFramePadding();
                        ImGui::Dummy( { 4, 0 } );
                        ImGui::SameLine( 0, 0 );
                        ImGui::TextUnformatted( ICON_FA_DICE_D20 );
                    }
                    ImGui::TableNextColumn();
                    {
                        ImGui::AlignTextToFramePadding();
                        ImGui::Dummy( { 3, 0 } );
                        ImGui::SameLine( 0, 0 );
                        ImGui::TextUnformatted( ICON_FA_TRASH_CAN );
                    }

                    ImGui::PopStyleVar();
                }

                // render the generated channel stats and controls
                for ( uint32_t chI = 0; chI < 8; chI++ )
                {
                    ImGui::PushID( (int32_t)chI );

                    ImGui::TableNextColumn();
                    ImGui::Checkbox( "##channel_lock", &m_generatedChannelLock[chI] );
                    ImGui::TableNextColumn();
                    ImGui::Checkbox( "##channel_clear", &m_generatedChannelClearOut[chI] );

                    ImGui::TableNextColumn();
                    {
                        ImGui::Scoped::Disabled sd( m_generatedResult.m_stemRef[chI].empty() );
                        if ( ImGui::Button( ICON_FA_GRIP, {-1, 0}) )
                        {
                            m_eventBusClient.Send< ::events::RequestNavigationToRiff >( m_generatedResult.m_identities[chI] );
                        }
                        ImGui::CompactTooltip( m_generatedResult.m_stemRef[chI] );
                    }

                    ImGui::TableNextColumn();
                    {
                        if ( !m_generatedResult.m_stemJamName[chI].empty() )
                        {
                            ImGui::TextUnformatted( endlesss::constants::cRootNames[m_generatedResult.m_stemRoot[chI]] );
                            ImGui::CompactTooltip( endlesss::constants::cScaleNames[m_generatedResult.m_stemScale[chI]] );
                        }
                    }

                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth( -FLT_MIN );
                    ImGui::InputText( "##jam_name", &m_generatedResult.m_stemJamName[chI], ImGuiInputTextFlags_ReadOnly );

                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth( -FLT_MIN );
                    ImGui::InputText( "##time_delta", &m_generatedResult.m_stemTimeDelta[chI], ImGuiInputTextFlags_ReadOnly );


                    {
                        ImGui::Scoped::Disabled sd( m_generationInProgress.load() );

                        ImGui::TableNextColumn();
                        if ( ImGui::Button( ICON_FA_ARROWS_SPIN ) )
                        {
                            generateNewRiff( coreGUI, bondClient, warehouse, chI );
                        }
                        ImGui::TableNextColumn();
                        if ( ImGui::Button( ICON_FA_XMARK ) )
                        {
                            m_generationInProgress = true;
                            coreGUI.getTaskExecutor().silent_async( [this, chI, &warehouse]
                                {
                                    m_generatedResult.clearChannel( chI );
                                    finaliseAndSendVirtualRiff( warehouse );
                                    m_generationInProgress = false;
                                });
                        }
                    }


                    ImGui::PopID();
                }

                ImGui::EndTable();
            }
        }

    }
    ImGui::End();
}


// ---------------------------------------------------------------------------------------------------------------------
Weaver::Weaver( const config::IPathProvider& pathProvider, base::EventBusClient eventBus )
    : m_state( std::make_unique<State>( pathProvider, std::move( eventBus ) ) )
{
}

// ---------------------------------------------------------------------------------------------------------------------
Weaver::~Weaver()
{
}

// ---------------------------------------------------------------------------------------------------------------------
void Weaver::imgui(
    app::CoreGUI& coreGUI,
    endlesss::live::RiffPtr& currentRiffPtr,      // currently playing riff, may be null
    net::bond::RiffPushClient& bondClient,
    endlesss::toolkit::Warehouse& warehouse )
{
    m_state->imgui( coreGUI, currentRiffPtr, bondClient, warehouse );
}

} // namespace ux
