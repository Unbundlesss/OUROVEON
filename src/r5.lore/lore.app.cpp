#include "pch.h"

namespace stdp = std::placeholders;

#include "base/utils.h"
#include "base/metaenum.h"
#include "base/instrumentation.h"
#include "base/text.h"
#include "base/bimap.h"

#include "buffer/buffer.2d.h"

#include "colour/gradient.h"

#include "config/frontend.h"
#include "config/data.h"
#include "config/audio.h"

#include "app/module.audio.h"
#include "app/module.frontend.h"
#include "app/module.frontend.fonts.h"
#include "app/module.midi.h"

#include "app/ouro.h"

#include "ux/diskrecorder.h"
#include "ux/cache.jams.browser.h"

#include "vx/stembeats.h"
#include "vx/vibes.h"

#include "discord/discord.bot.ui.h"

#include "endlesss/all.h"

#include "effect/effect.stack.h"

#include "mix/preview.h"
#include "mix/stem.amalgam.h"

#include "gfx/sketchbook.h"

#include "net/bond.riffpush.h"
#include "net/bond.riffpush.connect.h"

#define OUROVEON_LORE           "LORE"
#define OUROVEON_LORE_VERSION   OURO_FRAMEWORK_VERSION "-alpha"



#define _WH_VIEW(_action)           \
      _action(Default)              \
      _action(Maintenance)
REFLECT_ENUM( WarehouseView, uint32_t, _WH_VIEW );

std::string generateWarehouseViewTitle( const WarehouseView::Enum _vwv )
{
#define _ACTIVE_ICON(_ty)             _vwv == WarehouseView::_ty ? ICON_FC_FILLED_SQUARE : ICON_FC_HOLLOW_SQUARE,
#define _ICON_PRINT(_ty)             "{}"

    return fmt::format( FMTX( "Data Warehouse [" _WH_VIEW( _ICON_PRINT ) "]###data_warehouse_view" ),
        _WH_VIEW( _ACTIVE_ICON )
        "" );

#undef _ICON_PRINT
#undef _ACTIVE_ICON
}

#undef _WH_VIEW



struct JamVisualisation
{
    #define _LINE_BREAK_ON(_action)   \
        _action(Never)                \
        _action(ChangedBPM)           \
        _action(ChangedScaleOrRoot)   \
        _action(TimePassing)
    REFLECT_ENUM( LineBreakOn, uint32_t, _LINE_BREAK_ON );
    #undef _LINE_BREAK_ON

    #define _RIFF_GAP_ON(_action)     \
        _action(Never)                \
        _action(ChangedBPM)           \
        _action(ChangedScaleOrRoot)   \
        _action(TimePassing)
    REFLECT_ENUM( RiffGapOn, uint32_t, _RIFF_GAP_ON );
    #undef _RIFF_GAP_ON

    #define _COLOUR_STYLE(_action)    \
        _action( Uniform )            \
        _action( UserIdentity )       \
        _action( UserChangeCycle )    \
        _action( UserChangeRate )     \
        _action( StemChurn )          \
        _action( StemTimestamp )      \
        _action( Scale )              \
        _action( Root )               \
        _action( BPM )
    REFLECT_ENUM( ColourStyle, uint32_t, _COLOUR_STYLE );
    #undef _COLOUR_STYLE

    #define _COLOUR_SOURCE(_action)         \
        _action( GradientGrayscale )        \
        _action( GradientBlueOrange )       \
        _action( GradientPlasma )           \
        _action( GradientRainbowVibrant )   \
        _action( GradientRainbow )          \
        _action( GradientTurbo )            \
        _action( GradientViridis )          \
        _action( GradientMagma )
    REFLECT_ENUM( ColourSource, uint32_t, _COLOUR_SOURCE );
    #undef _COLOUR_SOURCE

    static constexpr size_t NameHighlightCount = 3;
    struct NameHighlighting
    {
        NameHighlighting()
            : m_colour( 0.5f, 0.8f, 1.0f, 1.0f )
        {}

        std::string         m_name;
        ImVec4              m_colour;
    };
    using NameHighlightingArray = std::array< NameHighlighting, NameHighlightCount >;

    NameHighlightingArray   m_nameHighlighting;

    LineBreakOn::Enum       m_lineBreakOn       = LineBreakOn::ChangedBPM;
    RiffGapOn::Enum         m_riffGapOn         = RiffGapOn::Never;
    ColourStyle::Enum       m_colourStyle       = ColourStyle::UserChangeRate;
    ColourSource::Enum      m_colourSource      = ColourSource::GradientPlasma;

    // defaults that can be then tuned on the UI depending which colour viz is running
    float                   m_bpmMinimum        = 50.0f;
    float                   m_bpmMaximum        = 200.0f;
    float                   m_activityTimeSec   = 30.0f;
    float                   m_changeRateDecay   = 0.8f;
    std::array< float, 4 >  m_uniformColour     = { 0.5, 0.5, 0.5, 1.0f };

    template<class Archive>
    void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( m_lineBreakOn )
               , CEREAL_NVP( m_riffGapOn )
               , CEREAL_NVP( m_colourStyle )
               , CEREAL_NVP( m_colourSource )
               , CEREAL_NVP( m_bpmMinimum )
               , CEREAL_NVP( m_bpmMaximum )
               , CEREAL_NVP( m_activityTimeSec )
               , CEREAL_NVP( m_changeRateDecay )
        );
    }

    inline uint32_t colourSampleT( const float t ) const
    {
        switch ( m_colourSource )
        {
            case ColourSource::GradientGrayscale:       return colour::gradient_grayscale_u32( t );
            case ColourSource::GradientBlueOrange:      return colour::gradient_blueorange_u32( t );
            case ColourSource::GradientPlasma:          return colour::gradient_plasma_u32( t );
            case ColourSource::GradientRainbowVibrant:  return colour::gradient_rainbow_ultra_u32( t );
            case ColourSource::GradientRainbow:         return colour::gradient_rainbow_u32( t );
            case ColourSource::GradientTurbo:           return colour::gradient_turbo_u32( t );
            case ColourSource::GradientViridis:         return colour::gradient_viridis_u32( t );
            case ColourSource::GradientMagma:           return colour::gradient_magma_u32( t );
            default:
                return 0xFF00FFFF;
        }
    }

    inline bool imgui()
    {
        bool choiceChanged = false;

        const float columnInset = 16.0f;
        const float panelWidth  = ImGui::GetContentRegionAvail().x;
        const float columnWidth = panelWidth * 0.5f;

        ImGui::Columns( 2, nullptr, false );
        ImGui::PushItemWidth( ( columnWidth * 0.75f ) - columnInset );

        {
            ImGui::TextUnformatted( "User Highlighting" );
            ImGui::Spacing();
            ImGui::Indent( columnInset );
            for ( auto nH = 0; nH < m_nameHighlighting.size(); nH ++ )
            {
                ImGui::PushID( nH );
                choiceChanged |= ImGui::ColorEdit3( "##Colour", &m_nameHighlighting[nH].m_colour.x , ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayHex );
                                 ImGui::SameLine();
                choiceChanged |= ImGui::InputText( "Name", &m_nameHighlighting[nH].m_name, ImGuiInputTextFlags_EnterReturnsTrue );
                choiceChanged |= ImGui::IsItemDeactivatedAfterEdit();
                                 ImGui::CompactTooltip( "enter an Endlesss username to have their riffs highlighted in the view" );
                ImGui::PopID();
            }
            ImGui::Unindent( columnInset );

            ImGui::TextUnformatted( "Layout" );
            ImGui::Spacing();
            ImGui::Indent( columnInset );
            {
                choiceChanged |= LineBreakOn::ImGuiCombo( "Line Break", m_lineBreakOn );
                                 ImGui::CompactTooltip( "Add a line-break between blocks of riffs, based on configurable differences" );
                choiceChanged |= RiffGapOn::ImGuiCombo( "Riff Gap", m_riffGapOn );
                                 ImGui::CompactTooltip( "Add one-block gaps between riffs, based on configurable differences" );
            }
            ImGui::Unindent( columnInset );
        }
        ImGui::NextColumn();
        {
            ImGui::TextUnformatted( "Cell Colouring" );
            ImGui::Spacing();
            ImGui::Indent( columnInset );
            {
                choiceChanged |= ColourStyle::ImGuiCombo( "Colour Style", m_colourStyle );

                if ( m_colourStyle == ColourStyle::Uniform )
                {
                    choiceChanged |= ImGui::ColorEdit3( "Uniform Colour", &m_uniformColour[0], ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_DisplayHex );
                }
                else
                {
                    choiceChanged |= ColourSource::ImGuiCombo( "Colour Source", m_colourSource );

                    if ( m_colourStyle == ColourStyle::BPM )
                        choiceChanged |= ImGui::DragFloatRange2( "BPM Range", &m_bpmMinimum, &m_bpmMaximum, 1.0f, 25.0f, 999.0f, "%.0f" );
                    else if ( m_colourStyle == ColourStyle::StemChurn )
                        choiceChanged |= ImGui::InputFloat( "Cooldown Time", &m_activityTimeSec, 1.0f, 5.0f, " %.0f Seconds" );
                    else if ( m_colourStyle == ColourStyle::UserChangeRate )
                        choiceChanged |= ImGui::InputFloat( "Decay Rate", &m_changeRateDecay, 0.05f, 0.1f, " %.2f" );
                    else
                        ImGui::TextUnformatted( "" );
                }
            }
            ImGui::Unindent( columnInset );
        }

        ImGui::PopItemWidth();
        ImGui::Columns( 1 );
        ImGui::Spacing();

        return choiceChanged;
    }
};

// ---------------------------------------------------------------------------------------------------------------------
struct LoreApp : public app::OuroApp
{
    LoreApp()
        : app::OuroApp()
        , m_rpClient( GetAppName() )
    {
        m_discordBotUI = std::make_unique<discord::BotWithUI>( *this, m_configDiscord );
    }

    ~LoreApp()
    {
    }

    const char* GetAppName() const override { return OUROVEON_LORE; }
    const char* GetAppNameWithVersion() const override { return (OUROVEON_LORE " " OUROVEON_LORE_VERSION); }
    const char* GetAppCacheName() const override { return "lore"; }

    bool supportsOfflineEndlesssMode() const override { return true; }

    int EntrypointOuro() override;

    void initMidi()
    {
        m_midiInputDevices = app::module::Midi::fetchListOfInputDevices();

        registerMainMenuEntry( 10, "BOND", [this]()
        {
            if ( ImGui::BeginMenu( "Riff Push" ) )
            {
                if ( ImGui::MenuItem( "Connect ..." ) )
                {
                    activateModalPopup( "Riff Push Connection", [this]( const char* title )
                    {
                        modalRiffPushClientConnection( title, m_mdFrontEnd, m_rpClient );
                    });
                }
                
                bool canReplayJam  = ( m_jamSliceSketch != nullptr ) && ( m_jamSliceSketch->m_slice != nullptr );
                     canReplayJam &= m_rpClient.getState() == net::bond::Connected;

                if ( ImGui::MenuItem( "Jam Replay ...", nullptr, false, canReplayJam ) )
                {
                    activateModalPopup( "Jam Replay Tool", [this]( const char* title )
                    {
                        endlesss::types::RiffPlaybackAbstraction defaultAbstraction;
                        const auto defaultPermutation = defaultAbstraction.asPermutation();

                        static std::size_t startOffsetIndex = 0;

                        ImGui::Text( "%u Riffs to replay", (uint32_t)(m_jamSliceSketch->m_slice->m_ids.size() - startOffsetIndex) );
                        ImGui::Separator();
                        ImGui::Text( "Prefetch Index : %u", m_jamSliceReplayState.m_riffPrefetchIndex );
                        ImGui::Text( "Playback Index : %u", m_jamSliceReplayState.m_currentRiffIndex );
                        ImGui::Separator();

                        const auto triggerNextRiffSend = [this, &defaultPermutation]()
                        {
                            endlesss::live::RiffPtr riffPtr;
                            m_jamSliceReplayState.m_riffsToSend.try_dequeue( riffPtr );

                            m_rpClient.pushRiff( riffPtr->m_riffData, defaultPermutation );
                            m_jamSliceReplayState.m_currentRiffSentAt.restart();

                            m_jamSliceReplayState.m_currentRiffDurationInSeconds = riffPtr->getTimingDetails().m_lengthInSec - ( riffPtr->getTimingDetails().m_lengthInSecPerBar * 0.2 );
                            m_jamSliceReplayState.m_replaySequenceLengthSeconds += riffPtr->getTimingDetails().m_lengthInSec;

                            const auto currentRiffID = riffPtr->m_riffData.riff.couchID;
                            const auto expectedRiffID = m_jamSliceSketch->m_slice->m_ids[m_jamSliceReplayState.m_currentRiffIndex];

                            if ( currentRiffID != expectedRiffID )
                                blog::error::app( "REPLAY : id mismatch; got {}, expected {}", currentRiffID, expectedRiffID );

                            blog::app( "REPLAY : {} for {}", currentRiffID, m_jamSliceReplayState.m_currentRiffDurationInSeconds );
                        };

                        if ( m_jamSliceReplayState.m_state == JamSliceReplayState::State::Idle )
                        {
                            ImGui::Text( "Start Offset : %u", startOffsetIndex );
                            if ( ImGui::Button( "Start At Selected" ) )
                            {
                                endlesss::live::RiffPtr currentRiffPtr = m_nowPlayingRiff;
                                const auto currentRiff = currentRiffPtr.get();

                                for ( std::size_t i = 0; i < m_jamSliceSketch->m_slice->m_ids.size(); i++ )
                                {
                                    if ( m_jamSliceSketch->m_slice->m_ids[i] == currentRiff->m_riffData.riff.couchID )
                                    {
                                        startOffsetIndex = i;
                                        break;
                                    }
                                }
                            }
                            if ( ImGui::Button( "Begin" ) )
                            {
                                m_jamSliceReplayState.m_riffPrefetchIndex = startOffsetIndex;
                                m_jamSliceReplayState.m_currentRiffIndex  = startOffsetIndex;

                                m_jamSliceReplayState.m_state = JamSliceReplayState::State::Warmstart;

                                for ( std::size_t p = 0; p < 4; p++ )
                                {
                                    blog::app( "REPLAY : prefetch[{}] = {}", m_jamSliceReplayState.m_riffPrefetchIndex, m_jamSliceSketch->m_slice->m_ids[m_jamSliceReplayState.m_riffPrefetchIndex + p] );

                                    m_jamSliceReplayState.m_replayPipeline->requestRiff( {
                                        {
                                            m_jamSliceSketch->m_slice->m_jamID,
                                            m_jamSliceSketch->m_slice->m_ids[m_jamSliceReplayState.m_riffPrefetchIndex]
                                        },
                                        defaultPermutation } );

                                    m_jamSliceReplayState.m_riffPrefetchIndex++;
                                }
                            }
                        }
                        else if ( m_jamSliceReplayState.m_state == JamSliceReplayState::State::Warmstart )
                        {
                            ImGui::TextUnformatted( "Prefetching ..." );
                            if ( m_jamSliceReplayState.m_riffsToSend.size_approx() > 2 )
                            {
                                triggerNextRiffSend();

                                m_jamSliceReplayState.m_state = JamSliceReplayState::State::Primed;
                            }
                        }
                        else if ( m_jamSliceReplayState.m_state == JamSliceReplayState::State::Primed )
                        {
                            ImGui::TextUnformatted( "Initial riff sent, waiting ..." );
                            if ( ImGui::Button( "Continue" ) )
                            {
                                m_jamSliceReplayState.m_currentRiffSentAt.restart();
                                m_jamSliceReplayState.m_state = JamSliceReplayState::State::Sending; 
                            }
                        }
                        else if ( m_jamSliceReplayState.m_state == JamSliceReplayState::State::Sending )
                        {
                            const double secondsSinceLastSend = (double)m_jamSliceReplayState.m_currentRiffSentAt.deltaMs().count() * 0.001;

                            ImGui::Text( "Sending [%i], %.2fs since last, current riff length is %.2fs", 
                                m_jamSliceReplayState.m_currentRiffIndex, 
                                secondsSinceLastSend,
                                m_jamSliceReplayState.m_currentRiffDurationInSeconds );

                            ImGui::Text( "Replay length : %.2fs", m_jamSliceReplayState.m_replaySequenceLengthSeconds );

                            if ( secondsSinceLastSend >= m_jamSliceReplayState.m_currentRiffDurationInSeconds )
                            {
                                m_jamSliceReplayState.m_currentRiffIndex++;
                                triggerNextRiffSend();

                                if ( m_jamSliceReplayState.m_riffPrefetchIndex < m_jamSliceSketch->m_slice->m_ids.size() )
                                {
                                    blog::app( "REPLAY > prefetch[{}] = {}", m_jamSliceReplayState.m_riffPrefetchIndex, m_jamSliceSketch->m_slice->m_ids[m_jamSliceReplayState.m_riffPrefetchIndex] );

                                    m_jamSliceReplayState.m_replayPipeline->requestRiff( {
                                        {
                                            m_jamSliceSketch->m_slice->m_jamID,
                                            m_jamSliceSketch->m_slice->m_ids[m_jamSliceReplayState.m_riffPrefetchIndex]
                                        }, defaultPermutation } );

                                    m_jamSliceReplayState.m_riffPrefetchIndex++;
                                }

                                if ( m_jamSliceReplayState.m_currentRiffIndex + 1 >= m_jamSliceSketch->m_slice->m_ids.size() )
                                {
                                    m_jamSliceReplayState.m_state = JamSliceReplayState::State::Idle;
                                }
                            }
                        }
                    });
                }

                ImGui::EndMenu();
            }
        });


        registerMainMenuEntry( 20, "EXPORT", [this]()
        {
//             ImGui::Separator();
//             if ( ImGui::MenuItem( "Configure ..." ) )
//             {
//             }
        });

        registerMainMenuEntry( 30, "MIDI", [this]()
        {
            for ( const auto& device : m_midiInputDevices )
            {
                if ( ImGui::MenuItem( device.getName().c_str(), nullptr, false ) )
                {
                }
            }
            /*
            auto* midiInput = m_mdMidi->getInputControl();
            if ( midiInput != nullptr && !m_midiInputDevices.empty() )
            {
                uint32_t openedIndex;
                const bool hasOpenPort = midiInput->getOpenPortIndex( openedIndex );

                for ( uint32_t inpIdx = 0; inpIdx < (uint32_t)m_midiInputPortNames.size(); inpIdx++ )
                {
                    const bool thisPortIsOpen = hasOpenPort && (openedIndex == inpIdx);

                    if ( ImGui::MenuItem( m_midiInputPortNames[inpIdx].c_str(), nullptr, thisPortIsOpen ) )
                    {
                        if ( thisPortIsOpen )
                            midiInput->closeInputPort();
                        else
                            midiInput->openInputPort( inpIdx );
                    }
                }
            }
            else
            {
                ImGui::MenuItem( "Unavailable", nullptr, nullptr, false );
            }
            */
        });
    }

protected:


    std::vector< app::module::MidiDevice >  m_midiInputDevices;


    // discord bot & streaming panel 
    std::unique_ptr< discord::BotWithUI >   m_discordBotUI;

#if OURO_FEATURE_VST24
    // VST playground
    std::unique_ptr< effect::EffectStack >  m_effectStack;
#endif // OURO_FEATURE_VST24
    
    std::unique_ptr< vx::Vibes >            m_vibes;


    base::EventListenerID                   m_eventListenerRiffChange;
    base::EventListenerID                   m_eventListenerOpComplete;


    mix::StemDataProcessor                  m_stemDataProcessor;


// riff playback management #HDD build into common module?
protected:

    using RiffPipeline          = std::unique_ptr< endlesss::toolkit::Pipeline >;
    using SyncAndPlaybackQueue  = mcc::ReaderWriterQueue< endlesss::types::RiffCouchID >;

    // take the completed IDs posted back from the worker thread and prune them from
    // our list of 'in flight' tasks
    void synchroniseRiffWork()
    {
        endlesss::types::RiffCouchID completedRiff;
        while ( m_syncAndPlaybackCompletions.try_dequeue( completedRiff ) )
        {
            m_syncAndPlaybackInFlight.erase( completedRiff );
        }
        while ( m_riffsDequedByMixer.try_dequeue( completedRiff ) )
        {
            m_riffsQueuedForPlayback.erase( completedRiff );
        }
    }

    RiffPipeline                    m_riffPipeline;

    SyncAndPlaybackQueue            m_syncAndPlaybackQueue;         // riffs to fetch & play - written to by main thread, read from worker
    SyncAndPlaybackQueue            m_syncAndPlaybackCompletions;   // riffs that have been fetched & played - written to by worker, read by main thread
    endlesss::types::RiffCouchIDSet m_syncAndPlaybackInFlight;      // main thread list of work submitted to worker

    SyncAndPlaybackQueue            m_riffsDequedByMixer;
    endlesss::types::RiffCouchIDSet m_riffsQueuedForPlayback;
    endlesss::live::RiffPtr         m_nowPlayingRiff;
    std::atomic_bool                m_riffPipelineClearInProgress = false;


    void requestRiffPlayback( const endlesss::types::RiffIdentity& riffIdent, const endlesss::types::RiffPlaybackPermutation& playback )
    {
        const auto& riffCouchID = riffIdent.getRiffID();

        blog::app( "requestRiffPlayback: {}", riffCouchID );

        m_riffsQueuedForPlayback.emplace( riffCouchID );        // stash it in the "queued but not playing yet" list; removed from 
                                                                // when the mixer eventually gets to playing it

        m_syncAndPlaybackInFlight.emplace( riffCouchID );       // log that we will be asynchronously fetching this

        m_riffPipeline->requestRiff( { riffIdent, playback } ); // kick the request to the pipeline
    }


    void handleNewRiffPlaying( const base::IEvent& eventPtr )
    {
        ABSL_ASSERT( eventPtr.getID() == events::MixerRiffChange::ID );

        const events::MixerRiffChange* riffChangeEvent = dynamic_cast<const events::MixerRiffChange*>( &eventPtr );
        ABSL_ASSERT( riffChangeEvent != nullptr );

        m_nowPlayingRiff = riffChangeEvent->m_riff;

        // might be a empty riff, only track actual riffs
        if ( riffChangeEvent->m_riff != nullptr )
            m_riffsDequedByMixer.emplace( m_nowPlayingRiff->m_riffData.riff.couchID );
    }

    void handleOperationComplete( const base::IEvent& eventPtr )
    {
        ABSL_ASSERT( eventPtr.getID() == events::OperationComplete::ID );

        const events::OperationComplete* opCompleteEvent = dynamic_cast<const events::OperationComplete*>(&eventPtr);
        ABSL_ASSERT( opCompleteEvent != nullptr );

        m_permutationOperationImGuiMap.remove( opCompleteEvent->m_id );
    }

    endlesss::types::RiffPlaybackAbstraction    m_riffPlaybackAbstraction;

    base::BiMap< base::OperationID, ImGuiID >   m_permutationOperationImGuiMap;

protected:
    endlesss::types::JamCouchID     m_currentViewedJam;
    std::string                     m_currentViewedJamName;

// data and callbacks used to react to changes from the warehouse
protected:

    void handleWarehouseWorkUpdate( const bool tasksRunning, const std::string& currentTask )
    {
        m_warehouseWorkUnderway = tasksRunning;
        m_warehouseWorkState    = currentTask;
    }

    void handleWarehouseContentsReport( const endlesss::toolkit::Warehouse::ContentsReport& report )
    {
        // this data is presented by the UI so for safety at the moment we just do a big chunky lock around it
        // before wiping it out and rewriting
        std::scoped_lock<std::mutex> reportLock( m_warehouseContentsReportMutex );

        m_warehouseContentsReport = report;
        m_warehouseContentsReportJamIDs.clear();
        m_warehouseContentsReportJamTitles.clear();
        m_warehouseContentsReportJamInFlux.clear();
        m_warehouseContentsReportJamInFluxSet.clear();

        endlesss::cache::Jams::Data jamData;

        for ( auto jIdx = 0; jIdx < m_warehouseContentsReport.m_jamCouchIDs.size(); jIdx++ )
        {
            const auto& jamCID = m_warehouseContentsReport.m_jamCouchIDs[jIdx];

            m_warehouseContentsReportJamIDs.emplace( jamCID );

            if ( m_jamLibrary.loadDataForDatabaseID( jamCID, jamData ) )
                m_warehouseContentsReportJamTitles.emplace_back( jamData.m_displayName );
            else
                m_warehouseContentsReportJamTitles.emplace_back( "[ Unknown ID ]" );

            if ( m_warehouseContentsReport.m_unpopulatedRiffs[jIdx] > 0 ||
                 m_warehouseContentsReport.m_unpopulatedStems[jIdx] > 0 )
            {
                m_warehouseContentsReportJamInFluxSet.emplace( jamCID );
                m_warehouseContentsReportJamInFlux.push_back( true );
            }
            else
            {
                m_warehouseContentsReportJamInFlux.push_back( false );
            }
        }

        generateWarehouseContentsSortOrder();
    }

    enum class WarehouseContentsSortMode
    {
        ByJoinTime,
        ByName,
    };

    void generateWarehouseContentsSortOrder()
    {
        m_warehouseContentsSortedIndices.clear();
        for ( auto sortIndex = 0; sortIndex < m_warehouseContentsReportJamTitles.size(); sortIndex++ )
            m_warehouseContentsSortedIndices.emplace_back( sortIndex );

        if ( m_warehouseContentsSortMode == WarehouseContentsSortMode::ByName )
        {
            std::sort( m_warehouseContentsSortedIndices.begin(), m_warehouseContentsSortedIndices.end(),
                [&]( const std::size_t lhsIdx, const std::size_t rhsIdx ) -> bool
                {
                    return m_warehouseContentsReportJamTitles[lhsIdx] < 
                           m_warehouseContentsReportJamTitles[rhsIdx];
                });
        }
    }

    std::string                                     m_warehouseWorkState;
    bool                                            m_warehouseWorkUnderway = false;

    std::mutex                                      m_warehouseContentsReportMutex;
    endlesss::toolkit::Warehouse::ContentsReport    m_warehouseContentsReport;
    endlesss::types::JamCouchIDSet                  m_warehouseContentsReportJamIDs;
    std::vector< std::string >                      m_warehouseContentsReportJamTitles;
    std::vector< bool >                             m_warehouseContentsReportJamInFlux;
    endlesss::types::JamCouchIDSet                  m_warehouseContentsReportJamInFluxSet;      // any jams that have unfetched data
    WarehouseContentsSortMode                       m_warehouseContentsSortMode                 = WarehouseContentsSortMode::ByName;
    std::vector< std::size_t >                      m_warehouseContentsSortedIndices;


    struct WarehouseJamBrowserBehaviour : public ux::UniversalJamBrowserBehaviour
    {
        // local copy of warehouse lookup to avoid threading drama
        endlesss::types::JamCouchIDSet      m_warehouseJamIDs;
    };



protected:

    struct JamSliceReplayState
    {
        enum class State
        {
            Idle,
            Warmstart,
            Primed,
            Sending,
        }                   m_state = State::Idle;

        std::size_t         m_riffPrefetchIndex = 0;

        double              m_jamSequenceLengthSeconds = 0;
        double              m_replaySequenceLengthSeconds = 0;

        std::size_t         m_currentRiffIndex = 0;
        double              m_currentRiffDurationInSeconds = 0;
        spacetime::Moment   m_currentRiffSentAt;

        RiffPipeline        m_replayPipeline;

        mcc::ReaderWriterQueue< endlesss::live::RiffPtr >
                            m_riffsToSend;
    };
    JamSliceReplayState     m_jamSliceReplayState;



public:

    bool isJamBeingSynced( const endlesss::types::JamCouchID& jamID )
    {
        return m_warehouseContentsReportJamInFluxSet.contains( jamID );
    }


// 
protected:

    gfx::Sketchbook     m_sketchbook;

    void updateJamViewWidth( const float width )
    {
        const int32_t iWidth = (int32_t)std::ceil(width);

        if ( m_jamViewWidthCommitCount < 0 )
        {
            m_jamViewWidth              = iWidth;
            m_jamViewWidthToCommit      = iWidth;
            m_jamViewWidthCommitCount   = 0;
        }
        else
        {
            if ( m_jamViewWidthToCommit == iWidth &&
                 m_jamViewWidth != iWidth )
            {
                m_jamViewWidthCommitCount++;
                if ( m_jamViewWidthCommitCount > 60 * 2 )
                {
                    blog::app( "setting new jam view width = {}", iWidth );
                    m_jamViewWidth = iWidth;
                    m_jamViewWidthCommitCount = 0;

                    notifyForRenderUpdate();
                }
            }

            m_jamViewWidthToCommit = iWidth;
        }
    }

    std::mutex                              m_jamSliceMapLock;

    // which riff the mouse is currently hovered over in the active jam slice; or -1 if not valid
    int32_t                                 m_jamSliceHoveredRiffIndex = -1;

    // hax; alt-clicking lets us operate on a range of riffs (ie for lining up a big chunk of the jam) - if this is >=0 this was the last alt-clicked riff
    int32_t                                 m_jamSliceRangeClick = -1;

    int32_t                                 m_jamViewWidth = 0;
    int32_t                                 m_jamViewWidthToCommit = 0;
    int32_t                                 m_jamViewWidthCommitCount = -1;

    static constexpr int32_t                cJamBitmapGridCell  = 16;

    using UserHashFMap = absl::flat_hash_map< uint64_t, float >;


    JamVisualisation                        m_jamVisualisation;


    struct JamSliceSketch
    {
        using RiffToBitmapOffsetMap     = absl::flat_hash_map< endlesss::types::RiffCouchID, ImVec2 >;
        using CellIndexToRiffMap        = absl::flat_hash_map< uint64_t, endlesss::types::RiffCouchID >;
        using CellIndexToSliceIndex     = absl::flat_hash_map< uint64_t, int32_t >;
        using LinearRiffOrder           = std::vector< endlesss::types::RiffCouchID >;

        using WarehouseJamSlicePtr      = endlesss::toolkit::Warehouse::JamSlicePtr;


        WarehouseJamSlicePtr                m_slice;

        RiffToBitmapOffsetMap               m_riffToBitmapOffset;
        CellIndexToRiffMap                  m_cellIndexToRiff;
        CellIndexToSliceIndex               m_cellIndexToSliceIndex;
        LinearRiffOrder                     m_riffOrderLinear;

        std::vector< float >                m_labelY;
        std::vector< std::string >          m_labelText;

        UserHashFMap                        m_jamViewRenderUserHashFMap;

        float                               m_currentScrollY = 0;
        bool                                m_syncToUI = false;

        std::vector< gfx::SketchUploadPtr > m_textures;

        void prepare( endlesss::toolkit::Warehouse::JamSlicePtr&& slicePtr )
        {
            m_slice = std::move( slicePtr );
        }

        void raster( gfx::Sketchbook& sketchbook, const JamVisualisation& jamVis, const int32_t viewWidth )
        {
            ABSL_ASSERT( viewWidth > 0 );   // BUG track down why viewWidth coming in negative

            m_textures.clear();
            m_jamViewRenderUserHashFMap.clear();

            const endlesss::toolkit::Warehouse::JamSlice& slice = *m_slice;
            const int32_t totalRiffs = (int32_t)slice.m_ids.size();


            m_riffToBitmapOffset.clear();
            m_riffToBitmapOffset.reserve( totalRiffs );

            m_cellIndexToRiff.clear();
            m_cellIndexToRiff.reserve( totalRiffs );

            m_cellIndexToSliceIndex.clear();
            m_cellIndexToSliceIndex.reserve( totalRiffs );

            m_riffOrderLinear.clear();
            m_riffOrderLinear.reserve( totalRiffs );

            m_labelY.clear();
            m_labelText.clear();



            std::array< bool,     JamVisualisation::NameHighlightCount > nameHighlightOn;
            std::array< uint64_t, JamVisualisation::NameHighlightCount > nameHighlightHashes;
            std::array< uint32_t, JamVisualisation::NameHighlightCount > nameHighlightColourU32;
            for ( auto nH = 0; nH < JamVisualisation::NameHighlightCount; nH++ )
            {
                nameHighlightOn[nH]             = !jamVis.m_nameHighlighting[nH].m_name.empty();
                if ( nameHighlightOn[nH] )
                {
                    nameHighlightHashes[nH]     = absl::Hash< std::string >{}( jamVis.m_nameHighlighting[nH].m_name );
                    nameHighlightColourU32[nH]  = ImGui::ColorConvertFloat4ToU32_BGRA_Flip( jamVis.m_nameHighlighting[nH].m_colour );
                }
            }


            uint32_t bgraUniformColourU32 = ImGui::ColorConvertFloat4ToU32_BGRA_Flip( ImVec4(jamVis.m_uniformColour) );



            int32_t cellColumns   = std::max( 16, (int32_t)std::floor( (viewWidth - cJamBitmapGridCell) / (float)cJamBitmapGridCell ) );
            
            int32_t pageHeight = 1024;
            int32_t cellsPerPage = (int32_t)std::floor( (pageHeight - cJamBitmapGridCell) / (float)cJamBitmapGridCell );

            gfx::DimensionsPow2 sketchPageDim( viewWidth, pageHeight );

            int32_t cellX = 0;
            int32_t cellY = 0;
            int32_t fullCellY = 0;

            gfx::SketchBufferPtr activeSketch = sketchbook.getBuffer( sketchPageDim );

            const auto commitCurrentPage = [&]()
            {
                // extents are the current cell Y offset, +1 because cellY is a top-left coordinate so we're ensuring the whole
                // terminating row gets included in the texture
                gfx::Dimensions extents( viewWidth, (cellY + 1) * cJamBitmapGridCell );

                activeSketch->setExtents( extents );
                m_textures.emplace_back( sketchbook.scheduleBufferUploadToGPU( std::move( activeSketch ) ) );
            };

            const auto incrementCellY = [&]()
            {
                if ( cellY + 1 >= cellsPerPage )
                {
                    commitCurrentPage();

                    activeSketch = sketchbook.getBuffer( sketchPageDim );
                    cellY = 0;
                }
                else
                {
                    cellY++;
                }

                fullCellY++;
            };

            const auto addLineBreak = [&]( std::string label )
            {
                // only increment twice if the line feed hadn't just happened
                if ( cellX != 0 )
                    incrementCellY();

                m_labelY.emplace_back( (float)(fullCellY * cJamBitmapGridCell) );
                m_labelText.emplace_back( std::move( label ) );

                cellX = 0;
                incrementCellY();
            };


            float    lastBPM = 0;
            uint32_t lastRoot = 0;
            uint32_t lastScale = 0;
            uint64_t lastUserHash = 0;
            uint8_t  lastDay = 0;

            float    runningColourV = 0;


            for ( auto riffI = 0; riffI < totalRiffs; riffI++ )
            {
                {
                    const float    riffBPM   = slice.m_bpms[riffI];
                    const uint32_t riffRoot  = slice.m_roots[riffI];
                    const uint32_t riffScale = slice.m_scales[riffI];
                    const auto     riffDay   = spacetime::getDayIndex( slice.m_timestamps[riffI] );

                    switch ( jamVis.m_lineBreakOn )
                    {
                        default:
                        case JamVisualisation::LineBreakOn::Never:
                            break;

                        case JamVisualisation::LineBreakOn::ChangedBPM:
                        {
                            if ( riffI == 0 ||
                                !base::floatAlmostEqualRelative( lastBPM, riffBPM, 0.001f ) )
                            {
                                addLineBreak( fmt::format( "-=> {} BPM", riffBPM ) );
                            }
                        }
                        break;

                        case JamVisualisation::LineBreakOn::ChangedScaleOrRoot:
                        {
                            if ( riffI == 0 ||
                                lastRoot != riffRoot ||
                                lastScale != riffScale )
                            {
                                addLineBreak( fmt::format( "-=> {} ({})",
                                    endlesss::constants::cRootNames[riffRoot],
                                    endlesss::constants::cScaleNames[riffScale] ) );
                            }
                        }
                        break;

                        case JamVisualisation::LineBreakOn::TimePassing:
                        {
                            if ( riffI == 0 ||
                                lastDay != riffDay )
                            {
                                addLineBreak( spacetime::datestampStringFromUnix( slice.m_timestamps[riffI] ) );
                            }
                        }
                        break;
                    }

                    lastBPM = riffBPM;
                    lastRoot = riffRoot;
                    lastScale = riffScale;
                    lastDay = riffDay;
                }

                bool addRiffGap = false;
                switch ( jamVis.m_riffGapOn )
                {
                    default:
                    case JamVisualisation::RiffGapOn::Never:
                        break;

                    case JamVisualisation::RiffGapOn::ChangedBPM:
                        addRiffGap = (riffI > 0 && !base::floatAlmostEqualRelative( slice.m_bpms[riffI - 1], slice.m_bpms[riffI], 0.001f ) );
                        break;
                    case JamVisualisation::RiffGapOn::ChangedScaleOrRoot:
                        addRiffGap = (riffI > 0 && (slice.m_roots[riffI - 1] != slice.m_roots[riffI]
                                                ||  slice.m_scales[riffI - 1] != slice.m_scales[riffI]));
                        break;
                    case JamVisualisation::RiffGapOn::TimePassing:
                        addRiffGap = (slice.m_deltaSeconds[riffI] > 60 * 60); // hardwired to an hour at the moment
                        break;
                }
                if ( addRiffGap && 
                     cellX > 0 )    // don't indent if we're already at the start of a row
                {
                    cellX++;
                    if ( cellX >= cellColumns )
                    {
                        cellX = 0;
                        incrementCellY();
                    }
                }


                const uint64_t cellIndex = (uint64_t)cellX | ((uint64_t)fullCellY << 32);
                const uint64_t userHash = slice.m_userhash[riffI];

                float colourT = 0.0f;

                switch ( jamVis.m_colourStyle )
                {
                    case JamVisualisation::ColourStyle::Uniform:
                        // override later
                        break;

                    case JamVisualisation::ColourStyle::UserIdentity:
                    {
                        if ( m_jamViewRenderUserHashFMap.contains( userHash ) )
                        {
                            colourT = m_jamViewRenderUserHashFMap.at( userHash );
                        }
                        else
                        {
                            m_jamViewRenderUserHashFMap.emplace( userHash, runningColourV );
                            runningColourV += 0.147f;
                            colourT = runningColourV;
                        }
                    }
                    break;

                    case JamVisualisation::ColourStyle::UserChangeCycle:
                    {
                        if ( riffI > 0 && userHash != lastUserHash )
                            runningColourV += 0.147f;

                        colourT = runningColourV;
                    }
                    break;

                    case JamVisualisation::ColourStyle::UserChangeRate:
                    {
                        if ( riffI > 0 && userHash != lastUserHash )
                            runningColourV = std::clamp( runningColourV + 0.15f, 0.0f, 0.999f );
                        else
                            runningColourV *= jamVis.m_changeRateDecay;

                        colourT = runningColourV;
                    }
                    break;

                    case JamVisualisation::ColourStyle::StemChurn:
                    {
                        const float shiftRate = (float)std::clamp(
                            jamVis.m_activityTimeSec - slice.m_deltaSeconds[riffI],
                            0.0f,
                            jamVis.m_activityTimeSec ) / jamVis.m_activityTimeSec;

                        colourT = (0.1f + (shiftRate * shiftRate));
                    }
                    break;

                    case JamVisualisation::ColourStyle::StemTimestamp:
                    {
                        const auto stemTp = slice.m_timestamps[riffI];

                        auto dp = date::floor<date::days>( stemTp );
                        auto ymd = date::year_month_day{ dp };
                        auto time = date::make_time( std::chrono::duration_cast<std::chrono::milliseconds>(stemTp - dp) );

                        colourT = (float)time.hours().count() / 24.0f;
                    }
                    break;

                    case JamVisualisation::ColourStyle::Scale:
                    {
                        colourT = ((float)slice.m_scales[riffI] / 17.0f);
                    }
                    break;

                    case JamVisualisation::ColourStyle::Root:
                    {
                        colourT = ((float)slice.m_roots[riffI] / 12.0f);
                    }
                    break;

                    case JamVisualisation::ColourStyle::BPM:
                    {
                        const float shiftRate = (float)std::clamp(
                            slice.m_bpms[riffI] - jamVis.m_bpmMinimum,
                            0.0f,
                            jamVis.m_bpmMaximum ) / (jamVis.m_bpmMaximum - jamVis.m_bpmMinimum);

                        colourT = shiftRate;
                    }
                    break;
                }

                // sample from the gradient, or override with a uniform single manual choice
                uint32_t cellColour = jamVis.colourSampleT( colourT );
                if ( jamVis.m_colourStyle == JamVisualisation::ColourStyle::Uniform )
                    cellColour = bgraUniformColourU32;


                lastUserHash = userHash;



                const auto cellRiffCouchID = slice.m_ids[riffI];
                m_cellIndexToRiff.try_emplace( cellIndex, cellRiffCouchID );
                m_cellIndexToSliceIndex.try_emplace( cellIndex, riffI );
                m_riffOrderLinear.emplace_back( cellRiffCouchID );


                const int32_t cellPixelX = cellX * cJamBitmapGridCell;
                const int32_t cellPixelY = cellY * cJamBitmapGridCell;
                const int32_t cellPixelFullY = fullCellY * cJamBitmapGridCell;

                m_riffToBitmapOffset.try_emplace( cellRiffCouchID, ImVec2{ (float)cellPixelX, (float)cellPixelFullY } );



                bool userHighlightApply = false;
                uint32_t userHighlightColour = 0;
                for ( auto nH = 0; nH < JamVisualisation::NameHighlightCount; nH++ )
                {
                    if ( nameHighlightOn[nH] &&
                         slice.m_userhash[riffI] == nameHighlightHashes[nH] )
                    {
                        userHighlightColour = nameHighlightColourU32[nH];
                        userHighlightApply  = true;
                        break;
                    }
                }

                base::U32Buffer& activeBuffer = activeSketch->get();

                for ( auto cellWriteY = 0; cellWriteY < cJamBitmapGridCell; cellWriteY++ )
                {
                    for ( auto cellWriteX = 0; cellWriteX < cJamBitmapGridCell; cellWriteX++ )
                    {
                        const bool edge0 = (cellWriteX == 0 ||
                            cellWriteY == 0 ||
                            cellWriteX == cJamBitmapGridCell - 1 ||
                            cellWriteY == cJamBitmapGridCell - 1);
                        const bool edge1 = (cellWriteX == 1 ||
                            cellWriteY == 1 ||
                            cellWriteX == cJamBitmapGridCell - 2 ||
                            cellWriteY == cJamBitmapGridCell - 2);

                        if ( edge0 )
                        {
                            if ( userHighlightApply )
                            {
                                activeBuffer(
                                    cellPixelX + cellWriteX,
                                    cellPixelY + cellWriteY ) = userHighlightColour;
                            }
                        }
                        else if ( edge1 )
                        {

                        }
                        else
                        {
                            activeBuffer(
                                cellPixelX + cellWriteX,
                                cellPixelY + cellWriteY ) = cellColour;
                        }
                    }
                }

                // move our cell target along, wrap at edges
                cellX++;
                if ( cellX >= cellColumns )
                {
                    cellX = 0;
                    incrementCellY();
                }
            }

            commitCurrentPage();

            m_syncToUI = true;
        }
    };
    using JamSliceSketchPtr = std::unique_ptr< JamSliceSketch >;

    enum class JamSliceRenderState
    {
        Invalidated,
        Rendering,
        Ready,
        PendingUpdate
    }                                           m_jamSliceRenderState = JamSliceRenderState::Invalidated;
    spacetime::Moment                           m_jamSliceRenderChangePendingTimer;

    endlesss::toolkit::Warehouse::JamSlicePtr   m_jamSlice;
    JamSliceSketchPtr                           m_jamSliceSketch;


    void clearJamSlice()
    {
        std::scoped_lock<std::mutex> sliceLock( m_jamSliceMapLock );

        m_jamSlice              = nullptr;
        m_jamSliceRenderState   = JamSliceRenderState::Invalidated;
        m_jamSliceSketch        = nullptr;
    }

    void newJamSliceGenerated(
        const endlesss::types::JamCouchID& jamCouchID,
        endlesss::toolkit::Warehouse::JamSlicePtr&& resultSlice )
    {
        std::scoped_lock<std::mutex> sliceLock( m_jamSliceMapLock );

        m_jamSlice              = std::move( resultSlice );
        m_jamSliceRenderState   = JamSliceRenderState::Invalidated;
        m_jamSliceSketch        = nullptr;

        m_jamSliceRenderChangePendingTimer.restart();
    }

    void notifyForRenderUpdate()
    {
        if ( m_jamSliceRenderState == JamSliceRenderState::Invalidated )
            return;

        m_jamSliceRenderChangePendingTimer.restart();
        m_jamSliceRenderState = JamSliceRenderState::PendingUpdate;
    }

    bool isRenderUpdatePending()
    {
        return ( m_jamSliceRenderState == JamSliceRenderState::PendingUpdate );
    }

    void updateJamSliceRendering()
    {
        std::scoped_lock<std::mutex> sliceLock( m_jamSliceMapLock );

        switch ( m_jamSliceRenderState )
        {
            case JamSliceRenderState::Invalidated:
            {
                if ( m_jamSlice != nullptr )
                {
                    m_jamSliceRenderState = JamSliceRenderState::Rendering;
                }
            }
            break;

            case JamSliceRenderState::Rendering:
            {
                m_jamSliceSketch = std::make_unique<JamSliceSketch>();
                m_jamSliceSketch->prepare( std::move( m_jamSlice ) );
                m_jamSliceSketch->raster( m_sketchbook, m_jamVisualisation, m_jamViewWidth );

                m_jamSliceRenderState = JamSliceRenderState::Ready;
            }
            break;

            default:
            case JamSliceRenderState::Ready:
                break;

            case JamSliceRenderState::PendingUpdate:
            {
                if ( m_jamSliceRenderChangePendingTimer.deltaMs().count() >= 1000 )
                {
                    m_jamSliceSketch->raster( m_sketchbook, m_jamVisualisation, m_jamViewWidth );
                    m_jamSliceRenderState = JamSliceRenderState::Ready;
                }
            }
            break;
        }
    }

protected:

    net::bond::RiffPushClient   m_rpClient;

};




// ---------------------------------------------------------------------------------------------------------------------
int LoreApp::EntrypointOuro()
{
    // create a lifetime-tracked provider token to pass to systems that want access to Riff-fetching abilities we provide
    // the app instance will outlive all those systems; this slightly convoluted process is there to double-check that assertion
    endlesss::services::RiffFetchInstance riffFetchService( this );
    endlesss::services::RiffFetchProvider riffFetchProvider = riffFetchService.makeBound();


    // create warehouse instance to manage ambient downloading
    endlesss::toolkit::Warehouse warehouse( m_storagePaths.value(), m_apiNetworkConfiguration.value() );
    warehouse.syncFromJamCache( m_jamLibrary );
    warehouse.setCallbackWorkReport( std::bind( &LoreApp::handleWarehouseWorkUpdate, this, stdp::_1, stdp::_2 ) );
    warehouse.setCallbackContentsReport( std::bind( &LoreApp::handleWarehouseContentsReport, this, stdp::_1 ) );

    WarehouseJamBrowserBehaviour warehouseJamBrowser;
    warehouseJamBrowser.fnIsDisabled = [&warehouseJamBrowser]( const endlesss::types::JamCouchID& newJamCID )
    {
        return warehouseJamBrowser.m_warehouseJamIDs.contains( newJamCID );
    };
    warehouseJamBrowser.fnOnSelected = [&warehouse]( const endlesss::types::JamCouchID& newJamCID )
    {
        warehouse.addOrUpdateJamSnapshot( newJamCID );
    };


    // fetch any customised stem export spec
    const auto exportLoadResult = config::load( *this, m_configExportOutput );


    // add status bar section to report warehouse activity
    const auto sbbWarehouseID = registerStatusBarBlock( app::CoreGUI::StatusBarAlignment::Left, 500.0f, [this]()
    {
        // worker thread control / status display
        {
            const auto currentLineHeight = ImGui::GetTextLineHeight();

            // show spinner if we're working, and the current reported work state
            ImGui::Spinner( "##syncing", m_warehouseWorkUnderway, currentLineHeight * 0.4f, 3.0f, 1.5f, ImGui::GetColorU32( ImGuiCol_Text ) );
            ImGui::Separator();

            ImGui::TextUnformatted( m_warehouseWorkState.c_str() );
        }
    });



    // default to viewing logged-in user in the jam view highlight
    if ( m_apiNetworkConfiguration->hasValidEndlesssAuth() )
    {
        m_jamVisualisation.m_nameHighlighting[0].m_name     = m_apiNetworkConfiguration->auth().user_id;
        m_jamVisualisation.m_nameHighlighting[0].m_colour   = ImVec4( 1.0f, 0.95f, 0.8f, 1.0f );
    }

    m_vibes = std::make_unique<vx::Vibes>();
    if ( const auto vibeStatus = m_vibes->initialize( *this ); !vibeStatus.ok() )
    {
        blog::error::app( FMTX( "Bad vibes : {}" ), vibeStatus.ToString() );
    }

    // examine our midi state, extract port names
    initMidi();


    // create and install the mixer engine
    mix::Preview mixPreview( m_mdAudio->getMaximumBufferSize(), m_mdAudio->getSampleRate(), m_appEventBusClient.value() );
    m_mdAudio->blockUntil( m_mdAudio->installMixer( &mixPreview ) );


    m_eventListenerRiffChange       = m_appEventBus->addListener( events::MixerRiffChange::ID, std::bind( &LoreApp::handleNewRiffPlaying, this, stdp::_1 ) );
    m_eventListenerOpComplete       = m_appEventBus->addListener( events::OperationComplete::ID, std::bind( &LoreApp::handleOperationComplete, this, stdp::_1 ) );


    checkedCoreCall( "add stem listener", [this] { return m_stemDataProcessor.connect( m_appEventBus ); } );


#if OURO_FEATURE_VST24
    // VSTs for audio engine
    m_effectStack = std::make_unique<effect::EffectStack>( m_mdAudio.get(), mixPreview.getTimeInfoPtr(), "preview" );
    m_effectStack->load( m_appConfigPath );
#endif // OURO_FEATURE_VST24


    m_riffPipeline = std::make_unique< endlesss::toolkit::Pipeline >(
        riffFetchProvider,
        m_configPerf.liveRiffInstancePoolSize,
        [&warehouse, this]( const endlesss::types::RiffIdentity& request, endlesss::types::RiffComplete& result) -> bool
        {
            // most requests can be serviced direct from the DB
            if ( warehouse.fetchSingleRiffByID( request.getRiffID(), result ) )
                return true;

            // assuming we have Endlesss auth, go hunting
            if ( m_apiNetworkConfiguration->hasValidEndlesssAuth() )
                return endlesss::toolkit::Pipeline::defaultNetworkResolver( m_apiNetworkConfiguration.value(), request, result );

            return false;
        },
        [&mixPreview, this]( const endlesss::types::RiffIdentity& request, endlesss::live::RiffPtr& loadedRiff, const endlesss::types::RiffPlaybackPermutationOpt& playbackPermutationOpt )
        {
            // if the provided riff is valid, hand it over to the mixer to enqueue for playing
            if ( loadedRiff )
            {
                mixPreview.enqueueRiff( loadedRiff );

                // TODO should we actually bind this in with enqueueRiff ?
                if ( playbackPermutationOpt.has_value() )
                {
                    mixPreview.enqueuePermutation( playbackPermutationOpt.value() );
                }
            }
            // if null, this may be either a failure to resolve or part of a request cancellation
            // .. in which case we manually push it on the list that it *would* have been put on by the mixer eventually
            else
                m_riffsDequedByMixer.enqueue( request.getRiffID() );

            // report that we're done with this one
            m_syncAndPlaybackCompletions.enqueue( request.getRiffID() );
        },
        [&mixPreview, this]()
        {
            mixPreview.stop();
            m_riffPipelineClearInProgress = false;
        });

    m_jamSliceReplayState.m_replayPipeline = std::make_unique< endlesss::toolkit::Pipeline >(
        riffFetchProvider,
        4,
        [&warehouse, this]( const endlesss::types::RiffIdentity& request, endlesss::types::RiffComplete& result ) -> bool
        {
            // most requests can be serviced direct from the DB
            if ( warehouse.fetchSingleRiffByID( request.getRiffID(), result ) )
                return true;

            // assuming we have Endlesss auth, go hunting
            if ( m_apiNetworkConfiguration->hasValidEndlesssAuth() )
                return endlesss::toolkit::Pipeline::defaultNetworkResolver( m_apiNetworkConfiguration.value(), request, result );

            return false;
        },
        [this]( const endlesss::types::RiffIdentity& request, endlesss::live::RiffPtr& loadedRiff, const endlesss::types::RiffPlaybackPermutationOpt& )
        {
            if ( loadedRiff )
                m_jamSliceReplayState.m_riffsToSend.enqueue( loadedRiff );
        },
        []()
        {
        });

    // UI core loop begins
    while ( beginInterfaceLayout( (app::CoreGUI::ViewportFlags)(
        app::CoreGUI::VF_WithDocking   |
        app::CoreGUI::VF_WithMainMenu  |
        app::CoreGUI::VF_WithStatusBar ) ) )
    {
        // process and blank out Exchange data ready to re-write it
        emitAndClearExchangeData();


        // run jam slice computation that needs to run on the main thread
        m_sketchbook.processPendingUploads();

        m_stemDataProcessor.update( GImGui->IO.DeltaTime, 0.5f );

        // tidy up any messages from our riff-sync background thread
        synchroniseRiffWork();

        ImGuiPerformanceTracker();

        {
            ImGui::Begin( "Audio" );

            // expose gain control
            {
                float gainF = m_mdAudio->getMasterGain() * 1000.0f;
                if ( ImGui::KnobFloat( "##mix_gain", 24.0f, &gainF, 0.0f, 1000.0f, 2000.0f ) )
                    m_mdAudio->setMasterGain( gainF * 0.001f );
            }
            ImGui::SameLine( 0, 8.0f );

            // button to toggle end-chain mute on audio engine (but leave processing, WAV output etc intact)
            const bool isMuted = m_mdAudio->isMuted();
            {
                const char* muteIcon = isMuted ? ICON_FA_VOLUME_OFF : ICON_FA_VOLUME_HIGH;

                {
                    ImGui::Scoped::ToggleButton bypassOn( isMuted );
                    if ( ImGui::Button( muteIcon, ImVec2( 48.0f, 48.0f ) ) )
                        m_mdAudio->toggleMute();
                }
                ImGui::CompactTooltip( "Mute final audio output\nThis does not affect streaming or disk-recording" );
            }
            ImGui::SameLine( 0, 8.0f );
            {
                ImGui::Scoped::ToggleButton isClearing( m_riffPipelineClearInProgress );
                if ( ImGui::Button( ICON_FA_BAN, ImVec2( 48.0f, 48.0f ) ) )
                {
                    m_riffPipelineClearInProgress = true;
                    m_riffPipeline->requestClear();
                }
                ImGui::CompactTooltip( "Panic stop all playback, buffering, pre-fetching, etc" );
            }

            ImGui::Spacing();
            ImGui::TextUnformatted( "Disk Recorders" );
            {
                ux::widget::DiskRecorder( *m_mdAudio, m_storagePaths->outputApp );

                auto* mixRecordable = mixPreview.getRecordable();
                if ( mixRecordable != nullptr )
                    ux::widget::DiskRecorder( *mixRecordable, m_storagePaths->outputApp );
            }

#if OURO_FEATURE_VST24
            ImGui::Spacing();
            ImGui::Spacing();

            m_effectStack->imgui( *this );
#endif // OURO_FEATURE_VST24

            ImGui::End();
        }



        // for rendering state from the current riff;
        // take a shared ptr copy here, just in case the riff is swapped out mid-tick
        endlesss::live::RiffPtr currentRiffPtr = m_nowPlayingRiff;
        const auto currentRiff = currentRiffPtr.get();

        // update the Exchange data block; this also serves as a way to push sanitized beat / energy / playback 
        // information around other parts of the app. this pulls data from a few different sources to try and 
        // give a solid, accurate overview of what is coming out of the audio engine at this instant
        {
            // take basic riff & stem data from the Riff instance
            endlesss::toolkit::Exchange::copyDetailsFromRiff( m_endlesssExchange, currentRiffPtr, m_currentViewedJamName.c_str() );

            if ( currentRiff != nullptr )
            {
                // copy in the current stem energy/pulse data that may have arrived from the mixer
                m_stemDataProcessor.copyToExchangeData( m_endlesssExchange );

                // compute the progression (bar/percentage through riff) of playback based on the current sample, embed in Exchange
                endlesss::live::RiffProgression playbackProgression;

                const auto timingData = currentRiff->getTimingDetails();
                timingData.ComputeProgressionAtSample(
                    (uint64_t)mixPreview.getTimeInfoPtr()->samplePos,
                    playbackProgression );

                endlesss::toolkit::Exchange::copyDetailsFromProgression( m_endlesssExchange, playbackProgression );

                // take a snapshot of the mixer layer gains and apply that to the exchange data so "stem gain" is 
                // more representative of what's coming out of the audio pipeline
                const auto currentMixPermutation = mixPreview.getCurrentPermutation();
                for ( std::size_t i = 0; i < currentMixPermutation.m_layerGainMultiplier.size(); i++ )
                    m_endlesssExchange.m_stemGain[i] *= currentMixPermutation.m_layerGainMultiplier[i];

                // copy in the scope data
                const dsp::Scope8::Result& scopeResult = m_mdAudio->getCurrentScopeResult();
                static_assert(dsp::Scope8::FrequencyBucketCount == endlesss::toolkit::Exchange::ScopeBucketCount, "fft bucket count mismatch");
                for ( std::size_t i = 0; i < endlesss::toolkit::Exchange::ScopeBucketCount; i++ )
                    m_endlesssExchange.m_scope[i] = scopeResult[i];

                // mark exchange as having a full complement of data
                m_endlesssExchange.m_dataflags |= endlesss::toolkit::Exchange::DataFlags_Playback;
                m_endlesssExchange.m_dataflags |= endlesss::toolkit::Exchange::DataFlags_Scope;
            }
        }


        m_vibes->doImGui( m_endlesssExchange, this );

        m_mdMidi->processMessages( []( const app::midi::Message& ){ } );

        m_discordBotUI->imgui( *this );


        {
            mixPreview.imgui();


            if ( ImGui::Begin( "Riff Details" ) )
            {

                struct TableFixedColumn
                {
                    constexpr TableFixedColumn( const char* title, const float width )
                        : m_title( title )
                        , m_width( width )
                    {}

                    const char* m_title;
                    const float m_width;
                };

                static constexpr std::array< TableFixedColumn, 12 > RiffViewTable{ {
                    { "",           15.0f  },
                    { "",           15.0f  },
                    { "",           10.0f  },
                    { "User",       140.0f },
                    { "Instr",      60.0f  },
                    { "Preset",     140.0f },
                    { "Gain",       45.0f  },
                    { "Speed",      45.0f  },
                    { "Rep",        30.0f  },
                    { "Length",     65.0f  },
                    { "Rate",       65.0f  },
                    { "Size/KB",    65.0f  }
                } };

                // based on the screen space we have available, figure out which columns we can draw
                int32_t visibleColumns = 0;
                float currentViewWidth = ImGui::GetContentRegionAvail().x;
                const std::size_t maxColumns = RiffViewTable.size();
                const float perColumnPadding = (GImGui->Style.CellPadding.x * 2.0f);
                for ( ;; )
                {
                    currentViewWidth -= RiffViewTable[visibleColumns].m_width + perColumnPadding;
                    if ( currentViewWidth <= 0.0f )
                        break;

                    // or run out of columns
                    visibleColumns++;
                    if ( visibleColumns >= maxColumns )
                        break;
                }

                ImGui::Dummy( ImVec2( 0.0f, 4.0f ) );
                ImGui::vx::StemBeats( "##stem_beat", m_endlesssExchange, 18.0f, false );
                ImGui::Dummy( ImVec2( 0.0f, 8.0f ) );

                if ( visibleColumns >= 3 && // only bother if there's actually enough space to make it worth while viewing
                    ImGui::BeginTable( "##stem_stack", visibleColumns, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg ) )
                {
                    ImGui::PushStyleColor( ImGuiCol_Text, ImGui::GetStyleColorVec4( ImGuiCol_ResizeGripHovered ) );
                    {
                        for ( int32_t cI = 0; cI < visibleColumns; cI++ )
                            ImGui::TableSetupColumn( RiffViewTable[cI].m_title, ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoSort, RiffViewTable[cI].m_width );
                    }
                    ImGui::TableNextRow( ImGuiTableRowFlags_Headers );

                    // render the table header ourselves so we can insert checkboxes to control the mute/solo columns
                    const bool anyChannelSolo = m_riffPlaybackAbstraction.query( endlesss::types::RiffPlaybackAbstraction::Query::AnySolo, -1 );
                    const bool anyChannelsMuted = m_riffPlaybackAbstraction.query( endlesss::types::RiffPlaybackAbstraction::Query::AnyMuted, -1 );
                    bool anyChannelsMutedCb = anyChannelsMuted;
                    for ( int32_t cI = 0; cI < visibleColumns; cI++ )
                    {
                        ImGui::TableSetColumnIndex( cI );
                        ImGui::PushID( cI );
                        if ( cI == 0 )
                        {
                            // show the checkbox for unmuting all muted channels if we have any muted 
                            // and they aren't muted because of a solo'ing
                            if ( anyChannelsMutedCb && !anyChannelSolo )
                            {
                                ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 0, 0 ) );
                                ImGui::Checkbox( "##unmute_check", &anyChannelsMutedCb );
                                ImGui::PopStyleVar();
                            }
                        }
                        else
                        {
                            ImGui::TableHeader( RiffViewTable[cI].m_title );
                        }
                        ImGui::PopID();
                    }
                    ImGui::PopStyleColor();

                    const auto currentPermutation = mixPreview.getCurrentPermutation();

                    // user chose to de-check the mute column, meaning we need to unmute everything
                    const bool unmuteAll = (anyChannelsMutedCb == false && anyChannelsMutedCb != anyChannelsMuted);


                    const auto muteButtonColour = ImGui::GetStyleColorVec4( ImGuiCol_NavHighlight );
                    const auto soloButtonColour = ImGui::GetStyleColorVec4( ImGuiCol_PlotHistogram );

                    const auto ImGuiPermutationButton = [this, &mixPreview](
                        const char* title,
                        const endlesss::types::RiffPlaybackAbstraction::Action action,
                        const endlesss::types::RiffPlaybackAbstraction::Query query,
                        const ImVec4& buttonColour,
                        const int32_t stemIndex,
                        bool forceOff )
                    {
                        const ImGuiID currentImGuiID = ImGui::GetID( title );

                        auto buttonState = m_riffPlaybackAbstraction.query( query, stemIndex ) ?
                            ImGui::Scoped::FluxButton::State::On :
                            ImGui::Scoped::FluxButton::State::Off;

                        if ( m_permutationOperationImGuiMap.hasValue( currentImGuiID ) )
                            buttonState = ImGui::Scoped::FluxButton::State::Flux;

                        ImGui::Scoped::FluxButton fluxButton( buttonState, buttonColour, ImVec4( 0, 0, 0, 1 ) );
                        if ( ImGui::Button( title ) || (forceOff && buttonState == ImGui::Scoped::FluxButton::State::On) )
                        {
                            if ( m_riffPlaybackAbstraction.action( action, stemIndex ) )
                            {
                                const auto newPermutation = m_riffPlaybackAbstraction.asPermutation();
                                const auto operationID = mixPreview.enqueuePermutation( newPermutation );
                                m_permutationOperationImGuiMap.add( operationID, currentImGuiID );
                            }
                        }
                    };

                    for ( auto sI = 0; sI < 8; sI++ )
                    {
                        ImGui::PushID( sI );

                        const endlesss::live::Stem* stem = currentRiff ? currentRiff->m_stemPtrs[sI] : nullptr;

                        // consider a stem "empty" if it has no data - a stem can have things like length and preset info
                        // as that comes from the database or backend .. but if it failed download or endlesss fucked up the 
                        // CDN upload then we get unplayable entries hanging around
                        const bool stemIsEmpty = (stem != nullptr && currentRiff != nullptr &&
                            (currentRiff->m_stemLengthInSamples[sI] == 0 ||
                                currentRiff->m_stemLengthInSec[sI] <= 0.0f));

                        if ( visibleColumns > 0 )
                        {
                            ImGui::TableNextColumn();

                            // mark any dead / damaged streams
                            if ( stemIsEmpty )
                            {
                                ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32( ImGuiCol_HeaderActive, 0.1f ) );
                                ImGui::PushStyleColor( ImGuiCol_Text, ImGui::GetColorU32( ImGuiCol_HeaderActive ) );
                            }

                            // MUTE toggle
                            ImGuiPermutationButton(
                                "M",
                                endlesss::types::RiffPlaybackAbstraction::Action::ToggleMute,
                                endlesss::types::RiffPlaybackAbstraction::Query::IsMuted,
                                muteButtonColour,
                                sI,
                                unmuteAll );
                        }
                        if ( visibleColumns > 1 )
                        {
                            ImGui::TableNextColumn();

                            // SOLO toggle
                            ImGuiPermutationButton(
                                "S",
                                endlesss::types::RiffPlaybackAbstraction::Action::ToggleSolo,
                                endlesss::types::RiffPlaybackAbstraction::Query::IsSolo,
                                soloButtonColour,
                                sI,
                                false );
                        }
                        if ( visibleColumns > 2 )
                        {
                            ImGui::TableNextColumn();

                            ImGui::VerticalProgress( "##gainBar",  currentPermutation.m_layerGainMultiplier[sI] );
                        }

                        // post up the stem data if we have it
                        if ( stem == nullptr )
                        {
                            for ( std::size_t cI = 3; cI < visibleColumns; cI++ )
                                ImGui::TableNextColumn(); ImGui::TextUnformatted( "" );
                        }
                        else
                        {
                            ABSL_ASSERT( currentRiff != nullptr );
                            const auto& riffDocument = currentRiff->m_riffData;

                            for ( std::size_t cI = 3; cI < visibleColumns; cI++ )
                            {
                                ImGui::TableNextColumn();
                                switch ( cI )
                                {
                                case 3:  ImGui::TextUnformatted( stem->m_data.user.c_str() );           break;
                                case 4:  ImGui::TextUnformatted( stem->m_data.getInstrumentName() );    break;
                                case 5:  ImGui::TextUnformatted( stem->m_data.preset.c_str() );         break;
                                case 6:  ImGui::Text( "%.2f", m_endlesssExchange.m_stemGain[sI] );      break;
                                case 7:  ImGui::Text( "%.2f", currentRiff->m_stemTimeScales[sI] );      break;
                                case 8:  ImGui::Text( "%ix", currentRiff->m_stemRepetitions[sI] );      break;
                                case 9:  ImGui::Text( "%.2fs", currentRiff->m_stemLengthInSec[sI] );    break;
                                case 10: ImGui::Text( "%i", stem->m_data.sampleRate );                  break;
                                case 11: ImGui::Text( "%i", stem->m_data.fileLengthBytes / 1024 );      break;
                                }
                            }
                        }

                        if ( stemIsEmpty )
                        {
                            ImGui::PopStyleColor();
                        }

                        ImGui::PopID();
                    }

                    ImGui::EndTable();
                    ImGui::Spacing();

                    {
                        const auto riffToolButtonSize = ImVec2( 180.0f, 0.0f );

                        ImGui::Scoped::ButtonTextAlignLeft riffToolButtons;
                        {
                            const bool disableButton = ( currentRiff == nullptr );
                            ImGui::BeginDisabledControls( disableButton );
                            if ( ImGui::Button( " " ICON_FA_BOOK " Add to Scrapbook ", riffToolButtonSize ) )
                            {

                            }
                            ImGui::EndDisabledControls( disableButton );
                        }
                        ImGui::SameLine( 0, 10.0f );
                        {
                            const bool disableButton = (m_rpClient.getState() != net::bond::BondState::Connected);
                            ImGui::BeginDisabledControls( disableButton );
                            if ( ImGui::Button( " " ICON_FA_CIRCLE_NODES " Push via BOND ", riffToolButtonSize ) )
                            {
                                m_rpClient.pushRiff( currentRiff->m_riffData, m_riffPlaybackAbstraction.asPermutation() );
                            }
                            ImGui::EndDisabledControls( disableButton );
                        }
                    }
                }
            }
            ImGui::End();
        }

        {
            ImGui::Begin( "Jam View" );

            ImGui::PushFont( m_mdFrontEnd->getFont( app::module::Frontend::FontChoice::FixedSmaller ) );

            const float jamViewWidth = ImGui::GetContentRegionAvail().x;

            // check to see if we need to rebuild the jam view on size change
            updateJamViewWidth( jamViewWidth );

            if ( ImGui::CollapsingHeader("Visualisation Options") )
            {
                const bool visOptionChanged = m_jamVisualisation.imgui();
                if ( visOptionChanged )
                {
                    notifyForRenderUpdate();
                }
            }
            ImGui::Spacing();

            ImGui::PushFont( m_mdFrontEnd->getFont( app::module::Frontend::FontChoice::MediumTitle ) );
            ImGui::TextUnformatted( m_currentViewedJamName.c_str() );
            ImGui::PopFont();


            updateJamSliceRendering();



            if ( ImGui::BeginChild( "##jam_browser" ) )
            {
                if ( isJamBeingSynced( m_currentViewedJam ) )
                {
                    ImGui::TextUnformatted( "Jam being synced ..." );
                }
                else
                {
                    ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 0, 0 ) );
                    std::scoped_lock<std::mutex> sliceLock( m_jamSliceMapLock );

                    const bool renderUpdatePending = isRenderUpdatePending();
                    const ImVec4 texturePageBlending = renderUpdatePending ? ImVec4( 1.0f, 1.0f, 1.0f, 0.5f ) : ImVec4( 1.0f, 1.0f, 1.0f, 1.0f );

                    if ( m_jamSliceRenderState == JamSliceRenderState::Ready || renderUpdatePending )
                    {
                        ImVec2 pos = ImGui::GetCursorScreenPos();
                        ImDrawList* draw_list = ImGui::GetWindowDrawList();
                        ImGuiIO& io = ImGui::GetIO();

                        gfx::GPUTask::ValidState textureState;

                        for ( const auto& texture : m_jamSliceSketch->m_textures )
                        {
                            if ( texture->getStateIfValid( textureState ) )
                            {
                                ImGui::Image(
                                    textureState.m_imTextureID,
                                    textureState.m_usageDimensionsVec2,
                                    ImVec2( 0, 0 ),
                                    textureState.m_usageUV,
                                    texturePageBlending );

                                const bool is_active = ImGui::IsItemActive();
                                const bool is_hovered = ImGui::IsItemHovered();
                                const auto mouseToCenterX = io.MousePos.x - pos.x;
                                const auto mouseToCenterY = io.MousePos.y - pos.y;

                                if ( is_hovered )
                                {
                                    const auto cellX = (int32_t)std::floor( mouseToCenterX / (float)cJamBitmapGridCell );
                                    const auto cellY = (int32_t)std::floor( mouseToCenterY / (float)cJamBitmapGridCell );

                                    const uint64_t cellIndex = (uint64_t)cellX | ((uint64_t)cellY << 32);

                                    // check and set which riff we are hovering over, if any
                                    const auto& indexHovered = m_jamSliceSketch->m_cellIndexToSliceIndex.find( cellIndex );
                                    if ( indexHovered != m_jamSliceSketch->m_cellIndexToSliceIndex.end() )
                                    {
                                        m_jamSliceHoveredRiffIndex = indexHovered->second;

                                        if ( ImGui::IsItemClicked() )
                                        {
                                            if ( ImGui::GetMergedModFlags() & ImGuiModFlags_Alt )
                                            {
                                                if ( m_jamSliceRangeClick < 0 )
                                                {
                                                    blog::app( "Selecting first riff in range [{}]", m_jamSliceHoveredRiffIndex );
                                                    m_jamSliceRangeClick = m_jamSliceHoveredRiffIndex;
                                                }
                                                else
                                                {
                                                    const int32_t direction = std::clamp( m_jamSliceHoveredRiffIndex - m_jamSliceRangeClick, -1, 1 );

                                                    int32_t currentRiffInRange = m_jamSliceRangeClick;
                                                    while ( currentRiffInRange != m_jamSliceHoveredRiffIndex )
                                                    {
                                                        const auto& riffCouchID = m_jamSliceSketch->m_slice->m_ids[currentRiffInRange];
                                                        requestRiffPlayback( { m_currentViewedJam, riffCouchID }, m_riffPlaybackAbstraction.asPermutation() );

                                                        currentRiffInRange += direction;
                                                    }

                                                    m_jamSliceRangeClick = -1;
                                                }
                                            }
                                            else
                                            {
                                                const auto& riffCouchID = m_jamSliceSketch->m_slice->m_ids[m_jamSliceHoveredRiffIndex];
                                                requestRiffPlayback( { m_currentViewedJam, riffCouchID }, m_riffPlaybackAbstraction.asPermutation() );
                                            }
                                        }
                                    }
                                }

                            }
                            else
                            {
                                const auto dummyBounds = texture->bounds();
                                ImGui::Dummy( ImVec2( (float)dummyBounds.width(), (float)dummyBounds.height() ) );
                            }
                        }

                        if ( m_jamSliceSketch->m_syncToUI )
                        {
                            ImGui::SetScrollY( m_jamSliceSketch->m_currentScrollY );
                            m_jamSliceSketch->m_syncToUI = false;
                        }
                        m_jamSliceSketch->m_currentScrollY = ImGui::GetScrollY();


                        for ( size_t lb = 0; lb < m_jamSliceSketch->m_labelY.size(); lb++ )
                        {
                            draw_list->AddText( ImVec2{ pos.x, pos.y + m_jamSliceSketch->m_labelY[lb] }, 0x80ffffff, m_jamSliceSketch->m_labelText[lb].c_str() );
                        }


                        const float jamGridCellHalf = (float)(cJamBitmapGridCell / 2);
                        const ImVec2 jamGridCell{ (float)cJamBitmapGridCell, (float)cJamBitmapGridCell };
                        const ImVec2 jamGridCellCenter{ jamGridCellHalf, jamGridCellHalf };
                        const ImU32 jamCellColourPlaying = ImGui::GetColorU32( ImGuiCol_Text );
                        const ImU32 jamCellColourEnqueue = ImGui::GetColorU32( ImGuiCol_ChildBg, 0.8f );
                        const ImU32 jamCellColourLoading = ImGui::GetPulseColour();

                        if ( currentRiff )
                        {
                            const auto& activeRectIt = m_jamSliceSketch->m_riffToBitmapOffset.find( currentRiff->m_riffData.riff.couchID );
                            if ( activeRectIt != m_jamSliceSketch->m_riffToBitmapOffset.end() )
                            {
                                const auto& riffRectXY = activeRectIt->second;
                                draw_list->AddNgonFilled( pos + riffRectXY + jamGridCellCenter, jamGridCellHalf * 0.85f, jamCellColourEnqueue, 3 );
                                draw_list->AddNgonFilled( pos + riffRectXY + jamGridCellCenter, jamGridCellHalf * 0.7f, jamCellColourPlaying, 3 );
                            }
                        }
                        if ( m_jamSliceRangeClick >= 0 )
                        {
                            const auto& riffCouchID = m_jamSliceSketch->m_slice->m_ids[m_jamSliceRangeClick];

                            const auto& activeRectIt = m_jamSliceSketch->m_riffToBitmapOffset.find( riffCouchID );
                            if ( activeRectIt != m_jamSliceSketch->m_riffToBitmapOffset.end() )
                            {
                                const auto& riffRectXY = activeRectIt->second;
                                draw_list->AddCircle( pos + riffRectXY + jamGridCellCenter, 8.0f, jamCellColourLoading, 8, 2.5f );
                            }
                        }

                        for ( const auto& riffEnqueue : m_riffsQueuedForPlayback )
                        {
                            const auto& activeRectIt = m_jamSliceSketch->m_riffToBitmapOffset.find( riffEnqueue );
                            if ( activeRectIt != m_jamSliceSketch->m_riffToBitmapOffset.end() )
                            {
                                const auto& riffRectXY = activeRectIt->second;
                                draw_list->AddNgonFilled( pos + riffRectXY + jamGridCellCenter, jamGridCellHalf * 0.85f, jamCellColourEnqueue, 3 );
                            }
                        }
                        for ( const auto& riffInFlight : m_syncAndPlaybackInFlight )
                        {
                            const auto& activeRectIt = m_jamSliceSketch->m_riffToBitmapOffset.find( riffInFlight );
                            if ( activeRectIt != m_jamSliceSketch->m_riffToBitmapOffset.end() )
                            {
                                const auto& riffRectXY = activeRectIt->second;
                                draw_list->AddRectFilled( pos + riffRectXY, pos + riffRectXY + jamGridCell, jamCellColourEnqueue );
                                draw_list->AddRectFilled( pos + riffRectXY, pos + riffRectXY + jamGridCell, jamCellColourLoading, 4.0f );
                            }
                        }

                    }
                    ImGui::PopStyleVar();

                }
            }
            ImGui::EndChild();


            ImGui::PopFont();
            ImGui::End();
        }
        {
            static WarehouseView::Enum warehouseView = WarehouseView::Default;
            const auto viewTitle = generateWarehouseViewTitle( warehouseView );

            ImGui::Begin( viewTitle.c_str() );

            const bool warehouseHasEndlesssAccess = warehouse.hasFullEndlesssNetworkAccess();

            if ( ImGui::IsWindowHovered( ImGuiHoveredFlags_RootAndChildWindows ) && 
                 ImGui::IsKeyPressedMap( ImGuiKey_Tab, false ) )
            {
                warehouseView = WarehouseView::getNextWrapped( warehouseView );
            }

            if ( warehouseView == WarehouseView::Maintenance )
            {
                ImGui::CenteredText( "Database Maintenance" );
                ImGui::SeparatorBreak();
            }

            static ImGuiTextFilter jamNameFilter;

            {
                ImGuiWindow* window = ImGui::GetCurrentWindow();
                window->DC.CursorPos.y += 3.0f;
                ImGui::TextUnformatted( "Filter : " );
                ImGui::SameLine( 0, 2.0f );
                window->DC.CursorPos.y -= 3.0f;
                jamNameFilter.Draw( "##NameFilter", 220.0f );
                ImGui::SameLine( 0, 2.0f );
                if ( ImGui::Button( ICON_FA_CIRCLE_XMARK ) )
                    jamNameFilter.Clear();
                ImGui::SameLine( 0, 2.0f );
            }

            {
                ImGui::Scoped::ButtonTextAlignLeft leftAlign;
                const ImVec2 toolbarButtonSize{ 140.0f, 0.0f };

                const float warehouseViewWidth = ImGui::GetContentRegionAvail().x;

                // extra tools in a pile
                if ( warehouseView == WarehouseView::Default )
                {
                    ImGui::SameLine( 0, warehouseViewWidth - ( toolbarButtonSize.x * 2.0f ) - 6.0f );

                    // note if the warehouse is running ops, if not then disable new-task buttons
                    const bool warehouseIsPaused = warehouse.workerIsPaused();

                    {
                        // enable or disable the worker thread
                        ImGui::Scoped::ToggleButton highlightButton( !warehouseIsPaused, true );
                        if ( ImGui::Button( warehouseIsPaused ?
                                            ICON_FA_CIRCLE_PLAY  " RESUME  " :
                                            ICON_FA_CIRCLE_PAUSE " RUNNING ", toolbarButtonSize ) )
                        {
                            warehouse.workerTogglePause();
                        }
                    }
                    
                    ImGui::SameLine();

                    ImGui::BeginDisabledControls( !warehouseHasEndlesssAccess );
                    if ( ImGui::Button( ICON_FA_CIRCLE_PLUS " Add Jam...", toolbarButtonSize ) )
                    {
                        // create local copy of the current warehouse jam ID map for use by the popup; avoids
                        // having to worry about warehouse contents shifting underneath / locking mutex in dialog
                        {
                            std::scoped_lock<std::mutex> reportLock( m_warehouseContentsReportMutex );
                            warehouseJamBrowser.m_warehouseJamIDs = m_warehouseContentsReportJamIDs;
                        }
                        // launch modal browser
                        activateModalPopup( "Select Jam To Sync", [&]( const char* title )
                        {
                            ux::modalUniversalJamBrowser( title, m_jamLibrary, warehouseJamBrowser );
                        });
                    }
                    ImGui::EndDisabledControls( !warehouseHasEndlesssAccess );
                }
            }

            ImGui::SeparatorBreak();


            static ImVec2 buttonSizeMidTable( 33.0f, 24.0f );

            if ( ImGui::BeginChild( "##data_child" ) )
            {
                const auto TextColourDownloading  = ImGui::GetStyleColorVec4( ImGuiCol_CheckMark );
                const auto TextColourDownloadable = ImGui::GetStyleColorVec4( ImGuiCol_PlotHistogram );

                const auto columnCount = (warehouseView == WarehouseView::Default) ? 5 : 4;

                if ( ImGui::BeginTable( "##warehouse_table", columnCount,
                            ImGuiTableFlags_ScrollY         |
                            ImGuiTableFlags_Borders         |
                            ImGuiTableFlags_RowBg           |
                            ImGuiTableFlags_NoSavedSettings ))
                {
                    ImGui::TableSetupScrollFreeze( 0, 1 );  // top row always visible

                    if ( warehouseView == WarehouseView::Default )
                    {
                        ImGui::TableSetupColumn( "View",        ImGuiTableColumnFlags_WidthFixed, 32.0f  );
                        ImGui::TableSetupColumn( "Jam Name",    ImGuiTableColumnFlags_WidthFixed, 360.0f );
                        ImGui::TableSetupColumn( "Sync",        ImGuiTableColumnFlags_WidthFixed, 32.0f  );
                        ImGui::TableSetupColumn( "Riffs",       ImGuiTableColumnFlags_WidthFixed, 120.0f );
                        ImGui::TableSetupColumn( "Stems",       ImGuiTableColumnFlags_WidthFixed, 120.0f );
                    }
                    else
                    {
                        ImGui::TableSetupColumn( "Jam Name",    ImGuiTableColumnFlags_WidthFixed, 390.0f );
                        ImGui::TableSetupColumn( "Riffs",       ImGuiTableColumnFlags_WidthFixed, 120.0f );
                        ImGui::TableSetupColumn( "Stems",       ImGuiTableColumnFlags_WidthFixed, 120.0f );
                        ImGui::TableSetupColumn( "Wipe",        ImGuiTableColumnFlags_WidthFixed, 32.0f  );

                    }
                    ImGui::TableHeadersRow();

                    // lock the data report so it isn't whipped away from underneath us mid-render
                    std::scoped_lock<std::mutex> reportLock( m_warehouseContentsReportMutex );

                    for ( size_t jamIdx = 0; jamIdx < m_warehouseContentsReport.m_jamCouchIDs.size(); jamIdx++ )
                    {
                        const std::size_t jI   = m_warehouseContentsSortedIndices[jamIdx];
                        const bool isJamInFlux = m_warehouseContentsReportJamInFlux[jI];

                        const int32_t knownCachedRiffCount = m_jamLibrary.loadKnownRiffCountForDatabaseID( m_warehouseContentsReport.m_jamCouchIDs[jI] );

                        if ( !jamNameFilter.PassFilter( m_warehouseContentsReportJamTitles[jI].c_str() ) )
                            continue;

                        ImGui::PushID( (int32_t)jI );
                        ImGui::TableNextColumn();

                        // highlight or lowlight column based on state
                        if ( isJamInFlux )
                            ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32( ImGuiCol_TableRowBgAlt, 0.0f ) );
                        if ( m_currentViewedJam == m_warehouseContentsReport.m_jamCouchIDs[jI] )
                            ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32( ImGuiCol_TableRowBgAlt, 2.5f ) );

                        if ( warehouseView == WarehouseView::Default )
                        {
                            ImGui::BeginDisabledControls( isJamInFlux );
                            if ( ImGui::PrecisionButton( ICON_FA_EYE, buttonSizeMidTable, 1.0f ) )
                            {
                                clearJamSlice();

                                // change which jam we're viewing, reset active riff hover in the process as this will invalidate it
                                m_currentViewedJam          = m_warehouseContentsReport.m_jamCouchIDs[jI];
                                m_currentViewedJamName      = m_warehouseContentsReportJamTitles[jI];
                                m_jamSliceHoveredRiffIndex  = -1;

                                warehouse.addJamSliceRequest( m_currentViewedJam, std::bind( &LoreApp::newJamSliceGenerated, this, std::placeholders::_1, std::placeholders::_2 ) );

                                ImGui::MakeTabVisible( "Jam View" );
                            }
                            ImGui::EndDisabledControls( isJamInFlux );

                            ImGui::TableNextColumn();
                        }

                        ImGui::Dummy( ImVec2( 0.0f, 1.0f ) );
                        if ( warehouseView == WarehouseView::Maintenance )
                        {
                            ImGui::TextDisabled( "ID" );
                            ImGui::CompactTooltip( m_warehouseContentsReport.m_jamCouchIDs[jI].c_str() );
                            ImGui::SameLine();
                        }
                        ImGui::TextUnformatted( m_warehouseContentsReportJamTitles[jI].c_str() );
                        ImGui::TableNextColumn();

                        if ( warehouseView == WarehouseView::Default )
                        {
                            ImGui::BeginDisabledControls( isJamInFlux || !warehouseHasEndlesssAccess );
                            if ( ImGui::PrecisionButton( ICON_FA_ARROWS_ROTATE, buttonSizeMidTable, 1.0f ) )
                            {
                                m_warehouseContentsReportJamInFlux[jI] = true;
                                warehouse.addOrUpdateJamSnapshot( m_warehouseContentsReport.m_jamCouchIDs[jI] );
                            }
                            ImGui::EndDisabledControls( isJamInFlux || !warehouseHasEndlesssAccess );

                            ImGui::TableNextColumn();
                        }

                        ImGui::Dummy( ImVec2( 0.0f, 1.0f ) );
                        {
                            const auto unpopulated = m_warehouseContentsReport.m_unpopulatedRiffs[jI];
                            const auto populated   = m_warehouseContentsReport.m_populatedRiffs[jI];

                            ImGui::Text( "%" PRIi64, populated );
                            
                            if ( unpopulated > 0 )
                            {
                                ImGui::SameLine( 0, 0 );
                                ImGui::TextColored( TextColourDownloading, " (+%" PRIi64 ")", unpopulated );
                            }
                            else if ( knownCachedRiffCount > populated )
                            {
                                ImGui::SameLine( 0, 0 );
                                ImGui::TextColored( TextColourDownloadable, " (" ICON_FA_ARROW_UP "%i)", knownCachedRiffCount - populated );
                            }
                        }
                        ImGui::TableNextColumn();

                        ImGui::Dummy( ImVec2( 0.0f, 1.0f ) );
                        {
                            const auto unpopulated = m_warehouseContentsReport.m_unpopulatedStems[jI];
                            const auto populated   = m_warehouseContentsReport.m_populatedStems[jI];

                            if ( unpopulated > 0 )
                                ImGui::Text( "%" PRIi64 " (+%" PRIi64 ")", populated, unpopulated );
                            else
                                ImGui::Text( "%" PRIi64, populated);
                        }

                        if ( warehouseView == WarehouseView::Maintenance )
                        {
                            ImGui::TableNextColumn();

                            ImGui::BeginDisabledControls( isJamInFlux );
                            if ( ImGui::PrecisionButton( ICON_FA_TRASH, buttonSizeMidTable ) )
                            {
                                warehouse.requestJamPurge( m_warehouseContentsReport.m_jamCouchIDs[jI] );
                            }
                            ImGui::EndDisabledControls( isJamInFlux );
                        }
                        ImGui::PopID();
                    }

                    ImGui::EndTable();
                }
            }
            ImGui::EndChild();

            ImGui::End();

        } // warehouse imgui 


        maintainStemCacheAsync();

        finishInterfaceLayoutAndRender();
    }

    unregisterStatusBarBlock( sbbWarehouseID );

    m_riffPipeline.reset();

    m_discordBotUI.reset();

    m_vibes.reset();

    // remove the mixer and ensure the async op has taken before leaving scope
    m_mdAudio->blockUntil( m_mdAudio->installMixer( nullptr ) );
    m_mdAudio->blockUntil( m_mdAudio->effectClearAll() );

    // unregister any listeners
    checkedCoreCall( "remove stem listener", [this] { return m_stemDataProcessor.disconnect( m_appEventBus ); } );
    
    checkedCoreCall( "remove op listener",   [this] { return m_appEventBus->removeListener( m_eventListenerOpComplete ); } );
    checkedCoreCall( "remove riff listener", [this] { return m_appEventBus->removeListener( m_eventListenerRiffChange ); } );

#if OURO_FEATURE_VST24
    // serialize effects
    m_effectStack->save( m_appConfigPath );
    m_effectStack.reset();
#endif // OURO_FEATURE_VST24


    return 0;
}

// ---------------------------------------------------------------------------------------------------------------------
int main( int argc, char** argv )
{
    LoreApp lore;
    const int result = lore.Run();
    if ( result != 0 )
        app::Core::waitForConsoleKey();

    return result;
}
