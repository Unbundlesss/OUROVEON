#include "pch.h"

#include "base/utils.h"
#include "base/metaenum.h"
#include "base/instrumentation.h"
#include "base/text.h"
#include "base/text.transform.h"
#include "base/bimap.h"
#include "base/paging.h"

#include "filesys/fsutil.h"
#include "xp/open.url.h"

#include "buffer/buffer.2d.h"

#include "colour/gradient.h"
#include "colour/preset.h"

#include "config/frontend.h"
#include "config/data.h"
#include "config/audio.h"

#include "app/module.audio.h"
#include "app/module.frontend.h"
#include "app/module.frontend.fonts.h"
#include "app/module.midi.h"

#include "app/ouro.h"
#include "math/rng.h"


#include "ux/diskrecorder.h"
#include "ux/proc.weaver.h"
#include "ux/jams.browser.h"
#include "ux/jam.precache.h"
#include "ux/jam.validate.h"
#include "ux/stem.beats.h"
#include "ux/stem.analysis.h"
#include "ux/shared.riffs.view.h"
#include "ux/riff.tagline.h"
#include "ux/riff.history.h"
#include "ux/user.selector.h"

#include "vx/vibes.h"

#include "discord/discord.bot.ui.h"

#include "endlesss/all.h"

#include "effect/effect.stack.h"

#include "mix/preview.h"
#include "mix/stem.amalgam.h"

#include "gfx/sketchbook.h"

#include "io/tarch.h"

#include "net/bond.riffpush.h"
#include "net/bond.riffpush.connect.h"

#include "ImGuiFileDialog.h"



#define OUROVEON_LORE           "LORE"
#define OUROVEON_LORE_VERSION   OURO_FRAMEWORK_VERSION "-beta"


// ---------------------------------------------------------------------------------------------------------------------
#define _WH_VIEW_STATES(_action)    \
      _action(Default)              \
      _action(ContentsManagement)   \
      _action(ImportExport)         \
      _action(Advanced)

DEFINE_PAGE_MANAGER( WarehouseView, ICON_FA_DATABASE " Data Warehouse", "data_warehouse_view", _WH_VIEW_STATES );

#undef _WH_VIEW_STATES

// ---------------------------------------------------------------------------------------------------------------------
#define _JAM_VIEW_STATES(_action)   \
      _action(Default)              \
      _action(Visualisation)

DEFINE_PAGE_MANAGER( JamView, ICON_FA_GRIP " Jam View", "jam_view", _JAM_VIEW_STATES );

#undef _JAM_VIEW_STATES

// ---------------------------------------------------------------------------------------------------------------------
#define _TAG_VIEW_STATES(_action)   \
      _action(Default)              \
      _action(Management)

DEFINE_PAGE_MANAGER( TagView, ICON_FA_ANGLES_UP " Jam Tags", "jam_view_tags", _TAG_VIEW_STATES );

#undef _TAG_VIEW_STATES


// ---------------------------------------------------------------------------------------------------------------------
struct JamVisualisation
{
    #define _RIFFCUBE_SIZE(_action)   \
        _action(Nano)                 \
        _action(Small)                \
        _action(Medium)               \
        _action(Large)                \
        _action(Jumbo)
    REFLECT_ENUM( RiffCubeSize, uint32_t, _RIFFCUBE_SIZE );
    #undef _RIFFCUBE_SIZE

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

    #define _SIGHTLINE_MODE(_action)  \
        _action(UserOne)              \
        _action(UserTwo)              \
        _action(Tagged)               \
        _action(Scale)                \
        _action(Root)                 \
        _action(BPM)
    REFLECT_ENUM( SightlineMode, uint32_t, _SIGHTLINE_MODE );
    #undef _SIGHTLINE_MODE

    #define _COLOUR_STYLE(_action)    \
        _action( Uniform )            \
        _action( StemOwnership )      \
        _action( UserIdentity )       \
        _action( UserChangeRate )     \
        _action( StemChurn )          \
        _action( StemTimestamp )      \
        _action( Scale )              \
        _action( Root )               \
        _action( BPM )
    REFLECT_ENUM( ColouringMode, uint32_t, _COLOUR_STYLE );
    #undef _COLOUR_STYLE

    #define _COLOUR_SOURCE(_action)     \
        _action( ColdBlueHotYellow )    \
        _action( ColdBlueCyanWhite )    \
        _action( TealSlateGold )        \
        _action( PurplePinkRed )        \
        _action( Spectral )             \
        _action( SpectralIntense )      \
        _action( Trans )
    REFLECT_ENUM( GradientChoice, uint32_t, _COLOUR_SOURCE );
    #undef _COLOUR_SOURCE

    const char* GetDescriptionForColouringMode( const ColouringMode::Enum cs ) const
    {
        switch ( cs )
        {
            case ColouringMode::Uniform:          return "Single colour for all cells.";
            case ColouringMode::StemOwnership:    return "Increased heat colour depending on how many of the stems in the riff are by the current/primary user";
            case ColouringMode::UserIdentity:     return "Pick a different colour per user from the gradient; colour repetition will occur over a larger jam with many unique users.";
            case ColouringMode::UserChangeRate:   return "Increased heat as riffs are submitted by different users. Good for tracking general jam busyness, higher heat indicates more multi-user activity. 'Decay Rate' controls heat decay as the jam progresses.";
            case ColouringMode::StemChurn:        return "Increased heat when new stems arrive or stems are disabled/enabled. Higher heat indicates major changes to the jam.";
            case ColouringMode::StemTimestamp:    return "Heat value is picked from 24-hour clock of the stem submission, showing when stems are committed over the course of a day/night cycle.";
            case ColouringMode::Scale:            return "Heat value from the riff scale.";
            case ColouringMode::Root:             return "Heat value from the riff root key.";
            case ColouringMode::BPM:              return "Heat value from the riff BPM, to the chosen maximum value.";
        }
        return "unknown";
    }

    struct NameHighlighting
    {
        NameHighlighting( const ImVec4& colour )
            : m_colour( colour )
        {}

        ImGui::ux::UserSelector     m_user;
        ImVec4                      m_colour;

        // precomputed values used during the rendering inner-loop; hashed name for fast comparisons, u32 colour value, etc
        struct Cached
        {
            uint64_t    m_nameHash;
            uint32_t    m_highlightColour;
            bool        m_active;
        };

        Cached makeCached() const
        {
            Cached result;
            result.m_active             = m_user.isEmpty() == false;
            result.m_nameHash           = absl::Hash< std::string >{}(m_user.getUsername());
            result.m_highlightColour    = ImGui::ColorConvertFloat4ToU32_BGRA_Flip( m_colour );

            return result;
        }
    };

    NameHighlighting        m_userHighlight1    = NameHighlighting( colour::shades::white.neutral()     );  // highlight user riff with top-left indicator (primary)
    NameHighlighting        m_userHighlight2    = NameHighlighting( colour::shades::sea_green.neutral() );  // highlight user riff with top-right indicator (secondary)

    RiffCubeSize::Enum      m_riffCubeSize      = RiffCubeSize::Medium;
    LineBreakOn::Enum       m_lineBreakOn       = LineBreakOn::ChangedBPM;
    RiffGapOn::Enum         m_riffGapOn         = RiffGapOn::ChangedScaleOrRoot;
    ColouringMode::Enum     m_colourMode        = ColouringMode::UserChangeRate;
    GradientChoice::Enum    m_gradientChoice    = GradientChoice::ColdBlueHotYellow;
    SightlineMode::Enum     m_sightlineMode     = SightlineMode::UserOne;

    // defaults that can be then tuned on the UI depending which colour viz is running
    float                   m_bpmMinimum        = 50.0f;
    float                   m_bpmMaximum        = 200.0f;
    float                   m_changeRateDecay   = 0.6f;
    ImVec4                  m_uniformColour     = colour::shades::slate.neutral();

    template<class Archive>
    void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( m_userHighlight1 )
               , CEREAL_NVP( m_userHighlight2 )
               , CEREAL_NVP( m_riffCubeSize )
               , CEREAL_NVP( m_lineBreakOn )
               , CEREAL_NVP( m_riffGapOn )
               , CEREAL_NVP( m_colourMode )
               , CEREAL_NVP( m_gradientChoice )
               , CEREAL_NVP( m_bpmMinimum )
               , CEREAL_NVP( m_bpmMaximum )
               , CEREAL_NVP( m_changeRateDecay )
               , CEREAL_NVP( m_uniformColour )
        );
    }

    // can tune these based on feedback, don't think there's point data driving such a detail
    uint32_t getRiffCubeSize() const
    {
        switch ( m_riffCubeSize )
        {
            case RiffCubeSize::Nano:    return 16;
            case RiffCubeSize::Small:   return 20;
            default:
            case RiffCubeSize::Medium:  return 24;
            case RiffCubeSize::Large:   return 28;
            case RiffCubeSize::Jumbo:   return 34;
        }
    }

    // a selection of heatmap colour gradients, trialled and chosen to provide good readability and
    // contrast against the existing UI colour scheme
    colour::col3 getHeatmapColourAtT( const float t ) const
    {
        switch ( m_gradientChoice )
        {
        default:
            case GradientChoice::TealSlateGold:       return colour::map::cividis( t );
            case GradientChoice::ColdBlueHotYellow:   return colour::map::plasma( t );
            case GradientChoice::ColdBlueCyanWhite:   return colour::map::YlGnBu_r( t );
            case GradientChoice::PurplePinkRed:       return colour::map::coolwarm( t );
            case GradientChoice::Spectral:            return colour::map::Spectral_r( t );
            case GradientChoice::SpectralIntense:     return colour::map::RdYlBu_r( t );
            case GradientChoice::Trans:               return colour::map::trans( t );
        }
    }

    inline bool imgui( app::CoreGUI& coreGUI )
    {
        bool choiceChanged = false;

        const float labelColumnsSize = 100.0f;
        const float subgroupInsetSize = 16.0f;
        const float panelWidth = ImGui::GetContentRegionAvail().x;

        ImGui::PushItemWidth( (panelWidth - labelColumnsSize) * 0.7f );
        {
            ImGui::TextUnformatted( "Riff Cell Rendering" );
            ImGui::SeparatorBreak();
            ImGui::Indent( subgroupInsetSize );

            if ( ImGui::BeginTable( "###riff_cell_config", 2,
                ImGuiTableFlags_NoSavedSettings ) )
            {
                ImGui::TableSetupColumn( "Label", ImGuiTableColumnFlags_WidthFixed, labelColumnsSize );
                ImGui::TableSetupColumn( "Content", ImGuiTableColumnFlags_WidthStretch, 1.0f );

                ImGui::TableNextColumn();
                ImGui::TextUnformatted( "Cell Size" );
                ImGui::TableNextColumn();
                choiceChanged |= RiffCubeSize::ImGuiCombo( "###cell_size", m_riffCubeSize );

                ImGui::TableNextColumn();
                ImGui::TextUnformatted( "Colouring" );
                ImGui::TableNextColumn();
                choiceChanged |= ColouringMode::ImGuiCombo( "###colour_style", m_colourMode );

                if ( m_colourMode == ColouringMode::Uniform )
                {
                    ImGui::SameLine( 0, 3.0f );
                    choiceChanged |= ImGui::ColorEdit3( "###uniform_colour", (float*)&m_uniformColour.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_DisplayHex );
                }
                else
                {
                    // in-view tooltip for the colour mode
                    {
                        ImGui::Indent( 12.0f );
                        ImGui::PushTextWrapPos( 0.0f );
                        ImGui::TextUnformatted( GetDescriptionForColouringMode( m_colourMode ) );
                        ImGui::PopTextWrapPos();
                        ImGui::Unindent( 12.0f );
                        ImGui::Spacing();
                    }

                    // alongside the gradient function choice, render a line of swatches showing a preview of the colour results
                    choiceChanged |= GradientChoice::ImGuiCombo( "##gradient_choice", m_gradientChoice );
                    {
                        const int32_t numberOfGradientSampleBoxes = 5;
                        const float sampleDelta = 1.0f / static_cast<float>(numberOfGradientSampleBoxes - 1);
                        float sampleT = 0.0f;

                        for ( int32_t gradientBox = 0; gradientBox < numberOfGradientSampleBoxes; gradientBox++ )
                        {
                            const colour::col3 previewColour = getHeatmapColourAtT( sampleT );
                            sampleT += sampleDelta;

                            ImGui::PushID( gradientBox );
                            ImGui::SameLine( 0, 3.0f );
                            ImGui::ColorEdit3( "###preview_block", (float*)&previewColour,
                                ImGuiColorEditFlags_NoAlpha |
                                ImGuiColorEditFlags_NoInputs |
                                ImGuiColorEditFlags_NoPicker |
                                ImGuiColorEditFlags_NoOptions |
                                ImGuiColorEditFlags_NoTooltip |
                                ImGuiColorEditFlags_NoBorder
                            );
                            ImGui::PopID();
                        }
                    }

                    // add in any other configurable values for the active colour mode, like the BPM range to use for scaling
                    if ( m_colourMode == ColouringMode::BPM )
                        choiceChanged |= ImGui::DragFloatRange2( "BPM Range", &m_bpmMinimum, &m_bpmMaximum, 1.0f, 25.0f, 999.0f, "%.0f" );
                    else if ( m_colourMode == ColouringMode::UserChangeRate )
                        choiceChanged |= ImGui::InputFloat( "Decay Rate", &m_changeRateDecay, 0.05f, 0.1f, " %.2f" );
                }

                ImGui::EndTable();
            }
            ImGui::Unindent( subgroupInsetSize );
        }
        {
            ImGui::Dummy(ImVec2(0,10.0f));
            ImGui::TextUnformatted( "Riff Cell Layout" );
            ImGui::SeparatorBreak();
            ImGui::Indent( subgroupInsetSize );

            if ( ImGui::BeginTable( "###riff_layout_config", 2,
                ImGuiTableFlags_NoSavedSettings ) )
            {
                ImGui::TableSetupColumn( "Label",   ImGuiTableColumnFlags_WidthFixed, labelColumnsSize );
                ImGui::TableSetupColumn( "Content", ImGuiTableColumnFlags_WidthStretch, 1.0f );

                ImGui::TableNextColumn();
                ImGui::TextUnformatted( "Line Break" );
                ImGui::TableNextColumn();
                choiceChanged |= LineBreakOn::ImGuiCombo( "###line_break", m_lineBreakOn );

                ImGui::TableNextColumn();
                ImGui::TextUnformatted( "Riff Gap" );
                ImGui::TableNextColumn();
                choiceChanged |= RiffGapOn::ImGuiCombo( "###riff_gap", m_riffGapOn );

                ImGui::EndTable();
            }
            ImGui::Unindent( subgroupInsetSize );
        }
        {
            ImGui::Dummy(ImVec2(0,10.0f));
            ImGui::TextUnformatted( "User Highlighting" );
            ImGui::SeparatorBreak();
            ImGui::Indent( subgroupInsetSize );

            if ( ImGui::BeginTable( "###user_highlights", 3,
                ImGuiTableFlags_NoSavedSettings ) )
            {
                ImGui::TableSetupColumn( "Label", ImGuiTableColumnFlags_WidthFixed, labelColumnsSize );
                ImGui::TableSetupColumn( "Names", ImGuiTableColumnFlags_WidthFixed, ImGui::ux::UserSelector::cDefaultWidthForUserSize );
                ImGui::TableSetupColumn( "Names", ImGuiTableColumnFlags_WidthStretch, 1.0f );

                {
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted( "Primary" );

                    ImGui::TableNextColumn();
                    choiceChanged |= m_userHighlight1.m_user.imgui( "user1", coreGUI.getEndlesssPopulation(), ImGui::ux::UserSelector::cDefaultWidthForUserSize );

                    ImGui::TableNextColumn();
                    choiceChanged |= ImGui::ColorEdit3( "###user1_colour", (float*)&m_userHighlight1.m_colour.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_DisplayHex );
                }
                {
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted( "Secondary" );

                    ImGui::TableNextColumn();
                    choiceChanged |= m_userHighlight2.m_user.imgui( "user2", coreGUI.getEndlesssPopulation(), ImGui::ux::UserSelector::cDefaultWidthForUserSize );

                    ImGui::TableNextColumn();
                    choiceChanged |= ImGui::ColorEdit3( "###user2_colour", (float*)&m_userHighlight2.m_colour.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_DisplayHex );
                }

                ImGui::EndTable();
            }
            ImGui::Unindent( subgroupInsetSize );
        }

//         ImGui::Dummy( ImVec2( 0, 10.0f ) );
//         ImGui::TextUnformatted( "Sightline Mode" );
//         ImGui::SeparatorBreak();

        ImGui::PopItemWidth();

        return choiceChanged;
    }
};

// ---------------------------------------------------------------------------------------------------------------------
struct LoreApp final : public app::OuroApp,
                       public ux::TagLineToolProvider
{
    LoreApp()
        : app::OuroApp()
        , m_rpClient( GetAppName() )
    {
        m_discordBotUI = std::make_unique<discord::BotWithUI>( *this );
    }

    ~LoreApp()
    {
    }

    const char* GetAppName() const override { return OUROVEON_LORE; }
    const char* GetAppNameWithVersion() const override { return (OUROVEON_LORE " " OUROVEON_LORE_VERSION); }
    const char* GetAppCacheName() const override { return "lore"; }

    bool supportsUnauthorisedEndlesssMode() const override { return true; }

    int EntrypointOuro() override;




protected:


    std::vector< app::module::MidiDevice >  m_midiInputDevices;


    // discord bot & streaming panel 
    std::unique_ptr< discord::BotWithUI >   m_discordBotUI;

#if OURO_FEATURE_VST24
    // VST playground
    std::unique_ptr< effect::EffectStack >  m_effectStack;
#endif // OURO_FEATURE_VST24
    
    std::unique_ptr< vx::Vibes >            m_vibes;

    std::unique_ptr< ux::SharedRiffView >   m_uxSharedRiffView;
    std::unique_ptr< ux::TagLine >          m_uxTagLine;
    std::unique_ptr< ux::RiffHistory >      m_uxRiffHistory;
    std::unique_ptr< ux::Weaver >           m_uxProcWeaver;



    bool                                    m_showStemAnalysis = false;


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

    base::EventListenerID           m_eventListenerRiffEnqueue;


    using OperationsRunningOnJamIDs = absl::btree_multimap< endlesss::types::JamCouchID, base::OperationID >;
    using OperationToSourceJamID    = absl::flat_hash_map< base::OperationID, endlesss::types::JamCouchID >;

    OperationsRunningOnJamIDs       m_operationsRunningOnJamIDs;
    OperationToSourceJamID          m_operationToSourceJamID;

    void addOperationToJam( const endlesss::types::JamCouchID& jamID, const base::OperationID& operationID )
    {
        m_operationsRunningOnJamIDs.emplace( jamID, operationID );
        m_operationToSourceJamID.emplace( operationID, jamID );
    }

    bool doesJamHaveActiveOperations( const endlesss::types::JamCouchID& jamID ) const
    {
        return m_operationsRunningOnJamIDs.contains( jamID );
    }

    bool clearOperationFromJam( const base::OperationID& operationID )
    {
        const auto oIt = m_operationToSourceJamID.find( operationID );
        ABSL_ASSERT( oIt != m_operationToSourceJamID.end() );
        if ( oIt != m_operationToSourceJamID.end() )
        {
            const endlesss::types::JamCouchID originJamID = oIt->second;
            m_operationToSourceJamID.erase( operationID );

            const auto count = absl::erase_if( m_operationsRunningOnJamIDs, [=]( const auto& item )
                {
                    auto const& [jam, op] = item;
                    return op == operationID;
                });

            ABSL_ASSERT( count == 1 );
            return count == 1;
        }
        return false;
    }



    static constexpr base::OperationVariant OV_RiffExport{ 0xBA };
    static constexpr base::OperationVariant OV_RiffPlayback{ 0xBB };

    using RiffExportOperations = base::BiMap< base::OperationID, endlesss::types::RiffCouchID >;

    RiffPipeline                    m_riffExportPipeline;
    RiffExportOperations            m_riffExportOperationsMap;

    void enqueueRiffForExport( const endlesss::types::RiffIdentity& identity )
    {
        const auto operationID = base::Operations::newID( OV_RiffExport );

        m_riffExportOperationsMap.add( operationID, identity.getRiffID() );
        m_riffExportPipeline->requestRiff( { identity, operationID } );
    }



    // ---------------------------------------------------------------------------------------------------------------------
    void onEvent_EnqueueRiffPlayback( const base::IEvent& eventRef )
    {
        ABSL_ASSERT( eventRef.getID() == events::EnqueueRiffPlayback::ID );

        const events::EnqueueRiffPlayback* enqueueRiffPlaybackEvent = dynamic_cast<const events::EnqueueRiffPlayback*>(&eventRef);
        ABSL_ASSERT( enqueueRiffPlaybackEvent != nullptr );

        requestRiffPlayback( enqueueRiffPlaybackEvent->m_identity, m_riffPlaybackAbstraction.asPermutation() );
    }

    base::OperationID requestRiffPlayback( const endlesss::types::RiffIdentity& riffIdent, const endlesss::types::RiffPlaybackPermutation& playback )
    {
        const auto operationID = base::Operations::newID( OV_RiffPlayback );

        const auto& riffCouchID = riffIdent.getRiffID();

        blog::app( FMTX("requestRiffPlayback: {}"), riffCouchID );

        m_riffsQueuedForPlayback.emplace( riffCouchID );        // stash it in the "queued but not playing yet" list; removed from 
                                                                // when the mixer eventually gets to playing it

        m_syncAndPlaybackInFlight.emplace( riffCouchID );       // log that we will be asynchronously fetching this

        m_riffPipeline->requestRiff( { riffIdent, playback, operationID } ); // kick the request to the pipeline

        return operationID;
    }

    void event_PanicStop( const events::PanicStop* eventData )
    {
        m_riffPipelineClearInProgress = true;
        m_riffPipeline->requestClear();
    }

    void event_MixerRiffChange( const events::MixerRiffChange* eventData )
    {
        m_nowPlayingRiff = eventData->m_riff;

        // might be a empty riff, only track actual riffs
        if ( eventData->m_riff != nullptr )
            m_riffsDequedByMixer.emplace( m_nowPlayingRiff->m_riffData.riff.couchID );
    }

    void event_OperationComplete( const events::OperationComplete* eventData )
    {
        const auto variant = base::Operations::variantFromID( eventData->m_id );
        
        if ( variant == mix::Preview::OV_Permutation )
        {
            m_permutationOperationImGuiMap.remove( eventData->m_id );
        }
        else
        if ( variant == OV_RiffExport )
        {
            m_riffExportOperationsMap.remove( eventData->m_id );
        }
        else
        if ( variant == endlesss::toolkit::Warehouse::OV_AddOrUpdateJamSnapshot ||
             variant == endlesss::toolkit::Warehouse::OV_ExportAction )
        {
            clearOperationFromJam( eventData->m_id );
        }
    }

    void event_BNSWasUpdated( const events::BNSWasUpdated* eventData )
    {
        m_warehouse->requestContentsReport();
    }


    endlesss::types::RiffPlaybackAbstraction    m_riffPlaybackAbstraction;

    base::BiMap< base::OperationID, ImGuiID >   m_permutationOperationImGuiMap;

    base::EventListenerID                       m_eventLID_PanicStop         = base::EventListenerID::invalid();
    base::EventListenerID                       m_eventLID_MixerRiffChange   = base::EventListenerID::invalid();
    base::EventListenerID                       m_eventLID_OperationComplete = base::EventListenerID::invalid();
    base::EventListenerID                       m_eventLID_BNSWasUpdated     = base::EventListenerID::invalid();

protected:
    endlesss::types::JamCouchID                 m_currentViewedJam;
    endlesss::toolkit::Warehouse::ChangeIndex   m_currentViewedJamChangeIndex = endlesss::toolkit::Warehouse::ChangeIndex::invalid();
    std::string                                 m_currentViewedJamName;

    std::optional< endlesss::types::RiffIdentity >  m_currentViewedJamScrollToRiff = std::nullopt;


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
        m_warehouseContentsReportJamTitlesForSort.clear();
        m_warehouseContentsReportJamTimestamp.clear();
        m_warehouseContentsReportJamInFlux.clear();
        m_warehouseContentsReportJamInFluxMoment.clear();
        m_warehouseContentsReportJamInFluxSet.clear();


        for ( auto jIdx = 0; jIdx < m_warehouseContentsReport.m_jamCouchIDs.size(); jIdx++ )
        {
            const auto& jamCouchID = m_warehouseContentsReport.m_jamCouchIDs[jIdx];

            m_warehouseContentsReportJamIDs.emplace( jamCouchID );

            // resolve data for the jam ID
            {
                std::string resolvedName;
                uint64_t resolvedTimestamp = 0;
                const auto lookupResult = lookupJamNameAndTime( jamCouchID, resolvedName, resolvedTimestamp );

                // no usable timestamp data for fall-back paths, sort to the bottom of the list
                if ( resolvedTimestamp == 0 )
                    resolvedTimestamp = m_warehouseContentsReportJamTimestamp.size();

                // emplace timestamp
                m_warehouseContentsReportJamTimestamp.emplace_back( resolvedTimestamp );

                if ( lookupResult == endlesss::services::IJamNameResolveService::LookupResult::NotFound )
                {
                    m_warehouseContentsReportJamTitlesForSort.emplace_back( "[ unknown id ]" );
                    m_warehouseContentsReportJamTitles.emplace_back( "[ Unknown ID ]" );

                    // failed to find anything in our local stores, go get an async request going to find the name from the servers
                    m_appEventBus->send< ::events::BNSCacheMiss >( jamCouchID );
                }
                else
                {
                    m_warehouseContentsReportJamTitlesForSort.emplace_back( base::StrToLwrExt( resolvedName ) );
                    m_warehouseContentsReportJamTitles.emplace_back( std::move( resolvedName ) );
                }
            }

            if ( m_warehouseContentsReport.m_unpopulatedRiffs[jIdx] > 0 ||
                 m_warehouseContentsReport.m_unpopulatedStems[jIdx] > 0 ||
                 m_warehouseContentsReport.m_awaitingInitialSync[jIdx] )
            {
                m_warehouseContentsReportJamInFluxSet.emplace( jamCouchID );
                m_warehouseContentsReportJamInFlux.push_back( true );
            }
            else
            {
                m_warehouseContentsReportJamInFlux.push_back( false );
            }
            m_warehouseContentsReportJamInFluxMoment.emplace_back();
        }

        generateWarehouseContentsSortOrder();
    }

    #define _WAREHOUSE_SORT(_action)   \
        _action(ByJoinTime)            \
        _action(ByName)
    REFLECT_ENUM( WarehouseContentsSortMode, uint32_t, _WAREHOUSE_SORT );
    #undef _WAREHOUSE_SORT

    void generateWarehouseContentsSortOrder()
    {
        // create a simple array of indices that represent the jam entries
        // when we then sort this array using the jam data as reference, producing a sorted look-up list
        m_warehouseContentsSortedIndices.clear();
        m_warehouseContentsSortedIndices.reserve( m_warehouseContentsReportJamTitles.size() );
        for ( auto sortIndex = 0; sortIndex < m_warehouseContentsReportJamTitles.size(); sortIndex++ )
            m_warehouseContentsSortedIndices.emplace_back( sortIndex );

        std::sort( m_warehouseContentsSortedIndices.begin(), m_warehouseContentsSortedIndices.end(),
            [&]( const std::size_t lhsIdx, const std::size_t rhsIdx ) -> bool
            {
                switch ( m_warehouseContentsSortMode )
                {
                    case WarehouseContentsSortMode::ByName:
                        return m_warehouseContentsReportJamTitlesForSort[lhsIdx] <
                               m_warehouseContentsReportJamTitlesForSort[rhsIdx];

                    case WarehouseContentsSortMode::ByJoinTime:
                        return m_warehouseContentsReportJamTimestamp[lhsIdx] <
                               m_warehouseContentsReportJamTimestamp[rhsIdx];

                    default:
                        ABSL_ASSERT( false );
                        return 0;
                }
            });
    }

    // for calling generateWarehouseContentsSortOrder() again, after the fact, to re-sort dynamically
    void updateWarehouseContentsSortOrder()
    {
        std::scoped_lock<std::mutex> reportLock( m_warehouseContentsReportMutex );
        generateWarehouseContentsSortOrder();
    }


    std::string                                     m_warehouseWorkState;
    bool                                            m_warehouseWorkUnderway = false;

    std::mutex                                      m_warehouseContentsReportMutex;
    endlesss::toolkit::Warehouse::ContentsReport    m_warehouseContentsReport;
    endlesss::types::JamCouchIDSet                  m_warehouseContentsReportJamIDs;
    std::vector< std::string >                      m_warehouseContentsReportJamTitles;
    std::vector< std::string >                      m_warehouseContentsReportJamTitlesForSort;
    std::vector< int64_t >                          m_warehouseContentsReportJamTimestamp;
    std::vector< bool >                             m_warehouseContentsReportJamInFlux;
    std::vector< spacetime::Moment >                m_warehouseContentsReportJamInFluxMoment;
    endlesss::types::JamCouchIDSet                  m_warehouseContentsReportJamInFluxSet;      // any jams that have unfetched data
    WarehouseContentsSortMode::Enum                 m_warehouseContentsSortMode                 = WarehouseContentsSortMode::ByName;
    std::vector< std::size_t >                      m_warehouseContentsSortedIndices;

    void triggerSyncOnJamInContentsReport( std::size_t index )
    {
        ABSL_ASSERT( index < m_warehouseContentsReportJamInFlux.size() );

        const auto iterCurrentJamID = m_warehouseContentsReport.m_jamCouchIDs[index];

        m_warehouseContentsReportJamInFlux[index] = true;
        m_warehouseContentsReportJamInFluxMoment[index].setToFuture( std::chrono::seconds( 4 ) );

        const base::OperationID updateOperationID = m_warehouse->addOrUpdateJamSnapshot( iterCurrentJamID );
        addOperationToJam( iterCurrentJamID, updateOperationID );
    }

    struct WarehouseJamBrowserBehaviour : public ux::UniversalJamBrowserBehaviour
    {
        // local copy of warehouse lookup to avoid threading drama
        endlesss::types::JamCouchIDSet      m_warehouseJamIDs;
    };


public:

    bool isJamBeingSynced( const endlesss::types::JamCouchID& jamID )
    {
        return m_warehouseContentsReportJamInFluxSet.contains( jamID );
    }


// 
protected:

    struct ViewDimension
    {
        ViewDimension() = default;
        ViewDimension( const ImVec2& dims )
            : m_width( static_cast<int32_t>( std::ceil( dims.x ) ) )
            , m_height( static_cast<int32_t>( std::ceil( dims.y ) ) )
        {}

        constexpr bool operator == ( const ViewDimension& other ) const
        {
            return (m_width == other.m_width) && (m_height == other.m_height);
        }

        constexpr bool isValid() const
        {
            return (m_width > 0 && m_height > 0);
        }

        int32_t     m_width = 0;
        int32_t     m_height = 0;
    };

    std::unique_ptr< gfx::Sketchbook >    m_sketchbook;

    void syncJamViewLayoutAndAsyncRendering( const ImVec2& dimensions, const int32_t browserHeight )
    {
        const ViewDimension viewDim( dimensions );

        if ( m_jamViewDimensionsCommitCount < 0 )
        {
            m_jamViewDimensions             = viewDim;
            m_jamViewDimensionsToCommit     = viewDim;
            m_jamViewBrowserHeight          = browserHeight;
            m_jamViewDimensionsCommitCount  = 0;
        }
        else
        {
            if ( m_jamViewDimensionsToCommit == viewDim &&
                 m_jamViewDimensions != viewDim )
            {
                m_jamViewDimensionsCommitCount++;
                if ( m_jamViewDimensionsCommitCount > 60 * 2 )
                {
                    blog::app( "setting new jam view [ {} x {} ] with browser height {}", viewDim.m_width, viewDim.m_height, browserHeight );

                    m_jamViewDimensions     = viewDim;
                    m_jamViewBrowserHeight  = browserHeight;
                    m_jamViewDimensionsCommitCount = 0;

                    notifyForRenderUpdate();
                }
            }

            m_jamViewDimensionsToCommit = viewDim;
        }

        updateJamSliceRendering();
    }

    std::mutex                              m_jamSliceMapLock;

    // which riff the mouse is currently hovered over in the active jam slice; or -1 if not valid
    int32_t                                 m_jamSliceHoveredRiffIndex = -1;

    // hax; alt-clicking lets us operate on a range of riffs (ie for lining up a big chunk of the jam) - if this is >=0 this was the last alt-clicked riff
    int32_t                                 m_jamSliceRangeClick = -1;

    ViewDimension                           m_jamViewDimensions;
    ViewDimension                           m_jamViewDimensionsToCommit;
    int32_t                                 m_jamViewBrowserHeight = 0;
    int32_t                                 m_jamViewDimensionsCommitCount = 60;

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

        std::vector< float >                m_jumpTargetsY;
        std::vector< uint32_t >             m_sightlineRowOn;

        UserHashFMap                        m_jamViewRenderUserHashFMap;

        float                               m_currentScrollY = 0;
        bool                                m_syncToUI = false;

        int32_t                             m_jamViewFullHeight = 0;

        std::vector< gfx::SketchUploadPtr > m_textures;
        gfx::SketchUploadPtr                m_sightlineUpload;

        void prepare( endlesss::toolkit::Warehouse::JamSlicePtr&& slicePtr )
        {
            m_slice = std::move( slicePtr );
        }

        void raster(
            gfx::Sketchbook& sketchbook,
            const JamVisualisation& jamViz,
            const ViewDimension& viewDimensions,
            const int32_t viewBrowserHeight )
        {
            base::instr::ScopedEvent wte( "JamSlice::raster", base::instr::PresetColour::Violet );

            // this should have been aborted with invalid dimensions before we ever get in here
            ABSL_ASSERT( viewDimensions.isValid() );

            m_textures.clear();
            m_jamViewRenderUserHashFMap.clear();

            m_sightlineUpload.reset();

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

            // these counts are purely speculative, we don't know how large the final payloads will be
            m_jumpTargetsY.clear();
            m_jumpTargetsY.reserve( totalRiffs >> 2 );
            m_sightlineRowOn.clear();
            m_sightlineRowOn.reserve( totalRiffs >> 2 );


            // get the size of cubes to be rendering
            const uint32_t  riffCubeSize        = jamViz.getRiffCubeSize();
            const uint32_t  riffCubeCorner      = jamViz.getRiffCubeSize() / 4;
            const float     riffCubeSizeF       = static_cast<float>( jamViz.getRiffCubeSize() );

            ABSL_ASSERT( riffCubeSize > 1 );

            // get cached data for doing user highlighting
            const auto vizUserHighlight1        = jamViz.m_userHighlight1.makeCached();
            const auto vizUserHighlight2        = jamViz.m_userHighlight2.makeCached();

            // convert uniform colour choice in case it is in use
            const uint32_t bgraUniformColourU32 = ImGui::ColorConvertFloat4ToU32_BGRA_Flip( jamViz.m_uniformColour );


            // compute how many cells we render per row
            const int32_t cellColumns           = std::max( 16, (int32_t)std::floor( (float)viewDimensions.m_width / riffCubeSizeF ) );
            
            // given a fixed texture page height, mostly-accurate guess at how many rows we can fit in
            // the width of the page is determined by the size of the window, rounded up to the next pow2
            const int32_t pageHeight            = 1024;
            const int32_t cellRowsPerPage       = (int32_t)std::floor( (pageHeight - riffCubeSize) / riffCubeSizeF );

            // go reserve us a texture page to start drawing on
            // each time we exhaust a page, we fetch a new identically sized one from the book and continue
            const gfx::DimensionsPow2 sketchPageDim( viewDimensions.m_width, pageHeight );
            gfx::SketchBufferPtr activeSketch = sketchbook.getBuffer( sketchPageDim );


            int32_t cellX = 0;
            int32_t cellY = 0;
            int32_t fullCellY = 0;

            const auto commitCurrentPage = [&]()
            {
                // extents are the current cell Y offset, +1 because cellY is a top-left coordinate so we're ensuring the whole
                // terminating row gets included in the texture
                gfx::Dimensions extents( viewDimensions.m_width, (cellY + 1) * riffCubeSize );

                activeSketch->setExtents( extents );
                m_textures.emplace_back( sketchbook.scheduleBufferUploadToGPU( std::move( activeSketch ) ) );
            };

            uint32_t sightlineRowColour = 0;

            const auto incrementCellY = [&]()
            {
                // run out of page space?
                if ( cellY + 1 >= cellRowsPerPage )
                {
                    // send this off to be pushed to GPU, pull a fresh page out ready to scribble on
                    commitCurrentPage();

                    activeSketch = sketchbook.getBuffer( sketchPageDim );
                    cellY = 0;
                }
                // space left on the current page, just increment cellY
                else
                {
                    cellY++;
                }

                // log if the most recent row needed an entry in the sightline map (then auto-resets that tracking variable)
                m_sightlineRowOn.emplace_back( sightlineRowColour );
                sightlineRowColour = 0;

                fullCellY++;
            };
            

            const float imguiSmallFontSize = 13.0f; // TODO get this from imgio
            const float labelCenteringOffset = (riffCubeSize * 0.5f) - (imguiSmallFontSize * 0.5f);

            const auto addLineBreak = [&]( std::string label )
            {
                // only increment twice if the line feed hadn't just happened
                if ( cellX != 0 )
                    incrementCellY();

                m_labelY.emplace_back( (float)( fullCellY * riffCubeSize) + labelCenteringOffset );
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

                    switch ( jamViz.m_lineBreakOn )
                    {
                        default:
                        case JamVisualisation::LineBreakOn::Never:
                            break;

                        case JamVisualisation::LineBreakOn::ChangedBPM:
                        {
                            if ( riffI == 0 ||
                                !base::floatAlmostEqualRelative( lastBPM, riffBPM, 0.001f ) )
                            {
                                addLineBreak( fmt::format( "{} BPM", riffBPM ) );
                            }
                        }
                        break;

                        case JamVisualisation::LineBreakOn::ChangedScaleOrRoot:
                        {
                            if ( riffI == 0 ||
                                lastRoot != riffRoot ||
                                lastScale != riffScale )
                            {
                                addLineBreak( fmt::format( "{} ({})",
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
                switch ( jamViz.m_riffGapOn )
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

                switch ( jamViz.m_colourMode )
                {
                    case JamVisualisation::ColouringMode::Uniform:
                        // override later
                        break;

                    case JamVisualisation::ColouringMode::StemOwnership:
                    {
                        const auto& stemNameHashes = slice.m_stemUserHashes[riffI];
                        for ( auto stemI = 0; stemI < 8; stemI++ )
                        {
                            // increment towards 1.0 if all stems are us
                            if ( stemNameHashes[stemI] == vizUserHighlight1.m_nameHash )
                                colourT += 0.125f;
                        }
                    }
                    break;

                    case JamVisualisation::ColouringMode::UserIdentity:
                    {
                        if ( m_jamViewRenderUserHashFMap.contains( userHash ) )
                        {
                            colourT = m_jamViewRenderUserHashFMap.at( userHash );
                        }
                        else
                        {
                            m_jamViewRenderUserHashFMap.emplace( userHash, runningColourV );
                            runningColourV += 0.441f;
                            runningColourV = base::fract( runningColourV );

                            colourT = runningColourV;
                        }
                    }
                    break;

                    case JamVisualisation::ColouringMode::UserChangeRate:
                    {
                        if ( riffI > 0 && userHash != lastUserHash )
                            runningColourV = std::clamp( runningColourV + 0.15f, 0.0f, 0.999f );
                        else
                            runningColourV *= jamViz.m_changeRateDecay;

                        colourT = runningColourV;
                    }
                    break;

                    case JamVisualisation::ColouringMode::StemChurn:
                    {
                        static constexpr float cStemDeltaRecpF = 1.0f / 8.0f;
                        colourT = static_cast<float>(slice.m_deltaStem[riffI]) * cStemDeltaRecpF;
                    }
                    break;

                    case JamVisualisation::ColouringMode::StemTimestamp:
                    {
                        const auto stemTimestamp    = slice.m_timestamps[riffI];

                        // compute a timestamp just including the hours for this stem
                        const auto dateDays         = date::floor<date::days>( stemTimestamp );
                        const auto dateJustHours    = date::make_time( std::chrono::duration_cast<std::chrono::milliseconds>( stemTimestamp - dateDays ) );

                        // ( 1 / 24 (hrs) ) * pi giving us a day/night cycle between 0..1..0
                        colourT = std::sin( static_cast<float>( dateJustHours.hours().count() ) * 0.130899693899574718269f );
                    }
                    break;

                    case JamVisualisation::ColouringMode::Scale:
                    {
                        static constexpr float cScaleCountRecpF = 1.0f / static_cast<float>( endlesss::constants::cScaleNames.size() - 1 );
                        colourT = static_cast<float>( slice.m_scales[riffI] ) * cScaleCountRecpF;
                    }
                    break;

                    case JamVisualisation::ColouringMode::Root:
                    {
                        static constexpr float cRootCountRecpF = 1.0f / static_cast<float>( endlesss::constants::cRootNames.size() - 1 );
                        colourT = static_cast<float>( slice.m_roots[riffI] ) * cRootCountRecpF;
                    }
                    break;

                    case JamVisualisation::ColouringMode::BPM:
                    {
                        const float shiftRate = (float)std::clamp(
                            slice.m_bpms[riffI] - jamViz.m_bpmMinimum,
                            0.0f,
                            jamViz.m_bpmMaximum ) / (jamViz.m_bpmMaximum - jamViz.m_bpmMinimum);

                        colourT = shiftRate;
                    }
                    break;
                }

                const bool bActiveUserHighlight1 = vizUserHighlight1.m_active && ( vizUserHighlight1.m_nameHash == slice.m_userhash[riffI] );
                const bool bActiveUserHighlight2 = vizUserHighlight2.m_active && ( vizUserHighlight2.m_nameHash == slice.m_userhash[riffI] );

                // #todo
                if ( bActiveUserHighlight2 )
                    sightlineRowColour = vizUserHighlight2.m_highlightColour;
                if ( bActiveUserHighlight1 )
                    sightlineRowColour = vizUserHighlight1.m_highlightColour;


                // sample from the gradient, or override with a uniform single manual choice
                auto cellColourF = jamViz.getHeatmapColourAtT( colourT );
                auto cellColour = cellColourF.bgrU32();
                if ( jamViz.m_colourMode == JamVisualisation::ColouringMode::Uniform )
                    cellColour = bgraUniformColourU32;


                lastUserHash = userHash;



                const auto cellRiffCouchID = slice.m_ids[riffI];
                m_cellIndexToRiff.try_emplace( cellIndex, cellRiffCouchID );
                m_cellIndexToSliceIndex.try_emplace( cellIndex, riffI );
                m_riffOrderLinear.emplace_back( cellRiffCouchID );


                const int32_t cellPixelX = cellX * riffCubeSize;
                const int32_t cellPixelY = cellY * riffCubeSize;
                const int32_t cellPixelFullY = fullCellY * riffCubeSize;

                m_riffToBitmapOffset.try_emplace( cellRiffCouchID, ImVec2{ (float)cellPixelX, (float)cellPixelFullY } );


                base::U32Buffer& activeBuffer = activeSketch->get();


                for ( auto cellWriteY = 0U; cellWriteY < riffCubeSize; cellWriteY++ )
                {
                    for ( auto cellWriteX = 0U; cellWriteX < riffCubeSize; cellWriteX++ )
                    {
                        auto cellWriteXMirroredX = ( riffCubeSize - 1 ) - cellWriteX;
                        auto cellWriteXMirroredY = ( riffCubeSize - 1 ) - cellWriteY;

                        const bool edge0 = (cellWriteX == 0 ||
                            cellWriteY == 0 ||
                            cellWriteX == riffCubeSize - 1 ||
                            cellWriteY == riffCubeSize - 1);
                        const bool edge1 = (cellWriteX == 1 ||
                            cellWriteY == 1 ||
                            cellWriteX == riffCubeSize - 2 ||
                            cellWriteY == riffCubeSize - 2);

                        const bool cornerTL = (cellWriteX + cellWriteY) <= riffCubeCorner;
                        const bool edgeTL   = (cellWriteX + cellWriteY) <= riffCubeCorner + 2;

                        const bool cornerBR = (cellWriteXMirroredX + cellWriteY) <= riffCubeCorner;
                        const bool edgeBR   = (cellWriteXMirroredX + cellWriteY) <= riffCubeCorner + 2;

                        if ( cornerTL )
                        {
                            if ( bActiveUserHighlight1 )
                            {
                                activeBuffer(
                                    cellPixelX + cellWriteX,
                                    cellPixelY + cellWriteY ) = vizUserHighlight1.m_highlightColour;
                            }
                        }
                        else if ( cornerBR && bActiveUserHighlight2 )
                        {
                            activeBuffer(
                                cellPixelX + cellWriteX,
                                cellPixelY + cellWriteY ) = vizUserHighlight2.m_highlightColour;
                        }
                        else if ( edgeTL && bActiveUserHighlight1 )
                        {
                        }
                        else if ( edgeBR && bActiveUserHighlight2 )
                        {
                        }
                        else if ( edge0 )
                        {
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

            m_jamViewFullHeight = fullCellY * riffCubeSize;

            // include the trailing row we might have finished on
            if ( cellX != 0 )
            {
                m_jamViewFullHeight += riffCubeSize;
                m_sightlineRowOn.emplace_back( sightlineRowColour );
            }

            commitCurrentPage();

            // build the sightline texture for rendering text to the scroll bar
            // this serves as a compact overview of jams of any size - eg. viewing where your riffs might be amongst
            // 40,000 others without randomly scrolling around looking for markers
            {
                const auto sightlineHeight     = static_cast<uint32_t>( m_jamViewFullHeight < viewBrowserHeight ? m_jamViewFullHeight : viewBrowserHeight );
                const auto sightlineHeightPow2 = base::nextPow2( sightlineHeight );

                gfx::DimensionsPow2 sightlineDim( 1, sightlineHeightPow2 );

                gfx::SketchBufferPtr sightlineBuffer = sketchbook.getBuffer( sightlineDim );
                sightlineBuffer->setExtents( { 1, sightlineHeight } );
                {
                    base::U32Buffer& sightlineMap = sightlineBuffer->get();

                    const uint32_t sightlineRowCount = static_cast<uint32_t>( m_sightlineRowOn.size() );
                    const double rowsPerPixel = (double)sightlineHeight / (double)sightlineRowCount;

                    uint32_t sightColour = 0;
                    double pixelD = 0;
                    uint32_t pixelI = 0;
                    for ( auto hmR = 0U; hmR < sightlineRowCount; hmR++ )
                    {
                        if ( m_sightlineRowOn[hmR] != 0 )
                            sightColour = m_sightlineRowOn[hmR];

                        pixelD += rowsPerPixel;

                        uint32_t newPixelI = static_cast<uint32_t>( pixelD );
                        if ( newPixelI != pixelI )
                        {
                            for ( auto hmY = pixelI; hmY < newPixelI; hmY++ )
                            {
                                sightlineMap( 0, hmY ) = sightColour;
                            }
                            pixelI = newPixelI;
                            sightColour = 0;
                        }
                    }
                }
                m_sightlineUpload = sketchbook.scheduleBufferUploadToGPU( std::move( sightlineBuffer ) );
            }

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
    static constexpr auto                       c_jamSliceRenderChangeDuration = std::chrono::seconds( 1 );
    spacetime::Moment                           m_jamSliceRenderChangePendingTimer;

    endlesss::toolkit::Warehouse::JamSlicePtr   m_jamSlice;
    JamSliceSketchPtr                           m_jamSliceSketch;


    using RiffTagMap = absl::flat_hash_map< endlesss::types::RiffCouchID, endlesss::types::RiffTag >;
    using RiffTagSet = absl::flat_hash_set< endlesss::types::RiffCouchID >;

    struct JamTaggingState
    {
        endlesss::types::JamCouchID                 jamID;
        RiffTagSet                                  tagSet;    // simplified set of 'does this riff ID have a tag'
        std::vector< endlesss::types::RiffTag >     tagVector; // flat data array returned from db query; kept around to try and reuse the memory

        void clear()
        {
            jamID = {};
            tagSet.clear();
            tagVector.clear();
            blog::app( FMTX( "tag state cleared" ) );
        }

        void rebuildSet()
        {
            tagSet.clear();
            tagSet.reserve( tagVector.size() );
            for ( const auto& tag : tagVector )
            {
                tagSet.emplace( tag.m_riff );
            }
        }

        template<class Archive>
        void save( Archive& archive ) const
        {
            archive( CEREAL_NVP( jamID )
                   , CEREAL_NVP( tagVector )
            );
            blog::app( FMTX( "tag state saved | jam [{}] | {} riffs" ), jamID, tagVector.size() );
        }

        template<class Archive>
        void load( Archive& archive )
        {
            archive( CEREAL_NVP( jamID )
                   , CEREAL_NVP( tagVector )
            );
            rebuildSet();
            blog::app( FMTX( "tag state loaded | jam [{}] | {} riffs" ), jamID, tagVector.size() );
        }
    }                                           m_jamTagging;
    bool                                        m_jamTaggingInBatch = false;

    // optional operation to perform to rearrange jam tag vector in UI cycle
    struct JamTagVectorOp
    {
        enum class Op
        {
            Exchange,
            Move1After2,
            Move1Before2,
        };

        JamTagVectorOp() = delete;
        JamTagVectorOp( const Op op, int32_t index1, int32_t index2 )
            : m_op( op )
            , m_index1( index1 )
            , m_index2( index2 )
        {}

        Op          m_op;
        int32_t     m_index1;
        int32_t     m_index2;
    };
    using JamTagVectorOpToDo = std::optional< JamTagVectorOp >;


    endlesss::types::RiffCouchID                m_jamTaggingCurrentlyHovered;

    fs::path                                    m_jamTaggingSaveLoadDir;
    std::string                                 m_jamTagExportPrefix;

    base::EventListenerID                       m_eventLID_RequestNavigationToRiff  = base::EventListenerID::invalid();



    void beginChangeToViewJam( const endlesss::types::JamCouchID& jamID, const endlesss::types::RiffIdentity* optionalRiffScrollTo = nullptr  )
    {
        if ( isJamBeingSynced( jamID ) )
            return;

        const auto jamChangeIndex = m_warehouse->getChangeIndexForJam( jamID );

        if ( m_currentViewedJam != jamID || jamChangeIndex != m_currentViewedJamChangeIndex )
        {
            clearJamSlice();

            // change which jam we're viewing, reset active riff hover in the process as this will invalidate it
            m_currentViewedJam              = jamID;
            m_currentViewedJamChangeIndex   = jamChangeIndex;

            blog::database( FMTX( "new jam view setup, id {} with cI:[{}]" ), m_currentViewedJam, m_currentViewedJamChangeIndex.get() );

            m_jamSliceHoveredRiffIndex = -1;

            const auto lookupResult = lookupJamName( m_currentViewedJam, m_currentViewedJamName );
            if ( lookupResult == endlesss::services::IJamNameResolveService::LookupResult::NotFound )
            {
                m_currentViewedJamName = "[ Unknown ]";
            }

            m_warehouse->addJamSliceRequest( m_currentViewedJam, [this](
                const endlesss::types::JamCouchID& jamCouchID,
                endlesss::toolkit::Warehouse::JamSlicePtr&& resultSlice )
                {
                    newJamSliceGenerated( jamCouchID, std::move( resultSlice ) );
                });
        }

        // we have a riff to scroll to once the view is built
        m_currentViewedJamScrollToRiff = std::nullopt;
        if ( optionalRiffScrollTo != nullptr )
        {
            m_currentViewedJamScrollToRiff = *optionalRiffScrollTo;
        }

        // force-switch over to make the jam view in focus
        ImGui::MakeTabVisible( "###jam_view" );
    }

    void clearJamSlice()
    {
        std::scoped_lock<std::mutex> sliceLock( m_jamSliceMapLock );

        m_jamSlice              = nullptr;
        m_jamSliceRenderState   = JamSliceRenderState::Invalidated;
        m_jamSliceSketch        = nullptr;

        // reset any latent scroll-to requests
        m_currentViewedJamScrollToRiff = std::nullopt;

        // purge the tag data
        m_jamTagging.clear();
    }

    void fetchAndUpdateTagsForCurrentJam( const endlesss::types::JamCouchID jamCouchID )
    {
        // fetch the tags for this jam
        m_jamTagging.clear();
        m_jamTagging.jamID = jamCouchID;
        ABSL_ASSERT( !jamCouchID.empty() );

        const auto tagCount = m_warehouse->fetchTagsForJam( m_jamTagging.jamID, m_jamTagging.tagVector );
        m_jamTagging.rebuildSet();

        blog::app( FMTX( "fetched {} tags for jam {} from warehouse" ), tagCount, m_jamTagging.jamID );
    }

    void newJamSliceGenerated(
        const endlesss::types::JamCouchID& jamCouchID,
        endlesss::toolkit::Warehouse::JamSlicePtr&& resultSlice )
    {
        std::scoped_lock<std::mutex> sliceLock( m_jamSliceMapLock );

        m_jamSlice              = std::move( resultSlice );
        m_jamSliceRenderState   = JamSliceRenderState::Invalidated;
        m_jamSliceSketch        = nullptr;

        m_jamSliceRenderChangePendingTimer.setToFuture( c_jamSliceRenderChangeDuration );

        fetchAndUpdateTagsForCurrentJam( jamCouchID );
    }

    void warehouseCallbackTagBatching( bool bBatchUpdateBegun )
    {
        m_jamTaggingInBatch = bBatchUpdateBegun;

        // finished a batch update, rebuild
        if ( !m_jamTaggingInBatch )
        {
            fetchAndUpdateTagsForCurrentJam( m_jamTagging.jamID );
        }
    }

    void warehouseCallbackTagUpdate( const endlesss::types::RiffTag& updatedTag )
    {
        if ( m_jamTaggingInBatch )
            return;

        if ( updatedTag.m_jam == m_jamTagging.jamID )
        {
            // insert or ignore the riff ID in the tag set
            m_jamTagging.tagSet.emplace( updatedTag.m_riff );

            bool bWasUpdated = false;
            // find and update existing tag
            for ( std::size_t idx = 0; idx < m_jamTagging.tagVector.size(); idx++ )
            {
                if ( m_jamTagging.tagVector[idx].m_riff == updatedTag.m_riff )
                {
                    m_jamTagging.tagVector[idx] = updatedTag;
                    bWasUpdated = true;
                    break;
                }
            }

            // didn't find existing tag, assume this is a new insert
            if ( !bWasUpdated )
            {
                m_jamTagging.tagVector.push_back( updatedTag );
            }
        }
    }

    void warehouseCallbackTagRemoved( const endlesss::types::RiffCouchID& tagRiffID )
    {
        if ( m_jamTaggingInBatch )
            return;

        m_jamTagging.tagSet.erase( tagRiffID );
        base::erase_where( m_jamTagging.tagVector, [&]( const endlesss::types::RiffTag& tag ) { return tag.m_riff == tagRiffID; } );
    }



    void event_RequestNavigationToRiff( const events::RequestNavigationToRiff* eventData )
    {
        // get the jam we need to display to begin looking for the riff requested
        const auto jamToNavigateTo = eventData->m_identity.getJamID();

        // can't navigate to a in-flux jam, bail out early with a message
        if ( isJamBeingSynced( jamToNavigateTo ) )
        {
            m_appEventBus->send<::events::AddErrorPopup>(
                "Cannot View Jam",
                "Cannot view this jam currently, it is being synchronised. Try again later."
            );

            return;
        }


        // go look up a display name for the jam in question; this should always resolve if the nav request came
        // from any of our UI tools as they automatically resolve and store unknown jam name results upfront
        std::string jamName;
        const LookupResult jamNameLookup = lookupJamName( jamToNavigateTo, jamName );
        if ( jamNameLookup == endlesss::services::IJamNameResolveService::LookupResult::NotFound )
        {
            jamName = "Unknown Jam";
        }
        // regardless of sync check below, we upsert the jam name into internal warehouse lookup; it needs it set so when we
        // pull riff data out of the warehouse we can also resolve its origin jam name (rather than interrogating the app / BNS)
        // .. doing it early/often ensures we have an up-to-date record, just in case
        m_warehouse->upsertSingleJamIDToName( jamToNavigateTo, jamName );


        const auto haveSyncedJamIt = m_warehouseContentsReportJamIDs.find( jamToNavigateTo );
        if ( haveSyncedJamIt == m_warehouseContentsReportJamIDs.end() )
        {
            activateModalPopup( "Sync Missing Jam?", [this, jamToNavigateTo, jamNameResolved = std::move( jamName ), netCfg = getNetworkConfiguration()]( const char* title )
            {
                static endlesss::types::JamCouchID lastValidatedJamCouchID;

                const ImVec2 configWindowSize = ImVec2( 620.0f, 250.0f );
                ImGui::SetNextWindowContentSize( configWindowSize );

                if ( ImGui::BeginPopupModal( title, nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize ) )
                {
                    const ImVec2 buttonSize( 240.0f, 32.0f );

                    const bool bHasFullAccess = netCfg->hasAccess( endlesss::api::NetConfiguration::Access::Authenticated );


                    ImGui::PushFont( m_mdFrontEnd->getFont( app::module::Frontend::FontChoice::MediumTitle ) );
                    ImGui::TextUnformatted( jamNameResolved );
                    ImGui::PopFont();
                    ImGui::Separator();

                    // describe the process
                    if ( bHasFullAccess )
                    {
                        ImGui::TextWrapped( "To jump to the requested riff, we need to first download the jam it originated in. First validate you have permissions to do that; once successfully validated, the jam can be added to the warehouse and will begin downloading.\n\nOnce fully complete, re-try jumping to the chosen riff." );
                    }
                    else
                    {
                        ImGui::TextWrapped( "Cannot synchronise this jam without network authentication." );
                    }
                    ImGui::Spacing();
                    
                    const auto panelRegionAvail = ImGui::GetContentRegionAvail();
                    {
                        const float alignButtonsToBase = panelRegionAvail.y - (buttonSize.y + 6.0f);
                        ImGui::Dummy( ImVec2( 0, alignButtonsToBase ) );
                    }

                    if ( ImGui::Button( "Cancel", buttonSize ) )
                        ImGui::CloseCurrentPopup();

                    if ( bHasFullAccess )
                    {
                        ImGui::SameLine( 0, panelRegionAvail.x - (buttonSize.x * 2.0f) );

                        if ( lastValidatedJamCouchID == jamToNavigateTo )
                        {
                            if ( ImGui::Button( ICON_FA_CIRCLE_PLUS " Add Jam to Warehouse ...", buttonSize ) )
                            {
                                m_warehouseContentsReportJamInFluxSet.emplace( jamToNavigateTo ); // immediately add to the current in-flux set to represent things are in play

                                const auto updateOperationID = m_warehouse->addOrUpdateJamSnapshot( jamToNavigateTo );
                                addOperationToJam( jamToNavigateTo, updateOperationID );

                                ImGui::CloseCurrentPopup();
                            }
                        }
                        else
                        {
                            if ( ImGui::Button( "Validate Permissions ...", buttonSize ) )
                            {
                                // test-run access to whatever they're asking for to avoid adding garbage to the warehouse
                                endlesss::api::JamLatestState testPermissions;
                                if ( testPermissions.fetch( *netCfg, jamToNavigateTo ) )
                                {
                                    lastValidatedJamCouchID = jamToNavigateTo;
                                }
                                else
                                {
                                    lastValidatedJamCouchID = {};
                                }
                            }
                        }
                    }

                    ImGui::EndPopup();
                }
            });
        }
        else
        {
            beginChangeToViewJam( jamToNavigateTo, &eventData->m_identity );
        }
    }
    

    void notifyForRenderUpdate()
    {
        if ( m_jamSliceRenderState == JamSliceRenderState::Invalidated )
            return;

        m_jamSliceRenderChangePendingTimer.setToFuture( c_jamSliceRenderChangeDuration );
        m_jamSliceRenderState = JamSliceRenderState::PendingUpdate;
    }

    bool isRenderUpdatePending()
    {
        return ( m_jamSliceRenderState == JamSliceRenderState::PendingUpdate );
    }

    void updateJamSliceRendering()
    {
        std::scoped_lock<std::mutex> sliceLock( m_jamSliceMapLock );

        if ( !m_jamViewDimensions.isValid() )
            return;

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
                m_jamSliceSketch->raster( *m_sketchbook, m_jamVisualisation, m_jamViewDimensions, m_jamViewBrowserHeight );

                m_jamSliceRenderState = JamSliceRenderState::Ready;
            }
            break;

            default:
            case JamSliceRenderState::Ready:
                break;

            case JamSliceRenderState::PendingUpdate:
            {
                if ( m_jamSliceRenderChangePendingTimer.hasPassed() )
                {
                    m_jamSliceSketch->raster( *m_sketchbook, m_jamVisualisation, m_jamViewDimensions, m_jamViewBrowserHeight );
                    m_jamSliceRenderState = JamSliceRenderState::Ready;
                }
            }
            break;
        }
    }

protected:

    net::bond::RiffPushClient   m_rpClient;


// TagLineToolProvider, add the BOND push tool
protected:

    enum TagExtraTools : uint8_t
    {
        SendViaBOND = ux::TagLineToolProvider::BuiltItToolIdTop,

        TagExtraToolsCount
    };

    uint8_t getToolCount() const override
    {
        if ( m_rpClient.getState() == net::bond::Connected )
            return TagExtraToolsCount;

        return ux::TagLineToolProvider::getToolCount();
    }

    const char* getToolIcon( const ToolID id, std::string& tooltip ) const override
    {
        if ( static_cast<TagExtraTools>(id) == SendViaBOND )
        {
            tooltip = "Push current riff to BOND server";
            return ICON_FA_CIRCLE_NODES;
        }

        return ux::TagLineToolProvider::getToolIcon( id, tooltip);
    }

    bool checkToolKeyboardShortcut( const ToolID id ) const override
    {
        if ( static_cast<TagExtraTools>(id) == SendViaBOND )
            return ImGui::Shortcut( ImGuiModFlags_Ctrl, ImGuiKey_B, false );

        return false;
    }

    void handleToolExecution( const ToolID id, base::EventBusClient& eventBusClient, endlesss::live::RiffPtr& currentRiffPtr ) override
    {
        if ( static_cast<TagExtraTools>(id) == SendViaBOND )
        {
            m_rpClient.pushRiff( currentRiffPtr->m_riffData, m_riffPlaybackAbstraction.asPermutation() );

            m_appEventBus->send<::events::AddToastNotification>( ::events::AddToastNotification::Type::Info,
                ICON_FA_CIRCLE_NODES " Riff Pushed To Server",
                currentRiffPtr->m_uiDetails );

            return;
        }

        ux::TagLineToolProvider::handleToolExecution( id, eventBusClient, currentRiffPtr );
    }


private:

#if OURO_DEBUG

    bool addDeveloperMenuItems() override
    {
        if ( ImGui::MenuItem( "Download All Shared Riffs" ) )
        {
            getTaskExecutor().silent_async( [this]()
                {
                    endlesss::toolkit::Shares shareCache;

                    config::endlesss::PopulationGlobalUsers populationData;
                    const auto dataLoad = config::load( *this, populationData );
                    if ( dataLoad == config::LoadResult::Success )
                    {
                        for ( const auto& username : populationData.users )
                        {
                            getTaskExecutor().run( shareCache.taskFetchLatest(
                                *m_networkConfiguration,
                                username,
                                [this, username]( endlesss::toolkit::Shares::StatusOrData newData )
                                {
                                    if ( newData.ok() )
                                    {
                                        const auto savePath = fmt::format( FMTX( "E:\\Dev\\Keybase\\Archivissst\\archivissst\\Data\\_per_user_shared\\{}.json" ), username );

                                        std::ofstream is( savePath );
                                        cereal::JSONOutputArchive archive( is );

                                        (*newData)->serialize( archive );

                                        blog::app( FMTX( "Saved {} shares : {}" ), (*newData)->m_count, username );
                                    }
                                    else
                                    {
                                        blog::error::app( FMTX( "No shares : {}" ), username );
                                    }
                                } )
                            );
                        }

                        getTaskExecutor().wait_for_all();
                    }
                });
        }

        return true;
    }

#endif // OURO_DEBUG

};


// =====================================================================================================================


// ---------------------------------------------------------------------------------------------------------------------
int LoreApp::EntrypointOuro()
{
    // create a lifetime-tracked provider token to pass to systems that want access to Riff-fetching abilities we provide
    // the app instance will outlive all those systems; this slightly convoluted process is there to double-check that assertion
    endlesss::services::RiffFetchInstance riffFetchService( this );
    endlesss::services::RiffFetchProvider riffFetchProvider = riffFetchService.makeBound();

    // .. and same for jam-name resolution calls
    endlesss::services::JamNameResolveInstance JamNameResolveService( this );
    endlesss::services::JamNameResolveProvider JamNameResolveProvider = JamNameResolveService.makeBound();




    m_warehouse->setCallbackWorkReport( [this](const bool tasksRunning, const std::string& currentTask)
        {
            this->handleWarehouseWorkUpdate( tasksRunning, currentTask );
        });
    m_warehouse->setCallbackContentsReport( [this]( const endlesss::toolkit::Warehouse::ContentsReport& report )
        {
            this->handleWarehouseContentsReport( report );
        });

    m_warehouse->setCallbackTagUpdate(
        [this]( const endlesss::types::RiffTag& updatedTag ) { this->warehouseCallbackTagUpdate( updatedTag ); },
        [this]( const bool bBatchUpdateBegun ) { this->warehouseCallbackTagBatching( bBatchUpdateBegun ); }
    );
    m_warehouse->setCallbackTagRemoved( [this]( const endlesss::types::RiffCouchID& tagRiffID )
        { 
            this->warehouseCallbackTagRemoved( tagRiffID );
        });


    WarehouseJamBrowserBehaviour warehouseJamBrowser;
    warehouseJamBrowser.fnIsDisabled = [&warehouseJamBrowser]( const endlesss::types::JamCouchID& newJamCID )
    {
        return warehouseJamBrowser.m_warehouseJamIDs.contains( newJamCID );
    };
    warehouseJamBrowser.fnOnSelected = [this]( const endlesss::types::JamCouchID& newJamCID )
    {
        m_warehouseContentsReportJamInFluxSet.emplace( newJamCID ); // immediately add to the current in-flux set to represent things are in play
        
        const auto updateOperationID = m_warehouse->addOrUpdateJamSnapshot( newJamCID );
        addOperationToJam( newJamCID, updateOperationID );
    };



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


    const bool bHasValidEndlesssAuth = m_networkConfiguration->hasAccess( endlesss::api::NetConfiguration::Access::Authenticated );

    // default to viewing logged-in user in the jam view highlight
    if ( bHasValidEndlesssAuth )
    {
        m_jamVisualisation.m_userHighlight1.m_user.setUsername( m_networkConfiguration->auth().user_id );
    }

    if ( m_configPerf.enableVibesRenderer )
    {
        m_vibes = std::make_unique<vx::Vibes>();
        if ( const auto vibeStatus = m_vibes->initialize( *this ); !vibeStatus.ok() )
        {
            blog::error::app( FMTX( "Bad vibes : {}" ), vibeStatus.ToString() );
        }
    }

    m_sketchbook = std::make_unique<gfx::Sketchbook>();

    m_uxSharedRiffView  = std::make_unique<ux::SharedRiffView>( m_networkConfiguration, getEventBusClient() );
    m_uxRiffHistory     = std::make_unique<ux::RiffHistory>( getEventBusClient() );
    m_uxTagLine         = std::make_unique<ux::TagLine>( getEventBusClient() );
    m_uxProcWeaver      = std::make_unique<ux::Weaver>( *this, getEventBusClient() );


    m_jamTaggingSaveLoadDir = m_storagePaths->outputApp;

    // add developer menu entry for stem analysis flag
    addDeveloperMenuFlag( "Stem Analysis", &m_showStemAnalysis );


    // create and install the mixer engine
    mix::Preview mixPreview(
        m_mdAudio->getMaximumBufferSize(),
        m_mdAudio->getSampleRate(),
        m_mdAudio->getOutputLatencyMs(),
        m_appEventBusClient.value() );
    m_mdAudio->blockUntil( m_mdAudio->installMixer( &mixPreview ) );

    // LINK controls for the mixer
    registerMainMenuEntry( 20, "LINK", [&mixPreview]()
        {
            static bool LinkEnableFlag = false;
            if ( ImGui::MenuItem( "Ableton Link", nullptr, &LinkEnableFlag  ) )
            {
                blog::app( FMTX( "Requesting LINK state : {}" ), LinkEnableFlag ? "enabled" : "disabled" );
                mixPreview.enableAbletonLink( LinkEnableFlag );
            }
        });

    {
        base::EventBusClient m_eventBusClient( m_appEventBus );

        APP_EVENT_BIND_TO( PanicStop );
        APP_EVENT_BIND_TO( MixerRiffChange );
        APP_EVENT_BIND_TO( OperationComplete );
        APP_EVENT_BIND_TO( BNSWasUpdated );
        APP_EVENT_BIND_TO( RequestNavigationToRiff );
    }

    registerMainMenuEntry( 10, "BOND", [this]()
        {
            if ( ImGui::BeginMenu( "Riff Push" ) )
            {
                if ( ImGui::MenuItem( "Connection ..." ) )
                {
                    activateModalPopup( "Riff Push Connection", [this]( const char* title )
                    {
                        modalRiffPushClientConnection( title, m_mdFrontEnd, m_rpClient );
                    });
                }

                ImGui::EndMenu();
            }
        });


#if OURO_FEATURE_VST24
    // VSTs for audio engine
    m_effectStack = std::make_unique<effect::EffectStack>( m_mdAudio.get(), mixPreview.getTimeInfoPtr(), "preview" );
    m_effectStack->load( m_appConfigPath );
#endif // OURO_FEATURE_VST24


    m_riffPipeline = std::make_unique< endlesss::toolkit::Pipeline >(
        m_appEventBus,
        riffFetchProvider,
        m_configPerf.liveRiffInstancePoolSize,
        [this]( const endlesss::types::RiffIdentity& request, endlesss::types::RiffComplete& result) -> bool
        {
            // most requests can be serviced direct from the DB
            if ( m_warehouse->fetchSingleRiffByID( request.getRiffID(), result ) )
            {
                ABSL_ASSERT( result.jam.couchID == request.getJamID() );

                endlesss::toolkit::Pipeline::applyRequestCustomNaming( request, result );
                return true;
            }

            return endlesss::toolkit::Pipeline::defaultNetworkResolver( *m_networkConfiguration, request, result );
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

    m_riffExportPipeline = std::make_unique< endlesss::toolkit::Pipeline >(
        m_appEventBus,
        riffFetchProvider,
        0, // no internal cache - we don't want riffs saved as we can modify jam/riff descriptions during batch exports which would then be ignored
        [this]( const endlesss::types::RiffIdentity& request, endlesss::types::RiffComplete& result ) -> bool
        {
            // most requests can be serviced direct from the DB
            if ( m_warehouse->fetchSingleRiffByID( request.getRiffID(), result ) )
            {
                endlesss::toolkit::Pipeline::applyRequestCustomNaming( request, result );
                return true;
            }

            return endlesss::toolkit::Pipeline::defaultNetworkResolver( *m_networkConfiguration, request, result );
        },
        [this]( const endlesss::types::RiffIdentity& request, endlesss::live::RiffPtr& loadedRiff, const endlesss::types::RiffPlaybackPermutationOpt& )
        {
            ::events::ExportRiff exportRiffData( loadedRiff, {} );
            event_ExportRiff( &exportRiffData );
        },
        []()
        {
        });

    m_eventListenerRiffEnqueue = m_appEventBus->addListener( events::EnqueueRiffPlayback::ID, [this]( const base::IEvent& evt ) { onEvent_EnqueueRiffPlayback( evt ); } );



    // UI core loop begins
    while ( beginInterfaceLayout( (app::CoreGUI::ViewportFlags)(
        app::CoreGUI::VF_WithDocking   |
        app::CoreGUI::VF_WithMainMenu  |
        app::CoreGUI::VF_WithStatusBar ) ) )
    {
        base::instr::ScopedEvent wte( "Lore::UI", base::instr::PresetColour::Orange );

        // run jam slice computation that needs to run on the main thread
        m_sketchbook->processPendingUploads();

        // tidy up any messages from our riff-sync background thread
        synchroniseRiffWork();



        // for rendering state from the current riff;
        // take a shared ptr copy here, just in case the riff is swapped out mid-tick
        endlesss::live::RiffPtr currentRiffPtr = m_nowPlayingRiff;
        const auto currentRiff = currentRiffPtr.get();

        // optionally pop the stem debug analysis
        if ( m_showStemAnalysis )
        {
            ImGui::ux::StemAnalysis( currentRiffPtr, m_mdAudio->getSampleRate() );
        }

        {
            // process and blank out Exchange data ready to re-write it
            emitAndClearExchangeData();

            const auto currentPermutation = mixPreview.getCurrentPermutation();
            encodeExchangeData(
                currentRiffPtr,
                m_currentViewedJamName.c_str(),
                (uint64_t)mixPreview.getTimeInfoPtr()->samplePos,
                &currentPermutation );
        }


        {
            if ( ImGui::Begin( ICON_FA_MUSIC " Audio Output###audio_output" ) )
            {
                // expose gain control
                {
                    float gainF = m_mdAudio->getOutputSignalGain();
                    if ( ImGui::KnobFloat(
                        "##mix_gain",
                        24.0f,
                        &gainF,
                        0.0f,
                        1.0f,
                        2000.0f,
                        0.5f,
                        // custom tooltip showing dB instead of 0..1
                        []( const float percentage01, const float value ) -> std::string
                        {
                            if ( percentage01 <= std::numeric_limits<float>::min() )
                                return "-INF";

                            const auto dB = cycfi::q::lin_to_db( percentage01 );
                            return fmt::format( FMTX( "{:.2f} dB" ), dB.rep );
                        }))
                    {
                        m_mdAudio->setOutputSignalGain( gainF );
                    }
                }
                ImGui::SameLine( 0, 8.0f );

                // button to toggle end-chain mute on audio engine (but leave processing, WAV output etc intact)
                const bool isMuted = m_mdAudio->isMuted();
                {
                    {
                        ImGui::Scoped::ToggleButton bypassOn( isMuted );
                        if ( ImGui::Button( ICON_FA_VOLUME_XMARK, ImVec2( 48.0f, 48.0f ) ) )
                        {
                            m_mdAudio->toggleMute();
                        }
                    }
                    ImGui::CompactTooltip( "Mute final audio output\nThis does not affect streaming or disk-recording" );
                }
                ImGui::SameLine( 0, 8.0f );

                {
                    ImGui::Scoped::ToggleButton isClearing( m_riffPipelineClearInProgress );
                    if ( ImGui::Button( ICON_FA_BAN, ImVec2( 48.0f, 48.0f ) ) )
                    {
                        m_appEventBus->send< ::events::PanicStop >();
                    }
                    ImGui::CompactTooltip( "Panic stop all playback, buffering, pre-fetching, etc" );
                }

                ImGui::SameLine( 0, 8.0f );
                if ( ImGui::BeginChild( "disk-recorders", ImVec2( 210.0f, 0 ) ) )
                {
                    ux::widget::DiskRecorder( *m_mdAudio, m_storagePaths->outputApp );

                    auto* mixRecordable = mixPreview.getRecordable();
                    if ( mixRecordable != nullptr )
                        ux::widget::DiskRecorder( *mixRecordable, m_storagePaths->outputApp );
                }
                ImGui::EndChild();

                ImGui::SameLine();

                if ( ImGui::BeginChild( "beat-box" ) )
                {
                    ux::StemBeats( "##stem_beat", m_endlesssExchange, 18.0f, false );
                }
                ImGui::EndChild();
            }
            ImGui::End();
        }
        {
#if OURO_FEATURE_VST24
            if ( ImGui::Begin( ICON_FA_HURRICANE " Effect Chain###effect_chain" ) )
            {
                m_effectStack->imgui( *this );
            }
            ImGui::End();
#endif // OURO_FEATURE_VST24
        }

        if ( m_vibes )
            m_vibes->doImGui( m_endlesssExchange, this );

        m_mdMidi->processMessages( []( const app::midi::Message& ){ } );

        m_discordBotUI->imgui( *this );
        m_uxProcWeaver->imgui( *this, currentRiffPtr, m_rpClient, *m_warehouse );

        mixPreview.imgui();

        {

            if ( ImGui::Begin( ICON_FA_BARS " Riff Details###riff_details" ) )
            {
                m_uxTagLine->imgui( currentRiffPtr, m_warehouse.get(), this );

                ImGui::Spacing();

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
                    { "",           15.0f  }, // 0 [M]
                    { "",           15.0f  }, // 1 [S]
                    { "",           10.0f  }, // 2 [gain]
                    { "User",       140.0f }, // 3
                    { "Instr",      65.0f  }, // 4
                    { "Preset",     175.0f }, // 5
                    { "Format",     65.0f  }, // 6
                    { "Rate",       55.0f  }, // 7
                    { "Gain",       45.0f  }, // 8
                    { "Speed",      45.0f  }, // 9
                    { "Length/s",   65.0f  }, // 10
                    { "Size/KB",    60.0f  }  // 11
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
                        if ( cI == 1 )
                        {
                            // cheap little random scrambler, chooses some random mute states
                            if ( ImGui::Button( "!" ) )
                            {
                                math::RNG32 rng;

                                m_riffPlaybackAbstraction = {};

                                for ( auto i = 0; i < 8; i++ )
                                {
                                    if ( (rng.genUInt32() & 1) == 1 )
                                        m_riffPlaybackAbstraction.action( endlesss::types::RiffPlaybackAbstraction::Action::ToggleMute, i );
                                }

                                const auto newPermutation = m_riffPlaybackAbstraction.asPermutation();
                                const auto operationID = mixPreview.enqueuePermutation( newPermutation );
                                m_permutationOperationImGuiMap.add( operationID, 0 );
                            }
                            ImGui::CompactTooltip( "Choose random muted channels" );
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

                        const bool isRandomisationPassHappening = m_permutationOperationImGuiMap.hasValue( 0 );
                        if ( m_permutationOperationImGuiMap.hasValue( currentImGuiID ) || isRandomisationPassHappening )
                            buttonState = ImGui::Scoped::FluxButton::State::Flux;

                        ImGui::Scoped::Disabled se( isRandomisationPassHappening );
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
                            {
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted( "" );
                            }
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
                                    case 3:
                                    {
                                        ImGui::TextUnformatted( stem->m_data.user );
                                        break;
                                    }
                                    case 4:
                                    {
                                        ImGui::PushStyleColor( ImGuiCol_Text, stem->m_colourU32 );
                                        ImGui::TextUnformatted( ICON_FC_FULL_BLOCK );
                                        ImGui::PopStyleColor();
                                        ImGui::SameLine( 0, 6.0f );
                                        ImGui::TextUnformatted( stem->m_data.getInstrumentName() );
                                        break;
                                    }
                                    case 5:
                                    {
                                        ImGui::TextUnformatted( stem->m_data.preset );
                                        break;
                                    }
                                    case 6:
                                    {
                                        switch ( stem->getCompressionFormat() )
                                        {
                                            case endlesss::live::Stem::Compression::Unknown:
                                                break;
                                            case endlesss::live::Stem::Compression::OggVorbis:
                                                ImGui::PushStyleColor( ImGuiCol_Text, colour::shades::blue_gray.neutralU32() );
                                                ImGui::TextUnformatted( ICON_FA_CIRCLE " OggV" );
                                                ImGui::PopStyleColor();
                                                break;
                                            case endlesss::live::Stem::Compression::FLAC:
                                                ImGui::PushStyleColor( ImGuiCol_Text, colour::shades::sea_green.darkU32() );
                                                ImGui::TextUnformatted( ICON_FA_CIRCLE_UP " FLAC");
                                                ImGui::PopStyleColor();
                                                break;
                                        }
                                        if ( ImGui::IsItemClicked() )
                                        {
                                            ImGui::SetClipboardText( fmt::format( FMTX("https://{}/{}"), stem->m_data.fullEndpoint(), stem->m_data.fileKey ).c_str() );
                                        }
                                        break;
                                    }
                                    case 7:
                                    {
                                        ImGui::Text( "%i", stem->m_data.sampleRate );
                                        break;
                                    }
                                    case 8:
                                    {
                                        ImGui::Text( "%.2f", m_endlesssExchange.m_stemGain[sI] );
                                        break;
                                    }
                                    case 9:
                                    {
                                        ImGui::Text( "%.2f", currentRiff->m_stemTimeScales[sI] );
                                        break;
                                    }
                                    case 10:
                                    {
                                        ImGui::Text( "%.2f", currentRiff->m_stemLengthInSec[sI] );
                                        break;
                                    }
                                    case 11:
                                    {
                                        ImGui::Text( "%i", stem->m_data.fileLengthBytes / 1024 );
                                        if ( ImGui::IsItemClicked() )
                                        {
                                            ImGui::SetClipboardText( stem->m_data.couchID.value().c_str() );
                                        }
                                        ImGui::CompactTooltip( stem->m_data.couchID.value() );
                                        break;
                                    }
                                    default:
                                        ABSL_ASSERT( false );
                                        break;
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
                }
            }
            ImGui::End();
        }

        {
            static JamView jamView( JamView::Default );

            if ( ImGui::Begin( jamView.generateTitle().c_str() ) )
            {
                jamView.checkForImGuiTabSwitch();

                if ( m_currentViewedJamName.empty() || m_currentViewedJam.empty() )
                {
                    ImGui::TextColored( colour::shades::toast.light(), "No jam currently loaded" );
                    ImGui::TextColored( colour::shades::toast.neutral(), "Use the " ICON_FA_DATABASE " Data Warehouse view to select one" );
                }
                else
                {
                    if ( jamView == JamView::Visualisation )
                    {
                        const bool visOptionChanged = m_jamVisualisation.imgui( *this );
                        if ( visOptionChanged )
                        {
                            notifyForRenderUpdate();
                        }
                    }
                    else
                    {
                        // capture the zone we'll be working with - this also lets us track if the user is changing the
                        // view size which will lazily trigger a re-rendering of all the backing textures
                        const ImVec2 jamViewDimensions = ImVec2(
                            ImGui::GetContentRegionAvail().x - 20.0f,
                            ImGui::GetContentRegionAvail().y );

                        ImGui::PushFont( m_mdFrontEnd->getFont( app::module::Frontend::FontChoice::MediumTitle ) );
                        ImGui::TextUnformatted( m_currentViewedJamName.c_str() );
                        ImGui::PopFont();

                        // font used for labels in amongst the jam cubes
                        ImGui::PushFont( m_mdFrontEnd->getFont( app::module::Frontend::FontChoice::FixedSmaller ) );

                        // check to see if we need to rebuild the jam view on size change
                        // also pokes the main-gl-thread texture GPU syncing jobs
                        syncJamViewLayoutAndAsyncRendering( jamViewDimensions, static_cast<int32_t>( ImGui::GetContentRegionAvail().y ) );


                        const ImVec2 outerPos = ImGui::GetCursorPos();
                        const auto contentRegion = ImGui::GetContentRegionAvail();

                        bool shouldShowSightline = false;

                        if ( ImGui::BeginChild( "##jam_browser" ) )
                        {
                            const float jamGridCellF = static_cast<float>( m_jamVisualisation.getRiffCubeSize() );
                            const float jamGridCellHalf = jamGridCellF * 0.5f;
                            const ImVec2 jamGridCell( jamGridCellF, jamGridCellF );
                            const ImVec2 jamGridCellCenter( jamGridCellHalf, jamGridCellHalf );
                            const ImU32 jamCellColourPlaying = ImGui::GetColorU32( ImGuiCol_Text );
                            const ImU32 jamCellColourEnqueue = ImGui::GetColorU32( ImGuiCol_ChildBg, 0.8f );
                            const ImU32 jamCellColourLoading = ImGui::GetPulseColour();

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
                                    shouldShowSightline = !renderUpdatePending;

                                    ImVec2 pos = ImGui::GetCursorScreenPos();
                                    ImDrawList* draw_list = ImGui::GetWindowDrawList();
                                    ImGuiIO& io = ImGui::GetIO();

                                    gfx::GPUTask::ValidState textureState;

                                    for ( const auto& texture : m_jamSliceSketch->m_textures )
                                    {
                                        if ( texture->getStateIfValid( textureState ) )
                                        {
                                            const bool isTextureOnScreen = ImGui::Image(
                                                textureState.m_imTextureID,
                                                textureState.m_usageDimensionsVec2,
                                                ImVec2( 0, 0 ),
                                                textureState.m_usageUV,
                                                texturePageBlending );

                                            if ( isTextureOnScreen )
                                            {
                                                const auto mouseToCenterX = io.MousePos.x - pos.x;
                                                const auto mouseToCenterY = io.MousePos.y - pos.y;

                                                if ( ImGui::IsItemHovered() )
                                                {
                                                    const auto cellX = (int32_t)std::floor( mouseToCenterX / (float)m_jamVisualisation.getRiffCubeSize() );
                                                    const auto cellY = (int32_t)std::floor( mouseToCenterY / (float)m_jamVisualisation.getRiffCubeSize() );

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

                                                                const bool bRiffCurrentlyPlaying = (currentRiff && currentRiff->m_riffData.riff.couchID == riffCouchID);
                                                                if ( !bRiffCurrentlyPlaying )
                                                                    requestRiffPlayback( { m_currentViewedJam, riffCouchID }, m_riffPlaybackAbstraction.asPermutation() );
                                                            }
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


                                    // deal with scrolling to a specific riff, if one has been specified for us
                                    if ( m_currentViewedJamScrollToRiff.has_value() )
                                    {
                                        ABSL_ASSERT( m_currentViewedJamScrollToRiff->getJamID() == m_currentViewedJam );
                                        const auto riffToScrollTo = m_currentViewedJamScrollToRiff->getRiffID();

                                        // lookup the riff position from the bitmap offset table
                                        const auto& scrollToRectIt = m_jamSliceSketch->m_riffToBitmapOffset.find( riffToScrollTo );
                                        if ( scrollToRectIt != m_jamSliceSketch->m_riffToBitmapOffset.end() )
                                        {
                                            // overwrite any existing scroll-restoration, plug in new value that roughly centers the chosen riff
                                            m_jamSliceSketch->m_syncToUI = true;
                                            m_jamSliceSketch->m_currentScrollY = scrollToRectIt->second.y - ( contentRegion.y * 0.5f );
                                        }
                                        else
                                        {
                                            m_appEventBus->send<::events::AddErrorPopup>(
                                                "Unable to find rifff",
                                                "Could not navigate to riff; you may need to synchronise this jam to ensure it has the latest data available."
                                            );
                                        }

                                        m_currentViewedJamScrollToRiff = std::nullopt;
                                    }

                                    // when rebuilding the UI, we save the scroll position while UI elements are modified
                                    // so the user doesn't lose their position
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

                                    for ( const auto& riffTag : m_jamTagging.tagVector )
                                    {
                                        const auto& activeRectIt = m_jamSliceSketch->m_riffToBitmapOffset.find( riffTag.m_riff );
                                        if ( activeRectIt != m_jamSliceSketch->m_riffToBitmapOffset.end() )
                                        {
                                            ImVec2 rv2 = pos + activeRectIt->second;

                                            uint32_t favourColour = colour::shades::tag_lvl_1.neutralU32();
                                            if ( riffTag.m_favour == 1 )
                                                favourColour = colour::shades::tag_lvl_2.neutralU32();

                                            if ( !m_jamTaggingCurrentlyHovered.empty() && 
                                                  m_jamTaggingCurrentlyHovered == riffTag.m_riff )
                                            {
                                                favourColour = colour::shades::white.lightU32();
                                            }


                                            draw_list->AddTriangleFilled(
                                                rv2 + ImVec2( -2.0f, jamGridCell.y - 2.0f ),
                                                rv2 + ImVec2( jamGridCell.x * 0.5f, (jamGridCell.y * 0.7f) - 4.0f ),
                                                rv2 + ImVec2( jamGridCell.x + 2.0f, jamGridCell.y - 2.0f ),
                                                ImGui::GetColorU32( ImGuiCol_ChildBg ) );

                                            draw_list->AddTriangleFilled(
                                                rv2 + ImVec2( 2.0f, jamGridCell.y - 2.0f ),
                                                rv2 + ImVec2( jamGridCell.x * 0.5f, jamGridCell.y * 0.7f ),
                                                rv2 + ImVec2( jamGridCell.x - 2.0f, jamGridCell.y - 2.0f ),
                                                favourColour );
                                        }
                                    }


                                    if ( currentRiff )
                                    {
                                        const auto& activeRectIt = m_jamSliceSketch->m_riffToBitmapOffset.find( currentRiff->m_riffData.riff.couchID );
                                        if ( activeRectIt != m_jamSliceSketch->m_riffToBitmapOffset.end() )
                                        {
                                            const auto& riffRectXY = activeRectIt->second;
                                            draw_list->AddCircleFilled( pos + riffRectXY + jamGridCellCenter, jamGridCellHalf * 0.77f, colour::shades::black.neutralU32() );
                                            draw_list->AddNgonFilled( pos + riffRectXY + jamGridCellCenter, jamGridCellHalf * 0.68f, jamCellColourLoading, 3 );
                                        }
                                    }
                                    for ( const auto& riffEnqueue : m_riffsQueuedForPlayback )
                                    {
                                        const auto& activeRectIt = m_jamSliceSketch->m_riffToBitmapOffset.find( riffEnqueue );
                                        if ( activeRectIt != m_jamSliceSketch->m_riffToBitmapOffset.end() )
                                        {
                                            const auto& riffRectXY = activeRectIt->second;
                                            draw_list->AddCircleFilled( pos + riffRectXY + jamGridCellCenter, jamGridCellHalf * 0.77f, jamCellColourEnqueue );
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

                        if ( m_jamSliceSketch && shouldShowSightline )
                        {
                            ImDrawList* draw_list = ImGui::GetWindowDrawList();

                            const float regionTop = m_jamSliceSketch->m_currentScrollY;
                            const float regionBottom = regionTop + contentRegion.y;

                            const float fullHeightRecp = 1.0f / m_jamSliceSketch->m_jamViewFullHeight;

                            gfx::GPUTask::ValidState textureState;
                            if ( m_jamSliceSketch->m_sightlineUpload->getStateIfValid( textureState ) )
                            {
                                const auto savedCursor = ImGui::GetCursorPos();
                                ImGui::SetCursorPos( outerPos + ImVec2( contentRegion.x - 20.0f, 0 ) );
                                ImGui::Image(
                                    textureState.m_imTextureID,
                                    ImVec2( 5.0f, textureState.m_usageDimensionsVec2.y ),
                                    ImVec2( 0, 0 ),
                                    textureState.m_usageUV,
                                    ImVec4( 1.0f, 1.0f, 1.0f, 1.0f ) );
                            }
                        }

                        ImGui::PopFont();
                    }
                }
            }
            ImGui::End();
        }
        {
            static TagView tagView( TagView::Default );

            if ( ImGui::Begin( tagView.generateTitle().c_str() ) )
            {
                static ImVec2 buttonSizeTools( 200.0, 30.0f );
                static ImVec2 buttonSizeHeader( 200.0, ImGui::GetTextLineHeight() + (GImGui->Style.FramePadding.y * 2.0f) + 1.0f );

                tagView.checkForImGuiTabSwitch();

                // can't view tags without a jam!
                if ( m_currentViewedJamName.empty() || m_currentViewedJam.empty() )
                {
                    ImGui::TextColored( colour::shades::toast.light(), "No jam currently loaded" );
                    ImGui::TextColored( colour::shades::toast.neutral(), "Use the " ICON_FA_DATABASE " Data Warehouse view to select one" );
                }
                // mid-sync; we have a viewed jam and are just waiting on the Warehouse
                else if ( m_jamTagging.jamID.empty() )
                {
                    ImGui::TextColored( colour::shades::toast.light(), "Fetching tags ..." );
                }
                else
                {
                    ABSL_ASSERT( m_jamTagging.jamID == m_currentViewedJam );
                    if ( ImGui::BeginChild( "tag-title-align", ImVec2( -( (buttonSizeHeader.x * 2.0f) + GImGui->Style.WindowPadding.x), buttonSizeHeader.y ) ) )
                    {
                        ImGui::AlignTextToFramePadding();
                        ImGui::TextUnformatted( "Tagged Riffs in " );
                        ImGui::SameLine( 0, 0 );
                        ImGui::TextColored( colour::shades::toast.light(), "%s", m_currentViewedJamName.c_str());
                    }
                    ImGui::EndChild();

                    if ( tagView == TagView::Management )
                    {
                        enum class LoadTask
                        {
                            None,
                            LoadToReplace,
                            LoadToMerge
                        } loadTask = LoadTask::None;

                        ImGui::SeparatorBreak();
                        if ( ImGui::BeginTable( "###file_io", 4,
                            ImGuiTableFlags_NoSavedSettings ) )
                        {
                            ImGui::TableSetupColumn( "Save",  ImGuiTableColumnFlags_WidthStretch, 0.3f );
                            ImGui::TableSetupColumn( "Gap",   ImGuiTableColumnFlags_WidthStretch, 0.1f );
                            ImGui::TableSetupColumn( "Load",  ImGuiTableColumnFlags_WidthStretch, 0.3f );
                            ImGui::TableSetupColumn( "Merge", ImGuiTableColumnFlags_WidthStretch, 0.3f );

                            {
                                ImGui::TableNextColumn();
                                ImGui::TextColored( colour::shades::callout.neutral(), "SAVE" );
                                ImGui::Spacing();
                                ImGui::TextWrapped( "Save tags out to a file for backup or exchange with others. These are associated with the current jam and can be re-imported later or by other users who have access to the same jam." );
                                ImGui::Spacing();
                            }
                            {
                                ImGui::TableNextColumn();
                                // gap row
                            }
                            {
                                ImGui::TableNextColumn();
                                ImGui::TextColored( colour::shades::callout.neutral(), "LOAD (replace)" );
                                ImGui::Spacing();
                                ImGui::TextWrapped( "Overwrite the current tags in the database with new data loaded from a file. This will REPLACE your current data!" );
                                ImGui::Spacing();
                            }
                            {
                                ImGui::TableNextColumn();
                                ImGui::TextColored( colour::shades::callout.neutral(), "LOAD (merge)" );
                                ImGui::Spacing();
                                ImGui::TextWrapped( "Merge data from a tag file with your current data; your current data will remain intact, any conflicts in the incoming data are ignored." );
                                ImGui::Spacing();
                            }

                            ImGui::TableNextColumn();
                            {
                                ImGui::Scoped::Disabled sd( m_jamTagging.tagVector.empty() );
                                if ( ImGui::Button( ICON_FA_FILE_EXPORT " Save Tags ...", buttonSizeTools ) )
                                {
                                    auto fileDialog = std::make_unique< ImGuiFileDialog >();

                                    fileDialog->OpenDialog(
                                        "TagSaveDlg",
                                        "Choose File To Save Tags To ...",
                                        ".tags",
                                        m_jamTaggingSaveLoadDir.string(),
                                        1,
                                        nullptr,
                                        ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_ConfirmOverwrite );

                                    std::ignore = activateFileDialog( std::move( fileDialog ), [this]( ImGuiFileDialog& dlg )
                                        {
                                            if ( !dlg.IsOk() )
                                                return;

                                            const fs::path fullPathToWriteTo = dlg.GetFilePathName();
                                            m_jamTaggingSaveLoadDir = dlg.GetCurrentPath();

                                            try
                                            {
                                                std::ofstream is( fullPathToWriteTo, std::ofstream::out );
                                                is.exceptions( std::ofstream::failbit | std::ofstream::badbit );

                                                cereal::JSONOutputArchive archive( is );

                                                m_jamTagging.save( archive );
                                            }
                                            catch ( std::exception& cEx )
                                            {
                                                blog::error::app( "tag save failed; cannot write to [{}] | {}", fullPathToWriteTo.string(), cEx.what() );

                                                m_appEventBus->send<::events::AddErrorPopup>(
                                                    "Failed to Save Tags",
                                                    "Failed to write tags data to disk.\nEnsure file is not read-only or in use by another application."
                                                );
                                            }
                                        } );
                                }
                            }
                            ImGui::TableNextColumn();
                            {
                                // gap row
                            }
                            ImGui::TableNextColumn();
                            {
                                if ( ImGui::Button( ICON_FA_FILE_IMPORT " Replace Tags ...", buttonSizeTools ) )
                                {
                                    loadTask = LoadTask::LoadToReplace;
                                }
                            }
                            ImGui::TableNextColumn();
                            {
                                if ( ImGui::Button( ICON_FA_FILE_IMPORT " Merge Tags ...", buttonSizeTools ) )
                                {
                                    loadTask = LoadTask::LoadToMerge;
                                }
                            }
                            ImGui::EndTable();

                            if ( loadTask != LoadTask::None )
                            {
                                auto fileDialog = std::make_unique< ImGuiFileDialog >();

                                fileDialog->OpenDialog(
                                    "TagLoadDlg",
                                    "Choose File To Load Tags From ...",
                                    ".tags",
                                    m_jamTaggingSaveLoadDir.string(),
                                    1,
                                    nullptr,
                                    ImGuiFileDialogFlags_Modal );

                                std::ignore = activateFileDialog( std::move( fileDialog ), [this, loadTask]( ImGuiFileDialog& dlg )
                                    {
                                        if ( !dlg.IsOk() )
                                            return;

                                        const fs::path fullPathToReadFrom = dlg.GetFilePathName();
                                        m_jamTaggingSaveLoadDir = dlg.GetCurrentPath();

                                        try
                                        {
                                            std::ifstream is( fullPathToReadFrom );

                                            cereal::JSONInputArchive archive( is );

                                            JamTaggingState loadedState;
                                            loadedState.load( archive );

                                            // lock loading to our current viewed jam to avoid any confusion
                                            // technically we can load whatever but it makes more sense I think if they user
                                            // is looking at the jam they are importing for
                                            if ( loadedState.jamID != m_jamTagging.jamID )
                                            {
                                                std::string resolvedName;
                                                const auto lookupResult = lookupJamName( loadedState.jamID, resolvedName );

                                                m_appEventBus->send<::events::AddErrorPopup>(
                                                    "Failed to Load Tags",
                                                    fmt::format( FMTX( "The selected file contains tags for [{}] - please load that jam first if you want to import tags for it" ), resolvedName )
                                                );
                                            }
                                            else
                                            {
                                                if ( loadTask == LoadTask::LoadToReplace )
                                                {
                                                    m_warehouse->batchRemoveAllTags( m_jamTagging.jamID );
                                                    m_jamTagging = std::move( loadedState );
                                                    m_warehouse->batchUpdateTags( m_jamTagging.tagVector );
                                                }
                                                else
                                                {
                                                    for ( const auto& newTag : loadedState.tagVector )
                                                    {
                                                        if ( m_jamTagging.tagSet.contains( newTag.m_riff ) )
                                                        {
                                                            blog::error::app( "Skipped tagged riff during import : [{}]", newTag.m_riff );
                                                        }
                                                        else
                                                        {
                                                            m_jamTagging.tagVector.emplace_back( newTag );
                                                        }
                                                    }
                                                    m_jamTagging.rebuildSet();

                                                    // re-index everything
                                                    for ( int32_t newIndex = 0; newIndex < static_cast<int32_t>(m_jamTagging.tagVector.size()); newIndex++ )
                                                        m_jamTagging.tagVector[newIndex].m_order = newIndex;

                                                    // batch update everything
                                                    m_warehouse->batchUpdateTags( m_jamTagging.tagVector );
                                                }
                                            }
                                        }
                                        catch ( std::exception& cEx )
                                        {
                                            blog::error::app( "tag load failed; cannot load from [{}] | {}", fullPathToReadFrom.string(), cEx.what() );

                                            m_appEventBus->send<::events::AddErrorPopup>(
                                                "Failed to Load Tags",
                                                "Failed to load tags data to disk.\nFile may be corrupt or otherwise unreadable. Send it to ishani for analysis!"
                                            );
                                        }

                                    });
                            }
                        }
                        ImGui::Spacing();
                        ImGui::Spacing();
                        ImGui::SeparatorBreak();
                        ImGui::Spacing();
                        ImGui::Spacing();
                        {
                            ImGui::TextUnformatted( "Remove all tags from this jam. This cannot be undone." );
                            ImGui::Scoped::ColourButton cb( colour::shades::errors, colour::shades::white );
                            if ( ImGui::Button( ICON_FA_TRASH_CAN " Delete All", buttonSizeTools ) )
                            {
                                m_warehouse->batchRemoveAllTags( m_jamTagging.jamID );
                            }
                        }
                    }
                    else
                    {
                        // trim out any foolish characters from the export path string
                        struct TextFilters
                        {
                            static int FilterForFilename(ImGuiInputTextCallbackData* data)
                            {
                                // 0..9
                                if ( data->EventChar >= 48 && data->EventChar <= 57 )
                                    return 0;
                                // A..Z
                                if ( data->EventChar >= 65 && data->EventChar <= 90 )
                                    return 0;
                                // a..z
                                if ( data->EventChar >= 97 && data->EventChar <= 122 )
                                    return 0;
                                // _ or space
                                if ( data->EventChar == 95 && data->EventChar == 32 )
                                    return 0;

                                return 1;
                            }
                        };

                        // trigger an export of all the tagged riffs by slinging them into the export pipeline
                        ImGui::SameLine();
                        if ( ImGui::Button( ICON_FA_FLOPPY_DISK " Export All Tagged", buttonSizeHeader ) )
                        {
                            for ( const auto& riffTag : m_jamTagging.tagVector )
                            {
                                endlesss::types::IdentityCustomNaming customNaming;
                                customNaming.m_riffDescription = fmt::format( FMTX( "tag{:03}_f{}_" ), riffTag.m_order, riffTag.m_favour );

                                if ( !m_jamTagExportPrefix.empty() )
                                    customNaming.m_jamDescription = m_jamTagExportPrefix;

                                enqueueRiffForExport( { riffTag.m_jam, riffTag.m_riff, std::move( customNaming ) } );
                            }
                        }
                        // add option for additional subdirectory inserted between jam and riff stack when exporting
                        ImGui::SameLine();
                        ImGui::AlignTextToFramePadding();
                        ImGui::TextUnformatted( "into" );
                        ImGui::SameLine();
                        ImGui::TextDisabled( "[?]" );
                        ImGui::SameLine();
                        ImGui::CompactTooltip( "(optional) Additional export subdirectory name used during export" );
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth( buttonSizeHeader.x * 0.6f );
                        ImGui::InputText( "##custom_name", &m_jamTagExportPrefix, ImGuiInputTextFlags_CallbackCharFilter, TextFilters::FilterForFilename );
                        ImGui::SeparatorBreak();
                        ImGui::Spacing();
                        ImGui::Spacing();

                        static ImVec2 buttonSizeMidTable( 31.0f, 22.0f );

                        endlesss::types::RiffCouchID viewHoveredRiffID;
                        if ( m_jamSliceSketch && m_jamSliceHoveredRiffIndex >= 0 )
                            viewHoveredRiffID = m_jamSliceSketch->m_slice->m_ids[m_jamSliceHoveredRiffIndex];

                        endlesss::types::RiffCouchID viewCurrentPlayingRiffID;
                        if ( currentRiff )
                            viewCurrentPlayingRiffID = currentRiff->m_riffData.riff.couchID;


                        ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, { 4.0f, 0.0f } );

                        if ( ImGui::BeginTable( "##riff_tag_table", 6,
                            ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_Borders |
                            ImGuiTableFlags_RowBg   |
                            ImGuiTableFlags_NoSavedSettings ) )
                        {
                            ImGui::TableSetupScrollFreeze( 0, 1 );  // top row always visible

                            ImGui::TableSetupColumn( "Play",                    ImGuiTableColumnFlags_WidthFixed,   48.0f );
                            ImGui::TableSetupColumn( "Notes",                   ImGuiTableColumnFlags_WidthStretch, 0.5f  );
                            ImGui::TableSetupColumn( "Time",                    ImGuiTableColumnFlags_WidthFixed,   32.0f );
                            ImGui::TableSetupColumn( "Find",                    ImGuiTableColumnFlags_WidthFixed,   32.0f );
                            ImGui::TableSetupColumn( " " ICON_FA_FLOPPY_DISK,   ImGuiTableColumnFlags_WidthFixed,   32.0f );
                            ImGui::TableSetupColumn( "Order",                   ImGuiTableColumnFlags_WidthFixed,   86.0f );
                            ImGui::TableHeadersRow();

                            m_jamTaggingCurrentlyHovered = {};

                            int32_t riffEntry = 0;
                            const int32_t tagCount = static_cast<int32_t>(m_jamTagging.tagVector.size());

                            // optional operation to carry out on tag data after we've done iterating it, triggered by
                            // some kind of user interaction (like hitting up/down buttons, drag/dropping etc)
                            JamTagVectorOpToDo tagOperationToDo = std::nullopt;

                            for ( auto& riffTag : m_jamTagging.tagVector )
                            {
                                const auto& riffID = riffTag.m_riff;

                                ImGui::TableNextColumn();
                                ImGui::PushID( static_cast<int32_t>( riffEntry ) );

                                const bool bRiffIsHoveredInJamView  = viewHoveredRiffID == riffID;
                                const bool bRiffIsPlaying           = viewCurrentPlayingRiffID == riffID;
                                const bool bRiffWaitingToPlay       = m_riffsQueuedForPlayback.contains( riffID );
                                const bool bRiffIsExporting         = m_riffExportOperationsMap.hasValue( riffID );

                                if ( bRiffIsHoveredInJamView )
                                {
                                    ImVec4 highlightRow = ImGui::GetStyleColorVec4( ImGuiCol_TableRowBg );
                                    highlightRow.w *= 3.0f;

                                    ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg1, ImGui::ColorConvertFloat4ToU32( highlightRow ) );
                                }
                                if ( bRiffWaitingToPlay )
                                {
                                    ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg1, ImGui::GetPulseColour( 0.25f ) );
                                }

                                {
                                    ImGui::Scoped::Disabled disabledButton( bRiffWaitingToPlay );
                                    ImGui::Scoped::ToggleButton highlightButton( bRiffIsPlaying, true );
                                    if ( ImGui::Button( ICON_FA_PLAY, buttonSizeMidTable ) )
                                    {
                                        if ( !bRiffIsPlaying )
                                        {
                                            getEventBusClient().Send< ::events::EnqueueRiffPlayback >( riffTag.m_jam, riffID );
                                        }
                                    }
                                    if ( ImGui::IsItemHovered() )
                                    {
                                        m_jamTaggingCurrentlyHovered = riffID;
                                    }
                                }
                                ImGui::SameLine();
                                ImGui::AlignTextToFramePadding();
                                {
                                    uint32_t favourColour = colour::shades::tag_lvl_1.neutralU32();
                                    switch ( riffTag.m_favour )
                                    {
                                        case 1: favourColour = colour::shades::tag_lvl_2.neutralU32();
                                    }
                                    ImGui::PushStyleColor( ImGuiCol_Text, favourColour );
                                    ImGui::TextUnformatted( ICON_FC_FULL_BLOCK );
                                    ImGui::PopStyleColor();
                                }

                                ImGui::TableNextColumn();
                                {
                                    ImGui::AlignTextToFramePadding();
                                    ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
                                    const bool bTextAccept = ImGui::InputText( "###note", &riffTag.m_note, ImGuiInputTextFlags_EnterReturnsTrue );
                                    if ( bTextAccept || ImGui::IsItemDeactivatedAfterEdit() )
                                    {
                                        getEventBusClient().Send< ::events::RiffTagAction >( riffTag, ::events::RiffTagAction::Action::Upsert );
                                    }
                                }

                                ImGui::TableNextColumn();
                                {
                                    ImGui::Dummy( { 0, 0 } );
                                    ImGui::SameLine( 0, 8.0f );
                                    // double-wrap tooltip so we only do the (non trivial) time conversion / string build on hover
                                    ImGui::TextDisabled( ICON_FA_CLOCK );
                                    if ( ImGui::IsItemHovered( ImGuiHoveredFlags_DelayNormal ) )
                                    {
                                        const auto shareTimeUnix = spacetime::InSeconds( std::chrono::seconds{ riffTag.m_timestamp } );
                                        const auto cacheTimeDelta = spacetime::calculateDeltaFromNow( shareTimeUnix ).asPastTenseString( 3 );
                                        ImGui::CompactTooltip( fmt::format( FMTX("{}\n{}"), spacetime::datestampStringFromUnix( shareTimeUnix ), cacheTimeDelta ).c_str() );
                                    }
                                }

                                ImGui::TableNextColumn();
                                if ( ImGui::Button( ICON_FA_GRIP, buttonSizeMidTable ) )
                                {
                                    // dispatch a request to navigate this this riff, if we can find it
                                    getEventBusClient().Send< ::events::RequestNavigationToRiff >( endlesss::types::RiffIdentity{ riffTag.m_jam, riffID } );
                                }

                                ImGui::TableNextColumn();
                                if ( bRiffIsExporting )
                                {
                                    ImGui::AlignTextToFramePadding();
                                    ImGui::Dummy( { 0, 0 } );
                                    ImGui::SameLine( 0, 4.0f );
                                    ImGui::Spinner( "##exporting", true, ImGui::GetTextLineHeight() * 0.48f, 3.0f, 0.0f, ImGui::GetColorU32( ImGuiCol_Text ) );
                                }

                                ImGui::TableNextColumn();
                                ImGui::AlignTextToFramePadding();
                                {
                                    static const char* cDragPayloadType = "dnd.TaggedRiff";

                                    bool bAllowDragTargetAbove = true;
                                    bool bAllowDragTargetBelow = true;
                                    bool bDragOperationRunning = false;

                                    // check on the current drag state - we choose to allow dragging for each row based on 
                                    // if the result would be valid - eg. don't bother dragging onto the initial drag source
                                    const ImGuiPayload* dragPayloadPeek = ImGui::GetDragDropPayload();
                                    if ( dragPayloadPeek && dragPayloadPeek->IsDataType( cDragPayloadType ) )
                                    {
                                        ABSL_ASSERT( dragPayloadPeek->DataSize == sizeof( int32_t ) );
                                        const int32_t draggedFromRiffIndex = *(const int32_t*)dragPayloadPeek->Data;

                                        bDragOperationRunning = true;

                                        // don't drag onto ourself
                                        if ( draggedFromRiffIndex == riffEntry )
                                        {
                                            bAllowDragTargetAbove = false;
                                            bAllowDragTargetBelow = false;
                                        }
                                        // dont bother reordering onto our original position either
                                        if ( riffEntry + 1 == draggedFromRiffIndex )
                                        {
                                            bAllowDragTargetBelow = false;
                                        }
                                        if ( riffEntry - 1 == draggedFromRiffIndex )
                                        {
                                            bAllowDragTargetAbove = false;
                                        }
                                    }

                                    // add an ordering button to shift the riff up or down in the list; this also wires in
                                    // the drag-drop logic to hide/colour buttons during drag procedures .. it's a little convoluted in there
                                    const auto AddOrderingButton = [&](
                                        const char* label,
                                        const bool exchangeEnableLogic,
                                        const int32_t exchangeIndex,
                                        const bool dragEnableLogic,
                                        const JamTagVectorOp::Op dragOp )
                                        {
                                            {
                                                // don't show buttons that aren't useful drag targets when we're dragging
                                                const bool bHideButtonDuringDragWithoutTarget = bDragOperationRunning && !dragEnableLogic;

                                                // disable the button for exchanging if the exchange wouldn't be valid .. UNLESS we're dragging!
                                                ImGui::Scoped::Enabled enabledButton( bDragOperationRunning || exchangeEnableLogic );
                                                ImGui::Scoped::ColourButton colourButton( colour::shades::pink, dragEnableLogic&& bDragOperationRunning );

                                                // just stick in an empty dummy space if we're not showing the button
                                                if ( bHideButtonDuringDragWithoutTarget )
                                                {
                                                    ImGui::Dummy( buttonSizeMidTable );
                                                }
                                                // and show the button but disable its actual click logic if we're dragging
                                                else if ( ImGui::Button( label, buttonSizeMidTable ) && !bDragOperationRunning )
                                                {
                                                    tagOperationToDo = JamTagVectorOp( JamTagVectorOp::Op::Exchange, riffEntry, exchangeIndex );
                                                }
                                            }
                                            // deal with creating a move operation on drop
                                            if ( dragEnableLogic && ImGui::BeginDragDropTarget() )
                                            {
                                                if ( const ImGuiPayload* payload = ImGui::AcceptDragDropPayload( cDragPayloadType ) )
                                                {
                                                    ABSL_ASSERT( payload->DataSize == sizeof( int32_t ) );
                                                    const int32_t draggedFromRiffIndex = *(const int32_t*)payload->Data;

                                                    tagOperationToDo = JamTagVectorOp( dragOp, draggedFromRiffIndex, riffEntry );
                                                }
                                                ImGui::EndDragDropTarget();
                                            }
                                        };

                                    AddOrderingButton( ICON_FA_ARROW_UP,    riffEntry > 0,              riffEntry - 1, bAllowDragTargetAbove, JamTagVectorOp::Op::Move1Before2 );
                                    ImGui::SameLine( 0, 2.0f );
                                    AddOrderingButton( ICON_FA_ARROW_DOWN,  riffEntry < tagCount - 1,   riffEntry + 1, bAllowDragTargetBelow, JamTagVectorOp::Op::Move1After2 );

                                    ImGui::SameLine( 0, 4.0f );

                                    // grip button to start dragging this row elsewhere in the list
                                    if ( !bDragOperationRunning )
                                    {
                                        ImGui::Button( ICON_FA_GRIP_VERTICAL );
                                        if ( ImGui::BeginDragDropSource( ImGuiDragDropFlags_None ) )
                                        {
                                            ImGui::SetDragDropPayload( cDragPayloadType, &riffEntry, sizeof( int32_t ) );
                                            ImGui::TextUnformatted( riffTag.m_note );
                                            ImGui::EndDragDropSource();
                                        }
                                    }
                                }

                                ImGui::PopID();
                                riffEntry++;
                            }

                            // act upon a request to move/exchange values in the array now we've done iterating
                            if ( tagOperationToDo.has_value() )
                            {
                                switch ( tagOperationToDo->m_op )
                                {
                                    case JamTagVectorOp::Op::Exchange:
                                    {
                                        // flip the requested pair in the array
                                        std::swap(
                                            m_jamTagging.tagVector[tagOperationToDo->m_index1],
                                            m_jamTagging.tagVector[tagOperationToDo->m_index2]
                                        );
                                    }
                                    break;

                                    case JamTagVectorOp::Op::Move1Before2:
                                    case JamTagVectorOp::Op::Move1After2:
                                    {
                                        base::vector_move( m_jamTagging.tagVector, tagOperationToDo->m_index1, tagOperationToDo->m_index2 );
                                    }
                                    break;
                                }

                                // re-index everything
                                for ( int32_t newIndex = 0; newIndex < static_cast<int32_t>(m_jamTagging.tagVector.size() ); newIndex++ )
                                    m_jamTagging.tagVector[newIndex].m_order = newIndex;

                                // batch update all the tags so the ordering is synchronised
                                m_warehouse->batchUpdateTags( m_jamTagging.tagVector );

                                tagOperationToDo = std::nullopt;
                            }

                            ImGui::EndTable();
                        }

                        ImGui::PopStyleVar();
                    }
                }
            }
            ImGui::End();
        }
        {
            const fs::path cWarehouseExportPath = m_storagePaths->outputApp / "$database_exports";
            static const ImVec2 toolbarButtonSize{ 155.0f, 0.0f };

            static WarehouseView warehouseView( WarehouseView::Default );
            static ImGuiTextFilter jamNameFilter;

            const bool bWarehouseHasEndlesssAccess = m_warehouse->hasFullEndlesssNetworkAccess();

            const auto fnWarehouseViewHeaders = [&]()
                {
                    constexpr float cEdgeInsetSize = 4.0f;
                    constexpr float cButtonGapSize = 6.0f;

                    // show if the warehouse is running ops, if not then disable new-task buttons
                    const bool bWarehouseIsPaused = m_warehouse->workerIsPaused();

                    ImGui::Scoped::ButtonTextAlignLeft leftAlign;

                    ImGui::Dummy( { cEdgeInsetSize, 0 } );
                    ImGui::SameLine( 0, 0 );
                    ImGui::AlignTextToFramePadding();
                    if ( warehouseView == WarehouseView::Default )
                    {
                        ImGui::TextColored( colour::shades::white.dark(), ICON_FA_DATABASE " Storing %s",
                            fmt::format( std::locale( "" ), "[ {:L} Jams ] [ {:L} Riffs ] [ {:L} Stems ]",
                                m_warehouseContentsReport.m_populatedRiffs.size(),
                                m_warehouseContentsReport.m_totalPopulatedRiffs,
                                m_warehouseContentsReport.m_totalPopulatedStems).c_str()
                        );

                        // button to start the sync process on every jam we know about
                        ImGui::RightAlignSameLine( toolbarButtonSize.x + cEdgeInsetSize );
                        if ( ImGui::Button( " " ICON_FA_ARROWS_ROTATE " Sync All", toolbarButtonSize ) )
                        {
                            std::scoped_lock<std::mutex> reportLock( m_warehouseContentsReportMutex );
                            for ( size_t jamIdx = 0; jamIdx < m_warehouseContentsReport.m_jamCouchIDs.size(); jamIdx++ )
                            {
                                const std::size_t jI = m_warehouseContentsSortedIndices[jamIdx];
                                triggerSyncOnJamInContentsReport( jI );
                            }
                        }
                        ImGui::CompactTooltip( "Trigger a sync for all jams\nThis may take a while if you have lots of jams!" );
                    }
                    else
                    if ( warehouseView == WarehouseView::ContentsManagement )
                    {
                        ImGui::TextColored( colour::shades::callout.neutral(), ICON_FA_GEAR " Contents Management" );

                        ImGui::RightAlignSameLine( toolbarButtonSize.x + cEdgeInsetSize );
                        if ( ImGui::Button( " " ICON_FA_CLIPBOARD " Copy Report", toolbarButtonSize ) )
                        {
                            std::string reportResult;
                            reportResult.reserve( 32 * 1024 );

                            std::scoped_lock<std::mutex> reportLock( m_warehouseContentsReportMutex );
                            for ( size_t jamIdx = 0; jamIdx < m_warehouseContentsReport.m_jamCouchIDs.size(); jamIdx++ )
                            {
                                const std::size_t jI = m_warehouseContentsSortedIndices[jamIdx];

                                reportResult += fmt::format( FMTX( "{:60} | {:6} riffs | {:6} stems\n" ),
                                    m_warehouseContentsReportJamTitles[jI],
                                    m_warehouseContentsReport.m_populatedRiffs[jI],
                                    m_warehouseContentsReport.m_populatedStems[jI]
                                    );
                            }
                            ImGui::SetClipboardText( reportResult.c_str() );
                        }
                        ImGui::CompactTooltip( "Produce and copy a simple jam contents report to the clipboard" );
                    }
                    else
                    if ( warehouseView == WarehouseView::ImportExport )
                    {
                        ImGui::TextColored( colour::shades::callout.neutral(), ICON_FA_GEAR " Import / Export" );

                        ImGui::RightAlignSameLine( ( toolbarButtonSize.x * 2.0f ) + cButtonGapSize + cEdgeInsetSize );

                        if ( ImGui::Button( " " ICON_FA_BOX_OPEN " Import Data", toolbarButtonSize) )
                        {
                            auto fileDialog = std::make_unique< ImGuiFileDialog >();
                            fileDialog->OpenDialog(
                                "ImpFileDlg",
                                "Choose exported LORE metadata",
                                ".yaml",
                                cWarehouseExportPath.string().c_str(),
                                1,
                                nullptr,
                                ImGuiFileDialogFlags_Modal );

                            std::ignore = activateFileDialog( std::move( fileDialog ), [this]( ImGuiFileDialog& dlg )
                                {
                                    // ask warehouse to deal with this
                                    const base::OperationID importOperationID = m_warehouse->requestJamDataImport( dlg.GetFilePathName() );
                                });
                        }
                        ImGui::SameLine( 0, cButtonGapSize );
                        if ( ImGui::Button( " " ICON_FA_BOX_OPEN " Import Stems", toolbarButtonSize) )
                        {
                            auto fileDialog = std::make_unique< ImGuiFileDialog >();
                            fileDialog->OpenDialog(
                                "ImpFileDlg",
                                "Choose LORE stem archive",
                                ".tar",
                                cWarehouseExportPath.string().c_str(),
                                1,
                                nullptr,
                                ImGuiFileDialogFlags_Modal );

                            std::ignore = activateFileDialog( std::move( fileDialog ), [this]( ImGuiFileDialog& dlg )
                                {
                                    const fs::path inputTarFile = dlg.GetFilePathName();
                                    const fs::path outputPath = getStemCache().getCacheRootPath();

                                    blog::app( FMTX( "stem import task queued - from [{}] to [{}]" ), inputTarFile.string(), outputPath.string() );

                                    const auto importOperationID = base::Operations::newID( endlesss::toolkit::Warehouse::OV_ImportAction );

                                    // spin up a background task to archive the stems into a .tar archive
                                    getTaskExecutor().silent_async( [this, inputTarFile, outputPath, importOperationID]()
                                        {
                                            base::EventBusClient m_eventBusClient( m_appEventBus );
                                            OperationCompleteOnScopeExit( importOperationID );

                                            std::size_t filesTouched = 0;

                                            const auto tarArchiveStatus = io::unarchiveTARIntoDirectory(
                                                inputTarFile,
                                                outputPath,
                                                [&]( const std::size_t bytesProcessed, const std::size_t filesProcessed )
                                                {
                                                    // ping that we're still working on async tasks
                                                    m_eventBusClient.Send< ::events::AsyncTaskActivity >();
                                                    filesTouched++;
                                                });

                                            // deal with issues, tell user we bailed
                                            if ( !tarArchiveStatus.ok() )
                                            {
                                                m_appEventBus->send<::events::AddErrorPopup>(
                                                    "Stem Import from TAR Failed",
                                                    fmt::format( FMTX("Error reported during stem import:\n{}"), tarArchiveStatus.ToString() )
                                                );
                                            }
                                            else
                                            {
                                                m_appEventBus->send<::events::AddToastNotification>( ::events::AddToastNotification::Type::Info,
                                                    ICON_FA_BOXES_PACKING " Stem Import Success",
                                                    fmt::format( FMTX( "Extracted {} stems" ), filesTouched ) );
                                            }
                                        });
                                });
                        }
                    }
                    else
                    if ( warehouseView == WarehouseView::Advanced )
                    {
                        ImGui::TextColored( colour::shades::callout.neutral(), ICON_FA_GEAR " Advanced" );

                        ImGui::RightAlignSameLine( (toolbarButtonSize.x * 2.0f) + cButtonGapSize + cEdgeInsetSize );

                        if ( ImGui::Button( " " ICON_FA_CODE_MERGE " Conflicts...", toolbarButtonSize ) )
                        {
                            activateModalPopup( "Set Conflict Resolution Mode", [&, this]( const char* title )
                            {
                                using namespace endlesss::toolkit;

                                const ImVec2 configWindowSize = ImVec2( 680.0f, 320.0f );
                                ImGui::SetNextWindowContentSize( configWindowSize );

                                if ( ImGui::BeginPopupModal( title, nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize ) )
                                {
                                    const ImVec2 buttonSize( 240.0f, 32.0f );

                                    ImGui::TextWrapped( "Due to a quirk with how Endlesss handles riff remixing, it's possible to find riffs in a personal feed that share a 'unique' ID with the jams where they were originally authored. This is not compatible with the way the OUROVEON Warehouse database organises things, expecting riffs to have a globally unqiue ID." );
                                    ImGui::Spacing();
                                    ImGui::TextWrapped( "More details can be found in the OUROVEON documentation." );
                                    ImGui::Spacing();
                                    ImGui::TextWrapped( "To work around this, there are several options for how to resolve conflicts. Check the tooltips for some further explanation." );
                                    ImGui::Spacing();
                                    ImGui::Spacing();
                                    ImGui::Spacing();

                                    {
                                        int32_t currentMode = (int32_t)m_warehouse->getCurrentRiffIDConflictHandling();

                                        ImGui::RadioButton( "Ignore All", &currentMode, (int32_t)Warehouse::RiffIDConflictHandling::IgnoreAll );
                                        ImGui::CompactTooltip( Warehouse::getRiffIDConflictHandlingTooltip( Warehouse::RiffIDConflictHandling::IgnoreAll ).data() );

                                        ImGui::RadioButton( "Overwrite", &currentMode, (int32_t)Warehouse::RiffIDConflictHandling::Overwrite );
                                        ImGui::CompactTooltip( Warehouse::getRiffIDConflictHandlingTooltip( Warehouse::RiffIDConflictHandling::Overwrite ).data() );

                                        ImGui::RadioButton( "Overwrite, Except Personal", &currentMode, (int32_t)Warehouse::RiffIDConflictHandling::OverwriteExceptPersonal );
                                        ImGui::CompactTooltip( Warehouse::getRiffIDConflictHandlingTooltip( Warehouse::RiffIDConflictHandling::OverwriteExceptPersonal ).data() );

                                        m_warehouse->setCurrentRiffIDConflictHandling( (Warehouse::RiffIDConflictHandling)currentMode );
                                    }

                                    ImGui::Spacing();
                                    ImGui::Spacing();

                                    if ( ImGui::BottomRightAlignedButton( "Close", buttonSize ) )
                                    {
                                        ImGui::CloseCurrentPopup();
                                    }

                                    ImGui::EndPopup();
                                }
                            });
                        }
                        ImGui::CompactTooltip( "Configure logic for handling Riff ID conflicts during sync" );

                        ImGui::SameLine( 0, cButtonGapSize );
                        {
                            // enable or disable the worker thread
                            ImGui::Scoped::ToggleButton highlightButton( !bWarehouseIsPaused, true );
                            if ( ImGui::Button( bWarehouseIsPaused ?
                                " " ICON_FA_CIRCLE_PLAY  " RESUME  " :
                                " " ICON_FA_CIRCLE_PAUSE " RUNNING ", toolbarButtonSize ) )
                            {
                                m_warehouse->workerTogglePause();
                            }
                        }
                    }
                };

            if ( ImGui::Begin( warehouseView.generateTitle().c_str() ) )
            {
                warehouseView.checkForImGuiTabSwitch();

                fnWarehouseViewHeaders();
                ImGui::SeparatorBreak();

                {
                    ImGui::Dummy( { 4, 0 } );
                    ImGui::SameLine( 0, 0 );
                    ImGui::AlignTextToFramePadding();
                    ImGui::StandardFilterBox( jamNameFilter, "###nameFilter" );

                    ImGui::RightAlignSameLine( ( toolbarButtonSize.x * 2.0f ) + 4.0f + 6.0f + 40.0f );

                    ImGui::TextUnformatted( "Sort" );
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth( toolbarButtonSize.x );
                    if ( WarehouseContentsSortMode::ImGuiCombo( "###sortOrder", m_warehouseContentsSortMode ) )
                    {
                        updateWarehouseContentsSortOrder();
                    }
                    ImGui::SameLine(0, 6.0f);
                }
                {
                    ImGui::Scoped::ButtonTextAlignLeft leftAlign;
                    ImGui::Scoped::Enabled se( bWarehouseHasEndlesssAccess );

                    if ( ImGui::Button( " " ICON_FA_CIRCLE_PLUS " Add Jam...", toolbarButtonSize ) )
                    {
                        // create local copy of the current warehouse jam ID map for use by the popup; avoids
                        // having to worry about warehouse contents shifting underneath / locking mutex in dialog
                        {
                            std::scoped_lock<std::mutex> reportLock( m_warehouseContentsReportMutex );
                            warehouseJamBrowser.m_warehouseJamIDs = m_warehouseContentsReportJamIDs;
                        }
                        // launch modal browser
                        activateModalPopup( "Select Jam To Sync", [&, this]( const char* title )
                        {
                            ux::modalUniversalJamBrowser( title, m_jamLibrary, warehouseJamBrowser, *this );
                        });
                    }
                }
                ImGui::Spacing();
                ImGui::Spacing();


                static ImVec2 buttonSizeMidTable( 33.0f, 22.0f );

                if ( ImGui::BeginChild( "##data_child" ) )
                {
                    const auto TextColourDownloading  = ImGui::GetStyleColorVec4( ImGuiCol_CheckMark );
                    const auto TextColourDownloadable = ImGui::GetStyleColorVec4( ImGuiCol_PlotHistogram );

                    const auto GetColumnCount = [=]()
                        {
                            if ( warehouseView == WarehouseView::Default )              return 5;
                            if ( warehouseView == WarehouseView::ContentsManagement )   return 3;
                            if ( warehouseView == WarehouseView::ImportExport )         return 3;
                            if ( warehouseView == WarehouseView::Advanced )             return 4;

                            ABSL_ASSERT( 0 );
                            return 0;
                        };
                    const auto columnCount = GetColumnCount();

                    if ( ImGui::BeginTable( "##warehouse_table", columnCount,
                                ImGuiTableFlags_ScrollY         |
                                ImGuiTableFlags_Borders         |
                                ImGuiTableFlags_RowBg           |
                                ImGuiTableFlags_NoSavedSettings ))
                    {
                        ImGui::TableSetupScrollFreeze( 0, 1 );  // top row always visible

                        ImGui::TableSetupColumn( "View",            ImGuiTableColumnFlags_WidthFixed,  32.0f );
                        ImGui::TableSetupColumn( "Jam Name",        ImGuiTableColumnFlags_WidthStretch, 1.0f );

                        if ( warehouseView == WarehouseView::Default )
                        {
                            ImGui::TableSetupColumn( "Sync",        ImGuiTableColumnFlags_WidthFixed,  32.0f );
                            ImGui::TableSetupColumn( "Riffs",       ImGuiTableColumnFlags_WidthFixed, 120.0f );
                            ImGui::TableSetupColumn( "Stems",       ImGuiTableColumnFlags_WidthFixed, 120.0f );
                        }
                        else
                        if ( warehouseView == WarehouseView::Advanced )
                        {
                            ImGui::TableSetupColumn( "Data",        ImGuiTableColumnFlags_WidthFixed, 240.0f + 9.0f ); // 9.0 for some kind of padding per column that gets added
                            ImGui::TableSetupColumn( "Wipe",        ImGuiTableColumnFlags_WidthFixed,  32.0f );
                        }
                        else
                        if ( warehouseView == WarehouseView::ImportExport )
                        {
                            ImGui::TableSetupColumn( "Export",      ImGuiTableColumnFlags_WidthFixed, 272.0f + 9.0f + 9.0f );
                        }
                        // generic fallback
                        else
                        {
                            ImGui::TableSetupColumn( "Tools",       ImGuiTableColumnFlags_WidthFixed, 272.0f + 9.0f + 9.0f );
                        }
                        ImGui::TableHeadersRow();

                        // lock the data report so it isn't whipped away from underneath us mid-render
                        std::scoped_lock<std::mutex> reportLock( m_warehouseContentsReportMutex );

                        for ( size_t jamIdx = 0; jamIdx < m_warehouseContentsReport.m_jamCouchIDs.size(); jamIdx++ )
                        {
                            const std::size_t jI            = m_warehouseContentsSortedIndices[jamIdx];
                            const int64_t unpopulatedRiffs  = m_warehouseContentsReport.m_unpopulatedRiffs[jI];
                            const int64_t populatedRiffs    = m_warehouseContentsReport.m_populatedRiffs[jI];
                            const int64_t unpopulatedStems  = m_warehouseContentsReport.m_unpopulatedStems[jI];
                            const int64_t populatedStems    = m_warehouseContentsReport.m_populatedStems[jI];

                            const auto iterCurrentJamID     = m_warehouseContentsReport.m_jamCouchIDs[jI];

                            const auto knownCachedRiffCount = m_jamLibrary.loadKnownRiffCountForDatabaseID( iterCurrentJamID );

                            const bool bIsJamInFlux         = m_warehouseContentsReportJamInFlux[jI] || doesJamHaveActiveOperations( iterCurrentJamID );
                            const bool bHasDataToSync       = (unpopulatedRiffs > 0 || knownCachedRiffCount > populatedRiffs);


                            const auto& jamNameToFilterAgainst = m_warehouseContentsReportJamTitlesForSort[jI];
                            if ( !jamNameFilter.PassFilter( jamNameToFilterAgainst.c_str(), &jamNameToFilterAgainst.back() + 1 ) )
                                continue;

                            ImGui::PushID( (int32_t)jI );
                            ImGui::TableNextColumn();

                            // highlight or lowlight column based on state
                            if ( bIsJamInFlux )
                                ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg0, ImGui::GetSyncBusyColour( 0.2f ) );
                            if ( m_currentViewedJam == iterCurrentJamID )
                                ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32( ImGuiCol_TableRowBgAlt, 2.5f ) );

                            // always show the view-this-jam button regardless of mode
                            {
                                if ( bIsJamInFlux )
                                {
                                    ImGui::Dummy( { 8, 2 } );
                                    ImGui::SameLine( 0, 0 );
                                    ImGui::Spinner( "##syncing", true, ImGui::GetTextLineHeight() * 0.4f, 3.0f, 1.5f, ImGui::GetColorU32( ImGuiCol_Text ) );
                                }
                                else
                                {
                                    if ( ImGui::PrecisionButton( ICON_FA_GRIP, buttonSizeMidTable, 1.0f ) )
                                    {
                                        beginChangeToViewJam( iterCurrentJamID );
                                    }
                                }

                                ImGui::TableNextColumn();
                            }
                            // .. followed by the jam name
                            {
                                ImGui::AlignTextToFramePadding();
                                ImGui::TextUnformatted( m_warehouseContentsReportJamTitles[jI].c_str() );
                                ImGui::TableNextColumn();
                            }


                            // -----------------------------------------------------------------------------------------
                            if ( warehouseView == WarehouseView::Default )
                            {
                                if ( bWarehouseHasEndlesssAccess )
                                {
                                    if ( bIsJamInFlux )
                                    {
                                        // handle the option to abort the sync .. by nuking the jam
                                        {
                                            // only show the abort button after a delay so there are less chances of a mis-click
                                            // given that it purges the jam entirely
                                            const bool bNotEnoughTimePassedSinceAbortShown = !m_warehouseContentsReportJamInFluxMoment[jI].hasPassed();
                                            ImGui::Scoped::Disabled se( bNotEnoughTimePassedSinceAbortShown || !bHasDataToSync );
                                            ImGui::Scoped::ColourButton cb( colour::shades::errors, colour::shades::white );
                                            if ( ImGui::Button( ICON_FA_BAN, buttonSizeMidTable ) )
                                            {
                                                m_warehouse->requestJamSyncAbort( iterCurrentJamID );
                                                // push the abort task timer way forward to immediately disable the button
                                                m_warehouseContentsReportJamInFluxMoment[jI].setToFuture( std::chrono::hours( 1 ) );
                                            }
                                        }
                                        ImGui::CompactTooltip( "Stop jam sync by removing all currently un-synchronised riffs, leaving the jam not up-to-date" );
                                    }
                                    else
                                    {
                                        if ( ImGui::Button( ICON_FA_ARROWS_ROTATE, buttonSizeMidTable ) )
                                        {
                                            triggerSyncOnJamInContentsReport( jI );
                                        }
                                        ImGui::CompactTooltip( "Update and download the latest metadata for this jam" );
                                    }
                                }

                                // riffs
                                {
                                    ImGui::TableNextColumn();
                                    ImGui::AlignTextToFramePadding();
                                    ImGui::Text( "%" PRIi64, populatedRiffs );

                                    if ( unpopulatedRiffs > 0 )
                                    {
                                        ImGui::SameLine( 0, 0 );
                                        ImGui::TextColored( TextColourDownloading, " (+%" PRIi64 ")", unpopulatedRiffs );
                                    }
                                    else if ( knownCachedRiffCount > populatedRiffs )
                                    {
                                        ImGui::SameLine( 0, 0 );
                                        ImGui::TextColored( TextColourDownloadable, " (" ICON_FA_ARROW_UP "%li)", knownCachedRiffCount - populatedRiffs );
                                    }
                                }
                                // stems
                                {
                                    ImGui::TableNextColumn();
                                    ImGui::AlignTextToFramePadding();
                                    ImGui::Text( "%" PRIi64, populatedStems );

                                    if ( unpopulatedStems > 0 )
                                    {
                                        ImGui::SameLine( 0, 0 );
                                        ImGui::TextColored( TextColourDownloading, " (+%" PRIi64 ")", unpopulatedStems );
                                    }
                                }
                            }

                            // -----------------------------------------------------------------------------------------
                            if ( warehouseView == WarehouseView::ContentsManagement )
                            {
                                // disable tools if we're syncing
                                ImGui::Scoped::Disabled sd( bIsJamInFlux );

                                if ( ImGui::Button( " " ICON_FA_LIST_CHECK " Cache ... " ) )
                                {
                                    const auto popupLabel = fmt::format( FMTX( "Precache All Stems : {}###precache_modal" ), m_warehouseContentsReportJamTitles[jI] );

                                    // create and launch the precache tool w. attached state
                                    activateModalPopup( popupLabel, [
                                        this,
                                            &riffFetchProvider,
                                            state = ux::createJamPrecacheState( iterCurrentJamID )](const char* title)
                                        {
                                            ux::modalJamPrecache( title, *state, *m_warehouse, riffFetchProvider, getTaskExecutor() );
                                        });
                                }
                                ImGui::CompactTooltip( "Open a utility that allows you to download all stems for this jam,\nallowing for fully offline browsing and archival" );
                            }
                            else
                            if ( warehouseView == WarehouseView::ImportExport )
                            {
                                // disable tools if we're syncing
                                ImGui::Scoped::Disabled sd( bIsJamInFlux );

                                const auto checkOutputDirValid = [&]()
                                    {
                                        const auto exportPathStatus = filesys::ensureDirectoryExists( cWarehouseExportPath );
                                        if ( !exportPathStatus.ok() )
                                        {
                                            m_appEventBus->send<::events::AddErrorPopup>(
                                                "Enable to create output directory",
                                                "Was unable to create database export directory, cannot save to disk"
                                            );
                                            return false;
                                        }
                                        return true;
                                    };

                                if ( ImGui::Button( " " ICON_FA_BOX_ARCHIVE " Data    ") )
                                {
                                    // make sure it exists, pop an error if that fails
                                    if ( checkOutputDirValid() )
                                    {
                                        // tell warehouse to spool the database records out to disk
                                        const base::OperationID exportOperationID = m_warehouse->requestJamDataExport(
                                            iterCurrentJamID,
                                            cWarehouseExportPath,
                                            m_warehouseContentsReportJamTitles[jI]
                                        );

                                        addOperationToJam( iterCurrentJamID, exportOperationID );
                                    }
                                }
                                ImGui::CompactTooltip( "Begin an export process to archive this jam's database records to a file on disk" );

                                ImGui::SameLine();
                                if ( ImGui::Button( " " ICON_FA_BOXES_PACKING " Stems   " ) )
                                {
                                    if ( checkOutputDirValid() )
                                    {
                                        const std::string exportFilenameTar = endlesss::toolkit::Warehouse::createExportFilenameForJam(
                                            iterCurrentJamID,
                                            m_warehouseContentsReportJamTitles[jI],
                                            "tar" );

                                        const fs::path inputPath = getStemCache().getCacheRootPath() / fs::path( iterCurrentJamID.value() );
                                        const fs::path outputPath = cWarehouseExportPath / exportFilenameTar;

                                        blog::app( FMTX( "stem export task queued - from [{}] to [{}]" ), inputPath.string(), outputPath.string() );

                                        const auto exportOperationID = base::Operations::newID( endlesss::toolkit::Warehouse::OV_ExportAction );
                                        addOperationToJam( iterCurrentJamID, exportOperationID );

                                        // spin up a background task to archive the stems into a .tar archive
                                        getTaskExecutor().silent_async( [this, inputPath, outputPath, exportFile = std::move( exportFilenameTar ), exportOperationID]()
                                            {
                                                base::EventBusClient m_eventBusClient( m_appEventBus );
                                                OperationCompleteOnScopeExit( exportOperationID );

                                                const auto tarArchiveStatus = io::archiveFilesInDirectoryToTAR(
                                                    inputPath,
                                                    outputPath,
                                                    [&]( const std::size_t bytesProcessed, const std::size_t filesProcessed )
                                                    {
                                                        // ping that we're still working on async tasks
                                                        m_eventBusClient.Send< ::events::AsyncTaskActivity >();
                                                    });

                                                // deal with issues, tell user we bailed
                                                if ( !tarArchiveStatus.ok() )
                                                {
                                                    m_appEventBus->send<::events::AddErrorPopup>(
                                                        "Stem Export to TAR Failed",
                                                        fmt::format( FMTX("Error reported during export:\n{}"), tarArchiveStatus.ToString() )
                                                    );
                                                }
                                                else
                                                {
                                                    m_appEventBus->send<::events::AddToastNotification>( ::events::AddToastNotification::Type::Info,
                                                        ICON_FA_BOXES_PACKING " Stem Export Success",
                                                        fmt::format( FMTX( "Written to [{}]" ), exportFile ) );
                                                }
                                            });
                                    }
                                }
                                ImGui::CompactTooltip( "Begin the process to bundle up all stems from this jam into a .tar archive" );
                            }
                            else
                            if ( warehouseView == WarehouseView::Advanced )
                            {
                                const char* jamBandID = iterCurrentJamID.c_str();

                                {
                                    // disable tools if we're syncing
                                    ImGui::Scoped::Disabled sd( bIsJamInFlux );

                                    if ( ImGui::Button( " " ICON_FA_MAGNIFYING_GLASS_PLUS " Validate " ) )
                                    {
                                        const auto popupLabel = fmt::format( FMTX( "Validation : {}###validate_modal" ), m_warehouseContentsReportJamTitles[jI] );

                                        // create and launch the precache tool w. attached state
                                        activateModalPopup( popupLabel, [
                                            this,
                                                netCfg = getNetworkConfiguration(),
                                                state = ux::createJamValidateState( iterCurrentJamID )](const char* title)
                                            {
                                                ux::modalJamValidate( title, *state, *m_warehouse, netCfg, getTaskExecutor() );
                                            } );
                                    }
                                    ImGui::CompactTooltip( "Display tools for validating the data in the warehouse against the Endlesss server" );
                                }
                                
                                ImGui::SameLine();
                                ImGui::AlignTextToFramePadding();
                                ImGui::TextDisabled( "%s", jamBandID );
                                if ( ImGui::IsItemClicked() )
                                {
                                    ImGui::SetClipboardText( jamBandID );
                                }
                                ImGui::CompactTooltip( jamBandID );


                                ImGui::TableNextColumn();
                                ImGui::AlignTextToFramePadding();

                                // trashing a jam even in sync should be fine, given the way the warehouse works and
                                // sequences operations. a purge will remove and future scanning for empty riffs & stems
                                ImGui::Scoped::ColourButton cb( colour::shades::errors, colour::shades::white );
                                if ( ImGui::Button( ICON_FA_TRASH_CAN, buttonSizeMidTable ) )
                                {
                                    m_warehouse->requestJamPurge( iterCurrentJamID );
                                }
                                ImGui::CompactTooltip( "Request a deletion of the jam and all associated data" );
                            }

                            ImGui::PopID();
                        }

                        ImGui::EndTable();
                    }
                }
                ImGui::EndChild();
            }
            ImGui::End();

        } // warehouse imgui 

        m_uxRiffHistory->imgui( *this );
        m_uxSharedRiffView->imgui( *this, JamNameResolveProvider );


        maintainStemCacheAsync();

        finishInterfaceLayoutAndRender();
    }

    // unhook events
    {
        checkedCoreCall( "remove listener", [this] { return m_appEventBus->removeListener( m_eventListenerRiffEnqueue ); } );
    }
    {
        base::EventBusClient m_eventBusClient( m_appEventBus );

        APP_EVENT_UNBIND( PanicStop );
        APP_EVENT_UNBIND( MixerRiffChange );
        APP_EVENT_UNBIND( OperationComplete );
        APP_EVENT_UNBIND( BNSWasUpdated );
        APP_EVENT_UNBIND( RequestNavigationToRiff );
    }

    // unplug from warehouse
    unregisterStatusBarBlock( sbbWarehouseID );
    m_warehouse->clearAllCallbacks();

    // shut down the GPU rendering sketchbook
    {
        m_jamSlice.reset();
        m_jamSliceSketch.reset();
        m_sketchbook.reset();
    }

    m_riffPipeline.reset();

    m_discordBotUI.reset();

    m_uxTagLine.reset();
    m_uxSharedRiffView.reset();

    m_vibes.reset();

    // remove the mixer and ensure the async op has taken before leaving scope
    m_mdAudio->blockUntil( m_mdAudio->installMixer( nullptr ) );
    m_mdAudio->blockUntil( m_mdAudio->effectClearAll() );

    // unregister any listeners
    checkedCoreCall( "remove stem listener", [this] { return m_stemDataProcessor.disconnect( m_appEventBus ); } );


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
