#include "pch.h"

#include "ispc/.gen/colour_ispc.gen.h"

#include "base/utils.h"
#include "base/buffer2d.h"
#include "base/metaenum.h"

#include "config/frontend.h"
#include "config/data.h"
#include "config/audio.h"

#include "app/module.audio.h"
#include "app/module.frontend.h"
#include "app/module.frontend.fonts.h"

#include "app/ouro.h"

#include "discord/discord.bot.ui.h"

#include "endlesss/all.h"

#include "effect/effect.stack.h"

#include "mix/preview.h"


#define OUROVEON_LORE           "LORE"
#define OUROVEON_LORE_VERSION   "0.6.0-alpha"

inline const char* getGlErrorText( GLenum err )
{
    switch ( err )
    {
    case GL_NO_ERROR:                       return "GL_NO_ERROR";
    case GL_INVALID_ENUM:                   return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE:                  return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION:              return "GL_INVALID_OPERATION";
    case GL_INVALID_FRAMEBUFFER_OPERATION:  return "GL_INVALID_FRAMEBUFFER_OPERATION";
    case GL_OUT_OF_MEMORY:                  return "GL_OUT_OF_MEMORY";
    case GL_STACK_UNDERFLOW:                return "GL_STACK_UNDERFLOW";
    case GL_STACK_OVERFLOW:                 return "GL_STACK_OVERFLOW";
    }
    return "Unknown Error";
}

#define glChecked( ... )    \
        __VA_ARGS__;        \
        { \
            const auto glErr = glGetError(); \
            if ( glErr != GL_NO_ERROR ) \
            { \
                blog::error::app( "{}:{} OpenGL call error [{}] : {}", __FUNCTION__, __LINE__, glErr, getGlErrorText(glErr) ); \
                blog::error::app( "{}", #__VA_ARGS__ ); \
            } \
        }



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

    std::string             m_nameHighlight;

    LineBreakOn::Enum       m_lineBreakOn       = LineBreakOn::ChangedBPM;
    RiffGapOn::Enum         m_riffGapOn         = RiffGapOn::Never;
    ColourStyle::Enum       m_colourStyle       = ColourStyle::UserChangeRate;
    ColourSource::Enum      m_colourSource      = ColourSource::GradientPlasma;

    // defaults that can be then tuned on the UI depending which colour viz is running
    float                   m_bpmMinimum        = 50.0f;
    float                   m_bpmMaximum        = 190.0f;
    float                   m_activityTimeSec   = 30.0f;
    float                   m_changeRateDecay   = 0.8f;

    inline uint32_t colourSampleT( const float t )
    {
        switch ( m_colourSource )
        {
            case ColourSource::GradientGrayscale:       return ispc::gradient_grayscale_u32( t );
            case ColourSource::GradientBlueOrange:      return ispc::gradient_blueorange_u32( t );
            case ColourSource::GradientPlasma:          return ispc::gradient_plasma_u32( t );
            case ColourSource::GradientRainbowVibrant:  return ispc::gradient_rainbow_ultra_u32( t );
            case ColourSource::GradientRainbow:         return ispc::gradient_rainbow_u32( t );
            case ColourSource::GradientTurbo:           return ispc::gradient_turbo_u32( t );
            case ColourSource::GradientViridis:         return ispc::gradient_viridis_u32( t );
            case ColourSource::GradientMagma:           return ispc::gradient_magma_u32( t );
            default:
                return 0xFF00FFFF;
        }
    }

    inline bool imgui()
    {
        bool choiceChanged = false;

        ImGui::Columns( 2, nullptr, false );
        ImGui::PushItemWidth( 200.0f );

        choiceChanged |= ImGui::InputText( "Name Highlight", &m_nameHighlight, ImGuiInputTextFlags_EnterReturnsTrue );
                         ImGui::CompactTooltip( "enter an Endlesss username to have their riffs highlighted in the view" );
        choiceChanged |= LineBreakOn::ImGuiCombo( "Line Break", m_lineBreakOn );
                         ImGui::CompactTooltip( "Add a line-break between blocks of riffs, based on configurable differences" );
        choiceChanged |= RiffGapOn::ImGuiCombo( "Riff Gap", m_riffGapOn );
                         ImGui::CompactTooltip( "Add one-block gaps between riffs, based on configurable differences" );

        ImGui::NextColumn();

        choiceChanged |= ColourSource::ImGuiCombo( "Colour Source", m_colourSource );
        choiceChanged |= ColourStyle::ImGuiCombo( "Colour Style", m_colourStyle );

        if ( m_colourStyle == ColourStyle::BPM )
            choiceChanged |= ImGui::DragFloatRange2( "BPM Range", &m_bpmMinimum, &m_bpmMaximum, 1.0f, 25.0f, 999.0f, "%.0f" );
        else if ( m_colourStyle == ColourStyle::StemChurn )
            choiceChanged |= ImGui::InputFloat( "Cooldown Time", &m_activityTimeSec, 1.0f, 5.0f, " %.0f Seconds" );
        else if ( m_colourStyle == ColourStyle::UserChangeRate )
            choiceChanged |= ImGui::InputFloat( "Decay Rate", &m_changeRateDecay, 0.05f, 0.1f, " %.2f" );
        else
            ImGui::TextUnformatted( "" );


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
        for ( const auto& [key, value] : m_jamSliceMap )
        {
            delete value;
        }
    }

    const char* GetAppName() const override { return OUROVEON_LORE; }
    const char* GetAppNameWithVersion() const override { return (OUROVEON_LORE " " OUROVEON_LORE_VERSION); }
    const char* GetAppCacheName() const override { return "lore"; }

    int EntrypointOuro() override;




    void requestRiffPlayback( const endlesss::types::RiffCouchID& riff )
    {
        blog::app( "requestRiffPlayback: {}", riff );

        m_riffsQueuedForPlayback.emplace_back( riff );      // stash it in the "queued but not playing yet" list; removed from 
                                                            // when the mixer eventually gets to playing it

        m_syncAndPlaybackInFlight.emplace_back( riff );     // log that we will be asynchronously fetching this
        m_syncAndPlaybackQueue.emplace( riff );             // push it onto the pile to be examined by the worker thread
    }

protected:
    void MainMenuCustom() override
    {
        if ( ImGui::BeginMenu( "LORE" ) )
        {
            if ( ImGui::MenuItem( "Reclaim Memory" ) )
                m_stemCache.prune();

            ImGui::EndMenu();
        }
    }

protected:
    std::unique_ptr< discord::BotWithUI >   m_discordBotUI;


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
    std::unique_ptr< std::jthread > m_syncAndPlaybackThread;        // the worker thread

    SyncAndPlaybackQueue            m_riffsDequedByMixer;
    endlesss::types::RiffCouchIDs   m_riffsQueuedForPlayback;
    endlesss::live::RiffPtr         m_nowPlayingRiff;

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
        m_warehouseWorkState = currentTask;
    }

    void handleWarehouseContentsReport( const endlesss::Warehouse::ContentsReport& report )
    {
        // this data is presented by the UI so for safety at the moment we just do a big chunky lock around it
        // before wiping it out and rewriting
        std::scoped_lock<std::mutex> reportLock( m_warehouseContentsReportMutex );

        m_warehouseContentsReport = report;
        m_warehouseContentsReportJamTitles.clear();
        m_warehouseContentsReportJamInFlux.clear();
        m_warehouseContentsReportJamInFluxSet.clear();

        for ( auto jIdx = 0; jIdx < m_warehouseContentsReport.m_jamCouchIDs.size(); jIdx++ )
        {
            const auto& jamCID = m_warehouseContentsReport.m_jamCouchIDs[jIdx];

            std::size_t jamIndex;
            m_jamLibrary.getIndexForDatabaseID( jamCID, jamIndex );

            m_warehouseContentsReportJamTitles.emplace_back( m_jamLibrary.getDisplayName( jamIndex ) );

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
    std::vector< std::string >              m_warehouseContentsReportJamTitles;
    std::vector< bool >                     m_warehouseContentsReportJamInFlux;
    endlesss::types::JamCouchIDSet          m_warehouseContentsReportJamInFluxSet;      // any jams that have unfetched data
    WarehouseContentsSortMode               m_warehouseContentsSortMode                 = WarehouseContentsSortMode::ByName;
    std::vector< std::size_t >              m_warehouseContentsSortedIndices;

public:

    bool isJamBeingSynced( const endlesss::types::JamCouchID& jamID )
    {
        return m_warehouseContentsReportJamInFluxSet.contains( jamID );
    }


// 
protected:

    struct JamSliceCapture
    {
        using RiffToBitmapOffsetMap     = robin_hood::unordered_flat_map< endlesss::types::RiffCouchID, ImVec2, cid_hash<endlesss::types::RiffCouchID> >;
        using CellIndexToRiffMap        = robin_hood::unordered_flat_map< uint64_t, endlesss::types::RiffCouchID >;
        using CellIndexToSliceIndex     = robin_hood::unordered_flat_map< uint64_t, int32_t >;
        using LinearRiffOrder           = std::vector< endlesss::types::RiffCouchID >;


        ~JamSliceCapture()
        {
            if ( hasValidBitmapTexture() )
                glDeleteTextures( 1, &m_bitmapHandle );

            delete m_bitmapBuffer;
        }

        endlesss::Warehouse::JamSlice   m_slice;

        RiffToBitmapOffsetMap           m_riffToBitmapOffset;
        CellIndexToRiffMap              m_cellIndexToRiff;
        CellIndexToSliceIndex           m_cellIndexToSliceIndex;
        LinearRiffOrder                 m_riffOrderLinear;

        std::vector< float >            m_labelY;
        std::vector< std::string >      m_labelText;

        int32_t                         m_builtFromViewWidth    = -1;
        int32_t                         m_cellColumns           = -1;
        int32_t                         m_cellRows              = -1;
        ImVec2                          m_cellDimV2;

        base::U32Buffer*                m_bitmapBuffer          = nullptr;
        uint32_t                        m_bitmapHandle          = 0;
        int32_t                         m_bitmapWidth           = 0;
        int32_t                         m_bitmapHeight          = 0;
        ImVec2                          m_bitmapDimV2;
        ImVec2                          m_bitmapCellUV;                 // UV for drawing just the cell-filled area of the image

        inline bool hasRawBitmapBufferData() const { return m_bitmapBuffer != nullptr; }
        inline bool hasValidBitmapTexture() const { return glIsTexture( m_bitmapHandle ); }

        bool createGLTexture()
        {
            if ( !hasRawBitmapBufferData() )
                return false;

            glChecked( glGenTextures( 1, &m_bitmapHandle ) );
            glChecked( glBindTexture( GL_TEXTURE_2D, m_bitmapHandle ) );

            if ( !hasValidBitmapTexture() )
            {
                return false;
            }

            glChecked( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR ) );
            glChecked( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR ) );
            glChecked( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE ) );
            glChecked( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE ) );

            glChecked( glPixelStorei( GL_UNPACK_ROW_LENGTH, m_bitmapBuffer->getWidth() ) );
            glChecked( glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, m_bitmapBuffer->getWidth(), m_bitmapBuffer->getHeight(), 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, m_bitmapBuffer->getBuffer() ) );

            delete m_bitmapBuffer;
            m_bitmapBuffer = nullptr;

            return true;
        }
    };
    using JamSliceMap = robin_hood::unordered_flat_map< endlesss::types::JamCouchID, JamSliceCapture*, cid_hash<endlesss::types::JamCouchID> >;

    void updateAnyPendingJamSlices()
    {
        for ( const auto& [key, value] : m_jamSliceMap )
        {
            if ( value->hasRawBitmapBufferData() )
                value->createGLTexture();
        }
    }



    std::unique_ptr<effect::EffectStack>    m_effectStack;

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
                }
            }

            m_jamViewWidthToCommit = iWidth;
        }
    }

    std::mutex                              m_jamSliceMapLock;
    JamSliceMap                             m_jamSliceMap;

    // which riff the mouse is currently hovered over in the active jam slice; or -1 if not valid
    int32_t                                 m_jamSliceHoveredRiffIndex = -1;

    // hax; alt-clicking lets us operate on a range of riffs (ie for lining up a big chunk of the jam) - if this is >=0 this was the last alt-clicked riff
    int32_t                                 m_jamSliceRangeClick = -1;

    int32_t                                 m_jamViewWidth = 0;
    int32_t                                 m_jamViewWidthToCommit = 0;
    int32_t                                 m_jamViewWidthCommitCount = -1;

    static constexpr int32_t                cJamBitmapGridCell  = 16;

    using UserHashFMap = robin_hood::unordered_flat_map< uint64_t, float >;

    UserHashFMap                            m_jamViewRenderUserHashFMap;

    JamVisualisation                        m_jamVisualisation;

    void handleJamSlice( 
        const endlesss::types::JamCouchID&   jamCouchID, 
        const endlesss::Warehouse::JamSlice& resultSlice )
    {
        perf::TimingPoint stemTiming( "jam view bitmap" );

        const uint64_t namehash = CityHash64( m_jamVisualisation.m_nameHighlight.data(), m_jamVisualisation.m_nameHighlight.length() );

        m_jamViewRenderUserHashFMap.clear();


        JamSliceCapture* newCapture = new JamSliceCapture();
        newCapture->m_slice = std::move(resultSlice);

        const int32_t totalRiffs = (int32_t)resultSlice.m_ids.size();

        newCapture->m_riffToBitmapOffset.reserve( totalRiffs );
        newCapture->m_cellIndexToRiff.reserve( totalRiffs );
        newCapture->m_cellIndexToSliceIndex.reserve( totalRiffs );
        newCapture->m_riffOrderLinear.reserve( totalRiffs );

        newCapture->m_builtFromViewWidth = m_jamViewWidth;

        newCapture->m_cellColumns        = std::max( 16, (int32_t)std::floor( (m_jamViewWidth - cJamBitmapGridCell) / (float)cJamBitmapGridCell ) );
        newCapture->m_cellRows           = (int32_t)( 1 + ( totalRiffs / newCapture->m_cellColumns ) );

        newCapture->m_bitmapWidth  = (int32_t)base::nextPow2( newCapture->m_cellColumns * cJamBitmapGridCell );
        newCapture->m_bitmapHeight = m_mdFrontEnd->getLargestTextureDim();
        newCapture->m_bitmapBuffer = new base::U32Buffer{ newCapture->m_bitmapWidth, newCapture->m_bitmapHeight };
        newCapture->m_bitmapDimV2  = ImVec2{ (float)newCapture->m_bitmapWidth, (float)newCapture->m_bitmapHeight };

        blog::app( "building jam bitmap; {} columns ({} px), {} rows ({} px) - image [{}, {}]",
            newCapture->m_cellColumns,
            newCapture->m_cellColumns * cJamBitmapGridCell,
            newCapture->m_cellRows,
            newCapture->m_cellRows * cJamBitmapGridCell,
            newCapture->m_bitmapWidth,
            newCapture->m_bitmapHeight );

        int32_t cellX = 0;
        int32_t cellY = 0;

        const auto addLineBreak = [&cellX, &cellY, newCapture]( std::string label )
        {
            // only increment twice if the line feed hadn't just happened
            if ( cellX != 0 )
                cellY++;

            newCapture->m_labelY.emplace_back( (float)(cellY * cJamBitmapGridCell) );
            newCapture->m_labelText.emplace_back( std::move( label ) );

            cellX = 0;
            cellY++;
        };

        float    lastBPM        = 0;
        uint32_t lastRoot       = 0;
        uint32_t lastScale      = 0;
        uint64_t lastUserHash   = 0;
        uint8_t  lastDay        = 0;

        float    runningColourV = 0;


        base::U32Buffer& bitmapData = *newCapture->m_bitmapBuffer;
        for ( auto riffI = 0; riffI < totalRiffs; riffI++ )
        {
            {
                const float    riffBPM      = resultSlice.m_bpms[riffI];
                const uint32_t riffRoot     = resultSlice.m_roots[riffI];
                const uint32_t riffScale    = resultSlice.m_scales[riffI];
                const auto     riffDay      = base::spacetime::getDayIndex( resultSlice.m_timestamps[riffI] );

                switch ( m_jamVisualisation.m_lineBreakOn )
                {
                    default:
                    case JamVisualisation::LineBreakOn::Never:
                        break;

                    case JamVisualisation::LineBreakOn::ChangedBPM:
                    {
                        if ( riffI == 0             ||
                             lastBPM != riffBPM )
                        {
                            addLineBreak( fmt::format( "-=> {} BPM", riffBPM ) );
                        }
                    }
                    break;

                    case JamVisualisation::LineBreakOn::ChangedScaleOrRoot:
                    {
                        if ( riffI == 0             ||
                             lastRoot  != riffRoot  ||
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
                            addLineBreak( base::spacetime::datestampStringFromUnix( resultSlice.m_timestamps[riffI] ) );
                        }
                    }
                    break;
                }

                lastBPM   = riffBPM;
                lastRoot  = riffRoot;
                lastScale = riffScale;
                lastDay   = riffDay;
            }

            bool addRiffGap = false;
            switch ( m_jamVisualisation.m_riffGapOn )
            {
                default:
                case JamVisualisation::RiffGapOn::Never:
                    break;

                case JamVisualisation::RiffGapOn::ChangedBPM:
                    addRiffGap = ( riffI > 0 && resultSlice.m_bpms[riffI - 1] != resultSlice.m_bpms[riffI] );
                    break;
                case JamVisualisation::RiffGapOn::ChangedScaleOrRoot:
                    addRiffGap = ( riffI > 0 && ( resultSlice.m_roots[riffI - 1]  != resultSlice.m_roots[riffI]
                                             ||   resultSlice.m_scales[riffI - 1] != resultSlice.m_scales[riffI] ) );
                    break;
                case JamVisualisation::RiffGapOn::TimePassing:
                    addRiffGap = ( resultSlice.m_deltaSeconds[riffI] > 60 * 60 ); // hardwired to an hour at the moment
                    break;
            }
            if ( addRiffGap )
            {
                cellX++;
                if ( cellX >= newCapture->m_cellColumns )
                {
                    cellX = 0;
                    cellY++;
                }
            }


            const uint64_t cellIndex = (uint64_t)cellX | ((uint64_t)cellY << 32);
            const uint64_t userHash  = newCapture->m_slice.m_userhash[riffI];

            float colourT = 0.0f;

            switch ( m_jamVisualisation.m_colourStyle )
            {
                case JamVisualisation::ColourStyle::Uniform:
                    colourT = 0.25f;
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
                        runningColourV *= m_jamVisualisation.m_changeRateDecay;

                    colourT = runningColourV;
                }
                break;

                case JamVisualisation::ColourStyle::StemChurn:
                {
                    const float shiftRate = (float)std::clamp( 
                        m_jamVisualisation.m_activityTimeSec - resultSlice.m_deltaSeconds[riffI],
                        0.0f,
                        m_jamVisualisation.m_activityTimeSec ) / m_jamVisualisation.m_activityTimeSec;

                    colourT = ( 0.1f + (shiftRate * shiftRate) );
                }
                break;

                case JamVisualisation::ColourStyle::StemTimestamp:
                {
                    const auto stemTp = resultSlice.m_timestamps[riffI];

                    auto dp     = date::floor<date::days>( stemTp );
                    auto ymd    = date::year_month_day{ dp };
                    auto time   = date::make_time( std::chrono::duration_cast<std::chrono::milliseconds>(stemTp - dp) );

                    colourT = (float)time.hours().count() / 24.0f;
                }
                break;

                case JamVisualisation::ColourStyle::Scale:
                {
                    colourT = ( (float)resultSlice.m_scales[riffI] / 17.0f );
                }
                break;

                case JamVisualisation::ColourStyle::Root:
                {
                    colourT = ( (float)resultSlice.m_roots[riffI] / 12.0f );
                }
                break;

                case JamVisualisation::ColourStyle::BPM:
                {
                    const float shiftRate = (float)std::clamp(
                        resultSlice.m_bpms[riffI] - m_jamVisualisation.m_bpmMinimum,
                        0.0f,
                        m_jamVisualisation.m_bpmMaximum ) / (m_jamVisualisation.m_bpmMaximum - m_jamVisualisation.m_bpmMinimum);

                    colourT = shiftRate;
                }
                break;
            }

            lastUserHash = userHash;



            const auto cellRiffCouchID = newCapture->m_slice.m_ids[riffI];
            newCapture->m_cellIndexToRiff.try_emplace( cellIndex, cellRiffCouchID );
            newCapture->m_cellIndexToSliceIndex.try_emplace( cellIndex, riffI );
            newCapture->m_riffOrderLinear.emplace_back( cellRiffCouchID );


            const int32_t cellPixelX = cellX * cJamBitmapGridCell;
            const int32_t cellPixelY = cellY * cJamBitmapGridCell;

            newCapture->m_riffToBitmapOffset.try_emplace( cellRiffCouchID, ImVec2{ (float)cellPixelX, (float)cellPixelY } );


            uint32_t cellColour = m_jamVisualisation.colourSampleT( colourT );

            

            const bool highlight = (newCapture->m_slice.m_userhash[riffI] == namehash);

            for ( auto cellWriteY = 0; cellWriteY < cJamBitmapGridCell; cellWriteY++ )
            {
                for ( auto cellWriteX = 0; cellWriteX < cJamBitmapGridCell; cellWriteX++ )
                {
                    const bool edge0 = ( cellWriteX == 0 ||
                                         cellWriteY == 0 ||
                                         cellWriteX == cJamBitmapGridCell - 1 ||
                                         cellWriteY == cJamBitmapGridCell - 1 );
                    const bool edge1 = ( cellWriteX == 1 ||
                                         cellWriteY == 1 ||
                                         cellWriteX == cJamBitmapGridCell - 2 ||
                                         cellWriteY == cJamBitmapGridCell - 2 );

                    if ( edge0 )
                    {
                        if ( highlight )
                        {
                            bitmapData(
                                cellPixelX + cellWriteX,
                                cellPixelY + cellWriteY ) = 0xFFFFFFEE;
                        }
                    }
                    else if ( edge1 )
                    {

                    }
                    else
                    {
                        bitmapData(
                            cellPixelX + cellWriteX,
                            cellPixelY + cellWriteY ) = cellColour;
                    }
                }
            }

            // move our cell target along, wrap at edges
            cellX++;
            if ( cellX >= newCapture->m_cellColumns )
            {
                cellX = 0;
                cellY++;
            }
        }

        cellY++;
        newCapture->m_cellDimV2 = ImVec2{ (float)(newCapture->m_cellColumns * cJamBitmapGridCell),
                                          (float)(                    cellY * cJamBitmapGridCell) };
        newCapture->m_bitmapCellUV = newCapture->m_cellDimV2 / newCapture->m_bitmapDimV2;

        {
            std::scoped_lock<std::mutex> sliceLock( m_jamSliceMapLock );
            if ( m_jamSliceMap.contains( jamCouchID ) )
            {
                JamSliceCapture* oldMap = m_jamSliceMap[jamCouchID];
                m_jamSliceMap.erase( jamCouchID );

                delete oldMap;
                m_jamSliceMap[jamCouchID] = newCapture;
            }
            else
            {
                m_jamSliceMap.try_emplace( jamCouchID, newCapture );
            }
        }
    }
};

struct RiffCacheLRU
{
    RiffCacheLRU( std::size_t cacheSize )
        : m_cacheSize( cacheSize )
        , m_used( 0 )
    {
        m_cache.reserve( m_cacheSize );
        m_age.reserve( m_cacheSize );

        for ( auto i = 0; i< m_cacheSize; i++ )
        {
            m_cache.emplace_back( nullptr );
            m_age.emplace_back( 0 );
        }
    }

    inline bool search( const endlesss::types::RiffCouchID& cid, endlesss::live::RiffPtr& result )
    {
        // early out on empty cache
        if ( m_used == 0 )
            return false;

        // compare CIDs by hash
        const auto incomingRiffCIDHash = endlesss::live::Riff::computeHashForRiffCID( cid );

        for ( auto idx = 0; idx < m_used; idx++ )
        {
            assert( m_cache[idx] != nullptr );

            m_age[idx] ++;

            if ( m_cache[idx]->getCIDHash() == incomingRiffCIDHash )
            {
                // reset age
                m_age[idx] = 0;

                result = m_cache[idx];

                // complete rest of run before we're done
                idx++;
                for ( ; idx < m_used; idx++ )
                    m_age[idx] ++;

                debugLog( "search-hit" );

                return true;
            }
        }
        return false;
    }

    inline void store( endlesss::live::RiffPtr& riffPtr )
    {
        if ( m_used < m_cacheSize )
        {
            // store new value in unused slot
            m_cache[m_used] = riffPtr;
            
            // age all other entries
            for ( auto prvI = 0; prvI < m_used; prvI++ )
                m_age[prvI] ++;

            m_used++;

            debugLog( "store-unfilled" );
            return;
        }

        int32_t oldestAge = -1;
        int32_t oldestIndex = -1;
        for ( auto idx = 0; idx < m_cacheSize; idx++ )
        {
            if ( m_age[idx] > oldestAge )
            {
                oldestAge   = m_age[idx];
                oldestIndex = idx;
            }

            // age as we go
            m_age[idx] ++;
        }
        assert( oldestIndex >= 0 );

        m_cache[oldestIndex] = riffPtr;
        m_age[oldestIndex] = 0;

        debugLog("store-added-new");
    }

    inline void debugLog(const std::string& context)
    {
#ifdef _DEBUG
        for ( auto idx = 0; idx < m_used; idx++ )
        {
            blog::app( "[R$] [{:30}] {} = {}, {}", context, idx, m_age[idx], (m_cache[idx] == nullptr) ? "NONE" : (m_cache[idx]->m_riffData.riff.couchID) );
        }
#endif // _DEBUG
    }

    std::size_t                              m_cacheSize;
    std::vector< endlesss::live::RiffPtr >   m_cache;
    std::vector< int32_t >                   m_age;
    int32_t                                  m_used = 0;
};



// ---------------------------------------------------------------------------------------------------------------------
int LoreApp::EntrypointOuro()
{
    // create warehouse instance to manage ambient downloading
    endlesss::Warehouse warehouse( m_storagePaths.value(), m_apiNetworkConfiguration.value() );
    warehouse.syncFromJamCache( m_jamLibrary );
    warehouse.setCallbackWorkReport( std::bind( &LoreApp::handleWarehouseWorkUpdate, this, stdp::_1, stdp::_2 ) );
    warehouse.setCallbackContentsReport( std::bind( &LoreApp::handleWarehouseContentsReport, this, stdp::_1 ) );


    // spin up stem cache manager
    if ( !m_stemCache.initialise( m_storagePaths->cacheCommon, m_mdAudio->getSampleRate() ) )
        return -1;

    // default to viewing logged-in user in the jam view highlight
    m_jamVisualisation.m_nameHighlight = m_apiNetworkConfiguration->auth().user_id;


    // create and install the mixer engine
    mix::Preview mixPreview( m_mdAudio->getMaximumBufferSize(), m_mdAudio->getSampleRate(), std::bind( &LoreApp::handleNewRiffPlaying, this, stdp::_1 ) );
    m_mdAudio->blockUntil( m_mdAudio->installMixer( &mixPreview ) );

    // VSTs for audio engine
    m_effectStack = std::make_unique<effect::EffectStack>( m_mdAudio.get(), mixPreview.getTimeInfoPtr(), "preview" );
    m_effectStack->load( m_appConfigPath );

    m_syncAndPlaybackThread = std::make_unique<std::jthread>( [&mixPreview, &warehouse, this]( std::stop_token stoken )
        {
            RiffCacheLRU liveRiffMiniCache{ 24 };

            blog::app( "background riff sync thread [enter]" );
            for (;;)
            {
                if ( stoken.stop_requested() ) 
                    return;

                endlesss::types::RiffCouchID riffCouchID;
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

                    // could have failed if we passed in a naff CID that warehouse doesn't know about
                    if ( riffToPlay != nullptr )
                        mixPreview.play( riffToPlay );

                    // report that we're done with this one
                    m_syncAndPlaybackCompletions.enqueue( riffCouchID );
                }
            }
            blog::app( "background riff sync thread [exit]" );
        } );

    while ( MainLoopBegin() )
    {
        // run modal jam browser window if it's open
        bool selectNewJamToSyncWithModalBrowser = false;
        const char* modalJamBrowserTitle = "Select Jam To Sync";
        modalJamBrowser(
            modalJamBrowserTitle,
            m_jamLibrary,
            nullptr,
            [&]( const endlesss::types::JamCouchID& newJamCID ) { warehouse.addJamSnapshot( newJamCID ); }
        );


        // run jam slice computation that needs to run on the main thread
        updateAnyPendingJamSlices();

        // tidy up any messages from our riff-sync background thread
        synchroniseRiffWork();

        ImGuiPerformanceTracker();

        {
            ImGui::Begin( "Audio" );

            const auto panelRegionAvailable = ImGui::GetContentRegionAvail();
            const auto panelVolumeModule    = 65.0f;


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
                ImGui_DiskRecorder( *m_mdAudio );

                auto* mixRecordable = mixPreview.getRecordable();
                if ( mixRecordable != nullptr )
                    ImGui_DiskRecorder( *mixRecordable );
            }

            ImGui::Spacing();
            ImGui::Spacing();

            m_effectStack->imgui( *m_mdFrontEnd );

            ImGui::End();
        }



        // for rendering state from the current riff;
        // take a shared ptr copy here, just in case the riff is swapped out mid-tick
        endlesss::live::RiffPtr currentRiffPtr = m_nowPlayingRiff;
        const auto currentRiff = currentRiffPtr.get();

        m_endlesssExchange.clear();
        m_endlesssExchange.m_live = currentRiffPtr != nullptr;
        if ( m_endlesssExchange.m_live )
        {
            endlesss::Exchange::populatePartialFromRiffPtr( currentRiffPtr, m_endlesssExchange );
            strncpy( m_endlesssExchange.m_jamName, m_currentViewedJamName.c_str(), endlesss::Exchange::MaxJamName - 1 );
        }


        // note if the warehouse is running ops, if not then disable new-task buttons
        const bool warehouseIsPaused = warehouse.workerIsPaused();


        {
            ImGui::Begin( "System" );

            ImGui_AppHeader();

            ImGui::End();
        }
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
            }


            if ( !m_currentViewedJam.empty() )
            {
                if ( ImGui::Button( "Update Display" ) )
                {
                    warehouse.addJamSliceRequest( m_currentViewedJam, std::bind( &LoreApp::handleJamSlice, this, std::placeholders::_1, std::placeholders::_2 ) );
                }

                ImGui::SameLine();
                ImGui::TextUnformatted( m_currentViewedJamName.c_str() );
            }


            if ( ImGui::BeginChild( "##jam_browser" ) )
            {
                if ( isJamBeingSynced( m_currentViewedJam ) )
                {
                    ImGui::TextUnformatted( "Jam being synced ...");
                }
                else
                {
                    std::scoped_lock<std::mutex> sliceLock( m_jamSliceMapLock );

                    // if this is not set after plotting the jam view / checking mouse coordinates etc then it 
                    // automatically sets [m_jamSliceHoveredRiffIndex] to -1 to show we have no valid view of a specific riff
                    bool updatedHoveredRiffIndex = false;

                    const auto sliceIt = m_jamSliceMap.find( m_currentViewedJam );
                    if ( sliceIt != m_jamSliceMap.end() )
                    {
                        const auto& jamSlice = *(sliceIt->second);
                        if ( jamSlice.hasValidBitmapTexture() )
                        {
                            ImVec2 pos = ImGui::GetCursorScreenPos();

                            ImDrawList* draw_list = ImGui::GetWindowDrawList();

                            ImGui::Image( (void*)(intptr_t)jamSlice.m_bitmapHandle, jamSlice.m_cellDimV2, ImVec2( 0, 0 ), jamSlice.m_bitmapCellUV );

                            for ( size_t lb = 0; lb < jamSlice.m_labelY.size(); lb++ )
                            {
                                draw_list->AddText( ImVec2{ pos.x, pos.y + jamSlice.m_labelY[lb] }, 0x80ffffff, jamSlice.m_labelText[lb].c_str() );
                            }

                            const bool is_active = ImGui::IsItemActive();
                            const bool is_hovered = ImGui::IsItemHovered();
                            ImGuiIO& io = ImGui::GetIO();
                            const auto mouseToCenterX = io.MousePos.x - pos.x;
                            const auto mouseToCenterY = io.MousePos.y - pos.y;

                            if ( is_hovered )
                            {
                                const auto cellX = (int32_t)std::floor( mouseToCenterX / (float)cJamBitmapGridCell );
                                const auto cellY = (int32_t)std::floor( mouseToCenterY / (float)cJamBitmapGridCell );

                                const uint64_t cellIndex = (uint64_t)cellX | ((uint64_t)cellY << 32);

                                // check and set which riff we are hovering over, if any
                                const auto& indexHovered = jamSlice.m_cellIndexToSliceIndex.find( cellIndex );
                                if ( indexHovered != jamSlice.m_cellIndexToSliceIndex.end() )
                                {
                                    m_jamSliceHoveredRiffIndex = indexHovered->second;
                                    updatedHoveredRiffIndex = true;

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
                                                    const auto& riffCouchID = jamSlice.m_slice.m_ids[currentRiffInRange];
                                                    requestRiffPlayback( riffCouchID );

                                                    currentRiffInRange += direction;
                                                }

                                                m_jamSliceRangeClick = -1;
                                            }
                                        }
                                        else
                                        {
                                            const auto& riffCouchID = jamSlice.m_slice.m_ids[m_jamSliceHoveredRiffIndex];
                                            requestRiffPlayback( riffCouchID );
                                        }
                                    }
                                }
                            }

                            const float jamGridCellHalf = (float)(cJamBitmapGridCell / 2);
                            const ImVec2 jamGridCell{ (float)cJamBitmapGridCell, (float)cJamBitmapGridCell };
                            const ImVec2 jamGridCellCenter{ jamGridCellHalf, jamGridCellHalf };
                            const ImU32 jamCellColourPlaying = ImGui::GetColorU32( ImGuiCol_Text );
                            const ImU32 jamCellColourEnqueue = ImGui::GetColorU32( ImGuiCol_ChildBg, 0.8f );
                            const ImU32 jamCellColourLoading = ImGui::GetPulseColour();

                            if ( currentRiff )
                            {
                                const auto& activeRectIt = jamSlice.m_riffToBitmapOffset.find( currentRiff->m_riffData.riff.couchID );
                                if ( activeRectIt != jamSlice.m_riffToBitmapOffset.end() )
                                {
                                    const auto& riffRectXY = activeRectIt->second;
                                    draw_list->AddNgonFilled( pos + riffRectXY + jamGridCellCenter, jamGridCellHalf * 0.85f, jamCellColourEnqueue, 3 );
                                    draw_list->AddNgonFilled( pos + riffRectXY + jamGridCellCenter, jamGridCellHalf * 0.7f, jamCellColourPlaying, 3 );
                                }
                            }

                            if ( m_jamSliceRangeClick >= 0 )
                            {
                                const auto& riffCouchID = jamSlice.m_slice.m_ids[m_jamSliceRangeClick];

                                const auto& activeRectIt = jamSlice.m_riffToBitmapOffset.find( riffCouchID );
                                if ( activeRectIt != jamSlice.m_riffToBitmapOffset.end() )
                                {
                                    const auto& riffRectXY = activeRectIt->second;
                                    draw_list->AddCircle( pos + riffRectXY + jamGridCellCenter, 8.0f, jamCellColourLoading, 8, 2.5f );
                                }
                            }

                            for ( const auto& riffEnqueue : m_riffsQueuedForPlayback )
                            {
                                const auto& activeRectIt = jamSlice.m_riffToBitmapOffset.find( riffEnqueue );
                                if ( activeRectIt != jamSlice.m_riffToBitmapOffset.end() )
                                {
                                    const auto& riffRectXY = activeRectIt->second;
                                    draw_list->AddNgonFilled( pos + riffRectXY + jamGridCellCenter, jamGridCellHalf * 0.85f, jamCellColourEnqueue, 3 );
                                }
                            }
                            for ( const auto& riffInFlight : m_syncAndPlaybackInFlight )
                            {
                                const auto& activeRectIt = jamSlice.m_riffToBitmapOffset.find( riffInFlight );
                                if ( activeRectIt != jamSlice.m_riffToBitmapOffset.end() )
                                {
                                    const auto& riffRectXY = activeRectIt->second;
                                    draw_list->AddRectFilled( pos + riffRectXY, pos + riffRectXY + jamGridCell, jamCellColourEnqueue );
                                    draw_list->AddRectFilled( pos + riffRectXY, pos + riffRectXY + jamGridCell, jamCellColourLoading, 4.0f );
                                }
                            }
                        }
                    }
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
                    ImGui::TableSetupColumn( "Load",        ImGuiTableColumnFlags_WidthFixed, 40.0f );
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
                        if ( ImGui::Button( ICON_FA_EYE, ImVec2( 30.0f, 22.0f ) ) )
                        {
                            // change which jam we're viewing, reset active riff hover in the process as this will invalidate it
                            m_currentViewedJam          = m_warehouseContentsReport.m_jamCouchIDs[jI];
                            m_currentViewedJamName      = m_warehouseContentsReportJamTitles[jI];
                            m_jamSliceHoveredRiffIndex  = -1;

                            warehouse.addJamSliceRequest( m_currentViewedJam, std::bind( &LoreApp::handleJamSlice, this, std::placeholders::_1, std::placeholders::_2 ) );
                        }
                        ImGui::EndDisabledControls( isJamInFlux );

                        ImGui::TableNextColumn(); 
                        
                        ImGui::TextDisabled( "ID" );
                        if ( ImGui::IsItemHovered() )
                        {
                            ImGui::BeginTooltip();
                            ImGui::PushTextWrapPos( ImGui::GetFontSize() * 35.0f );
                            ImGui::TextUnformatted( m_warehouseContentsReport.m_jamCouchIDs[jI].c_str() );
                            ImGui::PopTextWrapPos();
                            ImGui::EndTooltip();
                        }
                        ImGui::SameLine();

                        ImGui::TextUnformatted( m_warehouseContentsReportJamTitles[jI].c_str() );

                        ImGui::TableNextColumn();
                        ImGui::BeginDisabledControls( isJamInFlux );
                        if ( ImGui::Button( ICON_FA_SYNC, ImVec2( 28.0f, 22.0f ) ) )
                        {
                            warehouse.addJamSnapshot( m_warehouseContentsReport.m_jamCouchIDs[jI] );
                        }
                        ImGui::EndDisabledControls( isJamInFlux );

                        ImGui::TableNextColumn(); 
                        {
                            const auto unpopulated = m_warehouseContentsReport.m_unpopulatedRiffs[jI];
                            const auto populated   = m_warehouseContentsReport.m_populatedRiffs[jI];

                            if ( unpopulated > 0 )
                                ImGui::Text( "%" PRIi64 " (+%" PRIi64 ")", populated, unpopulated );
                            else
                                ImGui::Text( "%" PRIi64, populated);
                        }
                        ImGui::TableNextColumn();
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
                        if ( ImGui::Button( ICON_FA_TRASH, ImVec2( 28.0f, 22.0f ) ) )
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
            ImGui::OpenPopup( modalJamBrowserTitle );

        MainLoopEnd( nullptr, nullptr );
    }

    m_syncAndPlaybackThread.reset();

    m_discordBotUI.reset();

    // remove the mixer and ensure the async op has taken before leaving scope
    m_mdAudio->blockUntil( m_mdAudio->installMixer( nullptr ) );

    // serialize effects
    m_effectStack->save( m_appConfigPath );
    m_effectStack.reset();


    return 0;
}

// ---------------------------------------------------------------------------------------------------------------------
int main( int argc, char** argv )
{
    LoreApp beam;
    return beam.Run();
}