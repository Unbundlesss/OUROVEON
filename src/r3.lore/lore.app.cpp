#include "pch.h"

#include "base/utils.h"
#include "base/buffer2d.h"
#include "base/metaenum.h"
#include "base/instrumentation.h"

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

#include "discord/discord.bot.ui.h"

#include "endlesss/all.h"

#include "effect/effect.stack.h"

#include "mix/preview.h"

#include "gfx/sketchbook.h"



#define OUROVEON_LORE           "LORE"
#define OUROVEON_LORE_VERSION   OURO_FRAMEWORK_VERSION "-alpha"


namespace stdp = std::placeholders;

struct JamVisualisation
{
    #define _LINE_BREAK_ON(_action) \
      _action(Never)                \
      _action(ChangedBPM)           \
      _action(ChangedScaleOrRoot)   \
      _action(TimePassing)
    REFLECT_ENUM( LineBreakOn, uint32_t, _LINE_BREAK_ON );
    #undef _LINE_BREAK_ON

    #define _RIFF_GAP_ON(_action) \
      _action(Never)                \
      _action(ChangedBPM)           \
      _action(ChangedScaleOrRoot)   \
      _action(TimePassing)
    REFLECT_ENUM( RiffGapOn, uint32_t, _RIFF_GAP_ON );
    #undef _RIFF_GAP_ON

    #define _COLOUR_STYLE(_action)  \
        _action( Uniform )          \
        _action( UserIdentity )     \
        _action( UserChangeCycle )  \
        _action( UserChangeRate )   \
        _action( StemChurn )        \
        _action( StemTimestamp )    \
        _action( Scale )            \
        _action( Root )             \
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
    float                   m_bpmMaximum        = 190.0f;
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
    {
        m_discordBotUI = std::make_unique<discord::BotWithUI>( *this, m_configDiscord );
    }

    ~LoreApp()
    {
    }

    const char* GetAppName() const override { return OUROVEON_LORE; }
    const char* GetAppNameWithVersion() const override { return (OUROVEON_LORE " " OUROVEON_LORE_VERSION); }
    const char* GetAppCacheName() const override { return "lore"; }

    int EntrypointOuro() override;

    void initMidi()
    {
        auto* midiInput = m_mdMidi->getInputControl();
        if ( midiInput != nullptr )
        {
            midiInput->getInputPorts( m_midiInputPortNames );
        }
    }

protected:

    void customMainMenu()
    {
        if ( ImGui::BeginMenu( "KERNEL" ) )
        {
            if ( ImGui::MenuItem( "Reclaim Memory" ) )
                m_stemCache.prune();

            ImGui::EndMenu();
        }

        auto* midiInput = m_mdMidi->getInputControl();
        if ( midiInput != nullptr && !m_midiInputPortNames.empty() )
        {
            uint32_t openedIndex;
            const bool hasOpenPort = midiInput->getOpenPortIndex( openedIndex );

            if ( ImGui::BeginMenu( "MIDI" ) )
            {
                for ( uint32_t inpIdx = 0; inpIdx < (uint32_t)m_midiInputPortNames.size(); inpIdx++ )
                {
                    const bool thisPortIsOpen = hasOpenPort && ( openedIndex == inpIdx );

                    if ( ImGui::MenuItem( m_midiInputPortNames[inpIdx].c_str(), nullptr, thisPortIsOpen ) )
                    {
                        if ( thisPortIsOpen )
                            midiInput->closeInputPort();
                        else
                            midiInput->openInputPort( inpIdx );
                    }
                }
                ImGui::EndMenu();
            }
        }
    }

    void customStatusBar()
    {
        uint32_t openedIndex;
        auto* midiInput = m_mdMidi->getInputControl();
        if ( midiInput != nullptr && midiInput->getOpenPortIndex( openedIndex ) )
        {
            const auto& inputPortName = m_midiInputPortNames[openedIndex];
            {
                ImGui::Scoped::ToggleButtonLit toggled;
                ImGui::Button( "MIDI" );
                ImGui::TextUnformatted( inputPortName );
            }
            ImGui::Separator();
        }
    }

    std::vector< std::string >              m_midiInputPortNames;


    // discord bot & streaming panel 
    std::unique_ptr< discord::BotWithUI >   m_discordBotUI;

#if OURO_FEATURES_VST
    // VST playground
    std::unique_ptr< effect::EffectStack >  m_effectStack;
#endif // OURO_FEATURES_VST


// riff playback management #HDD build into common module?
protected:

    using SyncAndPlaybackQueue = mcc::ReaderWriterQueue< endlesss::types::RiffCouchID >;

    // take the completed IDs posted back from the worker thread and prune them from
    // our list of 'in flight' tasks
    void synchroniseRiffWork()
    {
        endlesss::types::RiffCouchID completedRiff;
        while ( m_syncAndPlaybackCompletions.try_dequeue( completedRiff ) )
        {
            m_syncAndPlaybackInFlight.erase( 
                std::remove( m_syncAndPlaybackInFlight.begin(), m_syncAndPlaybackInFlight.end(), completedRiff ), m_syncAndPlaybackInFlight.end() );
        }
        while ( m_riffsDequedByMixer.try_dequeue( completedRiff ) )
        {
            m_riffsQueuedForPlayback.erase(
                std::remove( m_riffsQueuedForPlayback.begin(), m_riffsQueuedForPlayback.end(), completedRiff ), m_riffsQueuedForPlayback.end() );
        }
    }

    SyncAndPlaybackQueue            m_syncAndPlaybackQueue;         // riffs to fetch & play - written to by main thread, read from worker
    SyncAndPlaybackQueue            m_syncAndPlaybackCompletions;   // riffs that have been fetched & played - written to by worker, read by main thread
    endlesss::types::RiffCouchIDs   m_syncAndPlaybackInFlight;      // main thread list of work submitted to worker
    std::unique_ptr< std::thread >  m_syncAndPlaybackThread;        // the worker thread
    std::atomic_bool                m_syncAndPlaybackThreadHalt;
#ifdef OURO_CXX20_SEMA    
    std::counting_semaphore<>       m_syncAndPlaybackSem { 0 };
#endif // OURO_CXX20_SEMA

    SyncAndPlaybackQueue            m_riffsDequedByMixer;
    endlesss::types::RiffCouchIDs   m_riffsQueuedForPlayback;
    endlesss::live::RiffPtr         m_nowPlayingRiff;


    void requestRiffPlayback( const endlesss::types::RiffCouchID& riff )
    {
        blog::app( "requestRiffPlayback: {}", riff );

        m_riffsQueuedForPlayback.emplace_back( riff );      // stash it in the "queued but not playing yet" list; removed from 
                                                            // when the mixer eventually gets to playing it

        m_syncAndPlaybackInFlight.emplace_back( riff );     // log that we will be asynchronously fetching this

        m_syncAndPlaybackQueue.emplace( riff );             // push it onto the pile to be examined by the worker thread
#if OURO_CXX20_SEMA
        m_syncAndPlaybackSem.release();
#endif // OURO_CXX20_SEMA        
    }

    void handleNewRiffPlaying( endlesss::live::RiffPtr& nowPlayingRiff )
    {
        m_nowPlayingRiff = nowPlayingRiff;

        // might be a empty riff, only track actual riffs
        if ( nowPlayingRiff != nullptr )
            m_riffsDequedByMixer.emplace( m_nowPlayingRiff->m_riffData.riff.couchID );
    }


protected:
    endlesss::types::JamCouchID     m_currentViewedJam;
    std::string                     m_currentViewedJamName;
    bool                            m_enableDbJamDeletion = false;

// data and callbacks used to react to changes from the warehouse
protected:

    void handleWarehouseWorkUpdate( const bool tasksRunning, const std::string& currentTask )
    {
        m_warehouseWorkUnderway = tasksRunning;
        m_warehouseWorkState    = currentTask;
    }

    void handleWarehouseContentsReport( const endlesss::Warehouse::ContentsReport& report )
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

    std::string                             m_warehouseWorkState;
    bool                                    m_warehouseWorkUnderway;

    std::mutex                              m_warehouseContentsReportMutex;
    endlesss::Warehouse::ContentsReport     m_warehouseContentsReport;
    endlesss::types::JamCouchIDSet          m_warehouseContentsReportJamIDs;
    std::vector< std::string >              m_warehouseContentsReportJamTitles;
    std::vector< bool >                     m_warehouseContentsReportJamInFlux;
    endlesss::types::JamCouchIDSet          m_warehouseContentsReportJamInFluxSet;      // any jams that have unfetched data
    WarehouseContentsSortMode               m_warehouseContentsSortMode                 = WarehouseContentsSortMode::ByName;
    std::vector< std::size_t >              m_warehouseContentsSortedIndices;


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

    using UserHashFMap = robin_hood::unordered_flat_map< uint64_t, float >;


    JamVisualisation                        m_jamVisualisation;


    struct JamSliceSketch
    {
        using RiffToBitmapOffsetMap     = robin_hood::unordered_flat_map< endlesss::types::RiffCouchID, ImVec2, cid_hash<endlesss::types::RiffCouchID> >;
        using CellIndexToRiffMap        = robin_hood::unordered_flat_map< uint64_t, endlesss::types::RiffCouchID >;
        using CellIndexToSliceIndex     = robin_hood::unordered_flat_map< uint64_t, int32_t >;
        using LinearRiffOrder           = std::vector< endlesss::types::RiffCouchID >;


        endlesss::Warehouse::JamSlicePtr    m_slice;

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

        void prepare( endlesss::Warehouse::JamSlicePtr&& slicePtr )
        {
            m_slice = std::move( slicePtr );
        }

        void raster( gfx::Sketchbook& sketchbook, const JamVisualisation& jamVis, const int32_t viewWidth )
        {
            m_textures.clear();
            m_jamViewRenderUserHashFMap.clear();

            const endlesss::Warehouse::JamSlice& slice = *m_slice;
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
                    nameHighlightHashes[nH]     = CityHash64( jamVis.m_nameHighlighting[nH].m_name.data(), jamVis.m_nameHighlighting[nH].m_name.length() );
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
                m_textures.emplace_back( std::move( sketchbook.scheduleBufferUploadToGPU( std::move( activeSketch ) ) ) );
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
                                lastBPM != riffBPM )
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
                        addRiffGap = (riffI > 0 && slice.m_bpms[riffI - 1] != slice.m_bpms[riffI]);
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
    }                                       m_jamSliceRenderState = JamSliceRenderState::Invalidated;
    spacetime::Moment                       m_jamSliceRenderChangePendingTimer;

    endlesss::Warehouse::JamSlicePtr        m_jamSlice;
    JamSliceSketchPtr                       m_jamSliceSketch;


    void clearJamSlice()
    {
        std::scoped_lock<std::mutex> sliceLock( m_jamSliceMapLock );

        m_jamSlice              = nullptr;
        m_jamSliceRenderState   = JamSliceRenderState::Invalidated;
        m_jamSliceSketch        = nullptr;
    }

    void newJamSliceGenerated(
        const endlesss::types::JamCouchID& jamCouchID,
        endlesss::Warehouse::JamSlicePtr&& resultSlice )
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

};




// ---------------------------------------------------------------------------------------------------------------------
int LoreApp::EntrypointOuro()
{
    // create warehouse instance to manage ambient downloading
    endlesss::Warehouse warehouse( m_storagePaths.value(), m_apiNetworkConfiguration.value() );
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


    // default to viewing logged-in user in the jam view highlight
    m_jamVisualisation.m_nameHighlighting[0].m_name = m_apiNetworkConfiguration->auth().user_id;
    m_jamVisualisation.m_nameHighlighting[0].m_colour = ImVec4( 1.0f, 0.95f, 0.8f, 1.0f );


    // examine our midi state, extract port names
    initMidi();


    // create and install the mixer engine
    mix::Preview mixPreview( m_mdAudio->getMaximumBufferSize(), m_mdAudio->getSampleRate(), std::bind( &LoreApp::handleNewRiffPlaying, this, stdp::_1 ) );
    m_mdAudio->blockUntil( m_mdAudio->installMixer( &mixPreview ) );

#if OURO_FEATURES_VST
    // VSTs for audio engine
    m_effectStack = std::make_unique<effect::EffectStack>( m_mdAudio.get(), mixPreview.getTimeInfoPtr(), "preview" );
    m_effectStack->load( m_appConfigPath );
#endif // OURO_FEATURES_VST

    m_syncAndPlaybackThreadHalt = false;
    m_syncAndPlaybackThread = std::make_unique<std::thread>( [&mixPreview, &warehouse, this]()
        {
            using namespace std::chrono_literals;

            base::instr::setThreadName( OURO_THREAD_PREFIX "LORE::riff-sync" );

            endlesss::live::RiffCacheLRU liveRiffMiniCache{ 50 };
            endlesss::types::RiffCouchID riffCouchID;

            blog::app( "background riff sync thread [enter]" );
            for (;;)
            {
                if ( m_syncAndPlaybackThreadHalt )
                    return;

#if OURO_CXX20_SEMA
                if ( m_syncAndPlaybackSem.try_acquire_for( 200ms ) )
#endif // OURO_CXX20_SEMA                
                {
                    base::instr::ScopedEvent se( "riff-load", base::instr::PresetColour::Emerald );

                    if ( m_syncAndPlaybackQueue.try_dequeue( riffCouchID ) )
                    {
                        endlesss::live::RiffPtr riffToPlay;

                        // rummage through our little local cache of live riff instances to see if we can re-use one
                        if ( !liveRiffMiniCache.search( riffCouchID, riffToPlay ) )
                        {
                            endlesss::types::RiffComplete riffComplete;
                            if ( warehouse.fetchSingleRiffByID( riffCouchID, riffComplete ) )
                            {
                                riffToPlay = std::make_shared< endlesss::live::Riff >( riffComplete, m_mdAudio->getSampleRate() );
                                riffToPlay->fetch( m_apiNetworkConfiguration.value(), m_stemCache, m_taskExecutor );

                                // stash new riff in cache
                                liveRiffMiniCache.store( riffToPlay );
                            }
                        }

                        // could have failed if we passed in a naff CID that warehouse doesn't know about; pretend we played it
                        if ( riffToPlay != nullptr )
                            mixPreview.play( riffToPlay );

                        // report that we're done with this one
                        m_syncAndPlaybackCompletions.enqueue( riffCouchID );
                    }
                }
                std::this_thread::yield();
            }
            blog::app( "background riff sync thread [exit]" );
        } );

    // UI core loop begins
    const auto callbackMainMenu  = std::bind( &LoreApp::customMainMenu, this );
    const auto callbackStatusBar = std::bind( &LoreApp::customStatusBar, this );
    while ( beginInterfaceLayout( app::CoreGUI::ViewportMode::DockingViewport, callbackMainMenu, callbackStatusBar ) )
    {
        // process and blank out Exchange data ready to re-write it
        emitAndClearExchangeData();

        // run modal jam browser window if it's open
        bool selectNewJamToSyncWithModalBrowser = false;
        const char* modalJamBrowserTitle = "Select Jam To Sync";
        ux::modalUniversalJamBrowser( modalJamBrowserTitle, m_jamLibrary, warehouseJamBrowser );



        // run jam slice computation that needs to run on the main thread
        m_sketchbook.processPendingUploads();

        // tidy up any messages from our riff-sync background thread
        synchroniseRiffWork();

        ImGuiPerformanceTracker();

        {
            ImGui::Begin( "Audio" );

            // expose volume control
            {
                float volumeF = m_mdAudio->getMasterVolume() * 1000.0f;
                if ( ImGui::KnobFloat( "Volume", 24.0f, &volumeF, 0.0f, 1000.0f, 2000.0f ) )
                    m_mdAudio->setMasterVolume( volumeF * 0.001f );
            }
            ImGui::SameLine( 0, 8.0f );
            // button to toggle end-chain mute on audio engine (but leave processing, WAV output etc intact)
            const bool isMuted = m_mdAudio->isMuted();
            {
                const char* muteIcon = isMuted ? ICON_FA_VOLUME_MUTE : ICON_FA_VOLUME_UP;

                {
                    ImGui::Scoped::ToggleButton bypassOn( isMuted );
                    if ( ImGui::Button( muteIcon, ImVec2( 48.0f, 48.0f ) ) )
                        m_mdAudio->toggleMute();
                }
                ImGui::CompactTooltip( "Mute final audio output\nThis does not affect streaming or disk-recording" );
            }


            ImGui::TextUnformatted( "Disk Recorders" );
            {
                ux::widget::DiskRecorder( *m_mdAudio, m_storagePaths->outputApp );

                auto* mixRecordable = mixPreview.getRecordable();
                if ( mixRecordable != nullptr )
                    ux::widget::DiskRecorder( *mixRecordable, m_storagePaths->outputApp );
            }

#if OURO_FEATURES_VST
            ImGui::Spacing();
            ImGui::Spacing();

            m_effectStack->imgui( *m_mdFrontEnd );
#endif // OURO_FEATURES_VST

            ImGui::End();
        }



        // for rendering state from the current riff;
        // take a shared ptr copy here, just in case the riff is swapped out mid-tick
        endlesss::live::RiffPtr currentRiffPtr = m_nowPlayingRiff;

        // pour current riff into Exchange block
        endlesss::Exchange::fillDetailsFromRiff( m_endlesssExchange, currentRiffPtr, m_currentViewedJamName.c_str() );



        const auto currentRiff = currentRiffPtr.get();

        // note if the warehouse is running ops, if not then disable new-task buttons
        const bool warehouseIsPaused = warehouse.workerIsPaused();


        m_mdMidi->processMessages( []( const app::midi::Message& ){ } );

        {
            m_discordBotUI->imgui( *m_mdFrontEnd );
        }
        {
            ImGui::Begin( "Playback Engine" );

            mixPreview.imgui( m_storagePaths.value() );

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
                                            if ( ImGui::GetMergedKeyModFlags() & ImGuiKeyModFlags_Alt )
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
                                                        requestRiffPlayback( riffCouchID );

                                                        currentRiffInRange += direction;
                                                    }

                                                    m_jamSliceRangeClick = -1;
                                                }
                                            }
                                            else
                                            {
                                                const auto& riffCouchID = m_jamSliceSketch->m_slice->m_ids[m_jamSliceHoveredRiffIndex];
                                                requestRiffPlayback( riffCouchID );
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
            ImGui::Begin( "Data Warehouse" );

            {
                ImGui::Scoped::ButtonTextAlignLeft leftAlign;
                const ImVec2 toolbarButtonSize{ 140.0f, 0.0f };

                // worker thread control / status display
                {
                    const auto currentLineHeight = ImGui::GetTextLineHeight();
                    {
                        // enable or disable the worker thread
                        ImGui::Scoped::ToggleButton highlightButton( !warehouseIsPaused, true );
                        if ( ImGui::Button( warehouseIsPaused ?
                            ICON_FA_PLAY_CIRCLE  " RESUME  " :
                            ICON_FA_PAUSE_CIRCLE " RUNNING ", toolbarButtonSize ) )
                        {
                            warehouse.workerTogglePause();
                        }
                    }
                    ImGui::SameLine( 0.0f, 16.0f );

                    // show spinner if we're working, and the current reported work state
                    ImGui::Spinner( "##syncing", m_warehouseWorkUnderway, currentLineHeight * 0.4f, 3.0f, ImGui::GetColorU32( ImGuiCol_Text ) );
                    ImGui::SameLine( 0.0f, 16.0f );

                    ImGui::TextUnformatted( m_warehouseWorkState.c_str() );
                }
                // extra tools in a pile
                {
                    if ( ImGui::Button( ICON_FA_PLUS_CIRCLE " Add Jam...", toolbarButtonSize ) )
                    {
                        selectNewJamToSyncWithModalBrowser = true;
                    }
                }
                {
                    ImGui::SameLine();
                    const float warehouseViewWidth = ImGui::GetContentRegionAvail().x;
                    ImGui::SameLine( 0, warehouseViewWidth - 150.0f );
                    ImGui::Checkbox( "Enable Deletion", &m_enableDbJamDeletion );
                }
            }

            const auto dataTableBegin = []( const char* label )
            {
                if ( ImGui::BeginTable( label, 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg ) )
                {
                    ImGui::TableSetupColumn( "View",        ImGuiTableColumnFlags_WidthFixed, 32.0f );
                    ImGui::TableSetupColumn( "Jam Name",    ImGuiTableColumnFlags_WidthFixed, 320.0f );
                    ImGui::TableSetupColumn( "Sync",        ImGuiTableColumnFlags_WidthFixed, 32.0f );
                    ImGui::TableSetupColumn( "Riffs",       ImGuiTableColumnFlags_WidthFixed, 120.0f );
                    ImGui::TableSetupColumn( "Stems",       ImGuiTableColumnFlags_WidthFixed, 120.0f );
                    ImGui::TableSetupColumn( "Wipe",        ImGuiTableColumnFlags_WidthFixed, 32.0f );
                    return true;
                }
                return false;
            };

            ImGui::SeparatorBreak();
            if ( dataTableBegin("##warehouse_headers") )
            {
                ImGui::PushStyleColor( ImGuiCol_Text, ImGui::GetStyleColorVec4( ImGuiCol_ResizeGripHovered ) );
                ImGui::TableHeadersRow();
                ImGui::PopStyleColor();
                ImGui::EndTable();
            }
            static ImVec2 buttonSizeMidTable( 33.0f, 24.0f );

            if ( ImGui::BeginChild( "##data_child" ) )
            {
                if ( dataTableBegin("##warehouse_table") )
                {
                    // lock the data report so it isn't whipped away from underneath us mid-render
                    std::scoped_lock<std::mutex> reportLock( m_warehouseContentsReportMutex );

                    for ( size_t jamIdx = 0; jamIdx < m_warehouseContentsReport.m_jamCouchIDs.size(); jamIdx++ )
                    {
                        const std::size_t jI = m_warehouseContentsSortedIndices[jamIdx];
                        const bool isJamInFlux = m_warehouseContentsReportJamInFlux[jI];

                        ImGui::PushID( (int32_t)jI );

                        ImGui::TableNextColumn();
                        
                        ImGui::BeginDisabledControls( isJamInFlux );
                        if ( ImGui::PrecisionButton( ICON_FA_EYE, buttonSizeMidTable, 1.0f ) )
                        {
                            clearJamSlice();

                            // change which jam we're viewing, reset active riff hover in the process as this will invalidate it
                            m_currentViewedJam          = m_warehouseContentsReport.m_jamCouchIDs[jI];
                            m_currentViewedJamName      = m_warehouseContentsReportJamTitles[jI];
                            m_jamSliceHoveredRiffIndex  = -1;

                            warehouse.addJamSliceRequest( m_currentViewedJam, std::bind( &LoreApp::newJamSliceGenerated, this, std::placeholders::_1, std::placeholders::_2 ) );
                        }
                        ImGui::EndDisabledControls( isJamInFlux );

                        ImGui::TableNextColumn(); 
                        
                        ImGui::Dummy( ImVec2( 0.0f, 1.0f ) );
                        ImGui::TextDisabled( "ID" );
                        ImGui::CompactTooltip( m_warehouseContentsReport.m_jamCouchIDs[jI].c_str() );
                        ImGui::SameLine();
                        ImGui::TextUnformatted( m_warehouseContentsReportJamTitles[jI].c_str() );


                        ImGui::TableNextColumn();


                        ImGui::BeginDisabledControls( isJamInFlux );
                        if ( ImGui::PrecisionButton( ICON_FA_SYNC, buttonSizeMidTable, 1.0f ) )
                        {
                            warehouse.addOrUpdateJamSnapshot( m_warehouseContentsReport.m_jamCouchIDs[jI] );
                        }
                        ImGui::EndDisabledControls( isJamInFlux );


                        ImGui::TableNextColumn();
                        ImGui::Dummy( ImVec2( 0.0f, 1.0f ) );
                        {
                            const auto unpopulated = m_warehouseContentsReport.m_unpopulatedRiffs[jI];
                            const auto populated   = m_warehouseContentsReport.m_populatedRiffs[jI];

                            if ( unpopulated > 0 )
                                ImGui::Text( "%" PRIi64 " (+%" PRIi64 ")", populated, unpopulated );
                            else
                                ImGui::Text( "%" PRIi64, populated);
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

                        ImGui::TableNextColumn();
                        ImGui::BeginDisabledControls( !m_enableDbJamDeletion || isJamInFlux );
                        if ( ImGui::PrecisionButton( ICON_FA_TRASH, buttonSizeMidTable ) )
                        {
                            warehouse.requestJamPurge( m_warehouseContentsReport.m_jamCouchIDs[jI] );
                        }
                        ImGui::EndDisabledControls( !m_enableDbJamDeletion || isJamInFlux );

                        ImGui::PopID();
                    }

                    ImGui::EndTable();
                }
            }
            ImGui::EndChild();

            ImGui::End();
        }


        if ( selectNewJamToSyncWithModalBrowser )
        {
            // create local copy of the current warehouse jam ID map for use by the popup; avoids
            // having to worry about warehouse contents shifting underneath / locking mutex in dialog
            {
                std::scoped_lock<std::mutex> reportLock( m_warehouseContentsReportMutex );
                warehouseJamBrowser.m_warehouseJamIDs = m_warehouseContentsReportJamIDs;
            }
            ImGui::OpenPopup( modalJamBrowserTitle );
        }

        submitInterfaceLayout();
    }

    m_syncAndPlaybackThreadHalt = true;
    m_syncAndPlaybackThread->join();
    m_syncAndPlaybackThread.reset();

    m_discordBotUI.reset();

    // remove the mixer and ensure the async op has taken before leaving scope
    m_mdAudio->blockUntil( m_mdAudio->installMixer( nullptr ) );

#if OURO_FEATURES_VST
    // serialize effects
    m_effectStack->save( m_appConfigPath );
    m_effectStack.reset();
#endif // OURO_FEATURES_VST


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