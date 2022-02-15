//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"

#include "base/instrumentation.h"

#include "config/base.h"
#include "config/frontend.h"
#include "config/data.h"

#include "app/core.h"
#include "app/module.frontend.h"
#include "app/module.frontend.fonts.h"
#include "app/module.audio.h"
#include "app/module.midi.h"

#include "filesys/fsutil.h"

#include "platform_folders.h"


#if OURO_PLATFORM_WIN
#include "win32/utils.h"
#include <ios>
#include <conio.h>
#endif 

#if OURO_FEATURE_VST24
#include "effect/vst2/host.h"
#endif 


namespace app {

// ---------------------------------------------------------------------------------------------------------------------
StoragePaths::StoragePaths( const config::Data& configData, const char* appName )
{
    cacheCommon = ( fs::path( configData.storageRoot ) / "cache"  ) / "common";
    cacheApp    = ( fs::path( configData.storageRoot ) / "cache"  ) / appName;
    outputApp   = ( fs::path( configData.storageRoot ) / "output" ) / appName;
}

// ---------------------------------------------------------------------------------------------------------------------
bool StoragePaths::tryToCreateAndValidate() const
{
    blog::core( "  storage paths :" );

    blog::core( "      app cache : {}", cacheApp.string() );
    if ( filesys::ensureDirectoryExists( cacheApp ) == false )
    {
        blog::error::core( "unable to create or find storage directory" );
        return false;
    }

    blog::core( "   common cache : {}", cacheCommon.string() );
    if ( filesys::ensureDirectoryExists( cacheCommon ) == false )
    {
        blog::error::core( "unable to create or find storage directory" );
        return false;
    }

    blog::core( "     app output : {}", outputApp.string() );
    if ( filesys::ensureDirectoryExists( outputApp ) == false )
    {
        blog::error::core( "unable to create or find storage directory" );
        return false;
    }

    return true;
}


// ---------------------------------------------------------------------------------------------------------------------
Core::Core()
    : m_taskExecutor( std::clamp( std::thread::hardware_concurrency(), 2U, 8U ) )
{
    base::instr::setThreadName( OURO_THREAD_PREFIX "$::main-thread" );
}

Core::~Core()
{
}

// ---------------------------------------------------------------------------------------------------------------------
int Core::Run()
{
#if OURO_PLATFORM_WIN
    // ansi colouring via fmt{} seems to fail on consoles launched from outside VS without jamming these on
    ::SetConsoleMode( ::GetStdHandle( STD_OUTPUT_HANDLE ), ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING );
#endif

    // http://www.cplusplus.com/reference/ios/ios_base/sync_with_stdio/
    std::ios_base::sync_with_stdio( false );

    // point at TZ database
    date::set_install( "../../shared/timezone" );

    // sup
    blog::core( "Hello from OUROVEON {} [{}]", GetAppNameWithVersion(), getOuroveonPlatform() );

    // big and wide
    blog::core( "launched taskflow {} with {} worker threads", tf::version(), m_taskExecutor.num_workers() );


    // we load configuration data from the known system config directory
    m_sharedConfigPath  = fs::path( sago::getConfigHome() ) / cOuroveonRootName;
    m_appConfigPath     = m_sharedConfigPath / GetAppCacheName();

    m_sharedDataPath    = fs::current_path().parent_path().parent_path() / "shared";

    blog::core( "core filesystem :" );
    blog::core( "  shared config : {}", m_sharedConfigPath.string() );
    blog::core( "     app config : {}", m_appConfigPath.string() );
    blog::core( "    shared data : {}", m_sharedDataPath.string() );

    // ensure all core directories exist or we can't continue
    {
        blog::core( "ensuring core paths are viable ..." );
        if ( filesys::ensureDirectoryExists( m_sharedConfigPath ) == false )
        {
            blog::error::core( "unable to create or find shared config directory, aborting\n[{}]", m_sharedConfigPath.string() );
            return -2;
        }
        if ( filesys::ensureDirectoryExists( m_appConfigPath ) == false )
        {
            blog::error::core( "unable to create or find app config directory, aborting\n[{}]", m_appConfigPath.string() );
            return -2;
        }
        if ( !fs::exists( m_sharedDataPath ) )
        {
            blog::error::core( "cannot find shared data directory, aborting" );
            return -2;
        }
    }

    // try and load the data storage configuration; may be unavailable on a fresh boot
    config::Data configData;
    const auto dataLoad = config::load( *this, configData );
    if ( dataLoad == config::LoadResult::Success )
    {
        // stash the loaded data as a starting point, otherwise code later on will need to set this up manually / via UI
        m_configData = configData;
        blog::core( "   storage root : {}", m_configData.value().storageRoot );
    }
    // nothing found, create some kind of default that can be set manually later
    else if ( dataLoad == config::LoadResult::CannotFindConfigFile )
    {
        blog::core( "no data configuration file [{}] found", config::Data::StorageFilename );
    }
    // couldn't load, will have to setup a new one
    else
    {
        blog::error::core( "unable to parse, find or load data configuration file [{}]", config::Data::StorageFilename );
    }



    blog::core( "initial Endlesss setup ..." );

    // try and load the API config bundle
    const auto apiLoadResult = config::load( *this, m_configEndlesssAPI );
    if ( apiLoadResult != config::LoadResult::Success )
    {
        // can't continue without the API config
        blog::error::cfg( "Unable to load required Endlesss API configuration data [{}]", 
            config::getFullPath< config::endlesss::API >( *this ).string() );

        return -2;
    }


    // resolve cert bundle from the shared data path
    const auto certPath = ( m_sharedDataPath / fs::path( m_configEndlesssAPI.certBundleRelative ) );
    if ( !fs::exists( certPath ) )
    {
        blog::error::cfg( "Cannot find CA root certificates file [{}], required for networking", certPath.string() );
        return -2;
    }
    // rewrite config option with the full path
    m_configEndlesssAPI.certBundleRelative = certPath.string();

    // drop the app name, version and platform into the user agent string
    m_configEndlesssAPI.userAgentApp += fmt::format( "{} ({})", GetAppNameWithVersion(), getOuroveonPlatform() );
    m_configEndlesssAPI.userAgentDb  += fmt::format( " ({})", GetAppNameWithVersion(), getOuroveonPlatform() );


    // load the jam cache data (or try to)
    m_jamLibrary.load( *this );



#if OURO_PLATFORM_WIN
    // bring up global external registrations / services that will auto-unwind on exit
    win32::ScopedInitialiseCOM      scopedCOM;

#if OURO_FEATURES_VST
    vst::ScopedInitialiseVSTHosting scopedVST;
#endif // OURO_FEATURES_VST
#endif // OURO_PLATFORM_WIN

#if OURO_EXCHANGE_IPC
    // create shared buffer for exchanging data with other apps
    if ( !m_endlesssExchangeIPC.init(
        endlesss::Exchange::GlobalMapppingNameW,
        endlesss::Exchange::GlobalMutexNameW,
        win32::details::IPC::Access::Write ) )
    {
        blog::error::core( "Failed to open global memory for data exchange; feature disabled" );
    }
    else
    {
        blog::core( "Broadcasting data exchange on [{}]", endlesss::Exchange::GlobalMapppingNameA );
    }
#endif // OURO_EXCHANGE_IPC


    // create our wrapper around PA; this doesn't connect to a device, just does initial startup & enumeration
    m_mdAudio = std::make_unique<app::module::Audio>();
    if ( !m_mdAudio->create( *this ) )
    {
        blog::error::core( "app::module::AudioServices unable to start" );
        return -2;
    }

    m_mdMidi = std::make_unique<app::module::Midi>();
    if ( !m_mdMidi->create( *this ) )
    {
        blog::error::core( "app::module::Midi unable to start" );
        return -2;
    }

    // run the app main loop
    int appResult = Entrypoint();

    // unwind started services
    m_mdMidi->destroy();
    m_mdAudio->destroy();

    return appResult;
}

// ---------------------------------------------------------------------------------------------------------------------
void Core::waitForConsoleKey()
{
    blog::core( "[press any key]\n" );
    _getch();
}

// ---------------------------------------------------------------------------------------------------------------------
void Core::emitAndClearExchangeData()
{
#if OURO_EXCHANGE_IPC
    if ( m_endlesssExchangeIPC.canWrite() )
        m_endlesssExchangeIPC.writeType( m_endlesssExchange );
#endif // OURO_EXCHANGE_IPC

    m_endlesssExchange.clear();
    m_endlesssExchange.m_dataWriteCounter = m_endlesssExchangeWriteCounter++;
}

// ---------------------------------------------------------------------------------------------------------------------
config::Frontend CoreGUI::createDefaultFrontendConfig() const
{
    return config::Frontend{};
}

// ---------------------------------------------------------------------------------------------------------------------
int CoreGUI::Entrypoint()
{
    const auto feLoad = config::load( *this, m_configFrontend );
    if ( feLoad != config::LoadResult::Success )
    {
        blog::core( "unable to find or load [{}], assuming defaults", config::Frontend::StorageFilename );
        m_configFrontend = createDefaultFrontendConfig();
    }

    // bring up frontend; SDL and IMGUI
    m_mdFrontEnd = std::make_unique<app::module::Frontend>( m_configFrontend, GetAppNameWithVersion() );
    if ( !m_mdFrontEnd->create( *this ) )
    {
        blog::error::core( "app::module::Frontend unable to start" );
        return -2;
    }

    // run the app main loop
    int appResult = EntrypointGUI();

    // unwind started services
    m_mdFrontEnd->destroy();

    return appResult;
}

// ---------------------------------------------------------------------------------------------------------------------
bool CoreGUI::beginInterfaceLayout(
    const ViewportMode viewportMode,
    const MainLoopCallback& mainMenuCallback /*= nullptr*/,
    const MainLoopCallback& statusBarCallback /*= nullptr*/ )
{
    // crap hack that I'll fix later (i wont), but resetting/loading new layouts has to happen before layout is underway
    static bool doResetBeforeTick = false;
    if ( doResetBeforeTick )
    {
        app::module::Frontend::reloadImguiLayoutFromDefault();
        doResetBeforeTick = false;
    }

    // tick the presentation layer, begin a new imgui Frame
    if ( m_mdFrontEnd->appTick() )
        return false;

    m_perfData.m_moment.restart();

    // inject dock space if we're expecting to lay the imgui out with docking
    if ( viewportMode == ViewportMode::DockingViewport )
        ImGui::DockSpaceOverViewport( ImGui::GetMainViewport() );

    static bool demoWindowVisible = false;
    if ( mainMenuCallback != nullptr )
    {
        if ( ImGui::BeginMainMenuBar() )
        {
            if ( ImGui::BeginMenu( "OUROVEON" ) )
            {
                ImGui::MenuItem( GetAppNameWithVersion(), nullptr, nullptr, false );
                ImGui::MenuItem( "ishani.org 2022", nullptr, nullptr, false );

                ImGui::Separator();
                if ( ImGui::MenuItem( "Toggle Border" ) )
                    m_mdFrontEnd->toggleBorderless();

                ImGui::Separator();
                if ( ImGui::MenuItem( "Quit" ) )
                    m_mdFrontEnd->requestQuit();

                ImGui::EndMenu();
            }

            if ( ImGui::BeginMenu( "LAYOUT" ) )
            {
                if ( ImGui::MenuItem( "Reset" ) )
                {
                    doResetBeforeTick = true;
                }
                ImGui::EndMenu();
            }

            mainMenuCallback();

#ifdef _DEBUG
            if ( ImGui::BeginMenu( "DEBUG" ) )
            {
                ImGui::MenuItem( "ImGUI Demo", "", &demoWindowVisible );
                ImGui::EndMenu();
            }
#endif
            ImGui::EndMainMenuBar();
        }
    }

    if ( statusBarCallback != nullptr )
    {
        if ( ImGui::BeginStatusBar() )
        {
            const double audioEngineLoad = m_mdAudio->getAudioEngineCPULoadPercent();
            m_audoLoadAverage.update( audioEngineLoad );

            ImGui::TextUnformatted( GetAppName() );
            ImGui::Separator();

            statusBarCallback();

            // right-align
            ImGui::Dummy( ImVec2( ImGui::GetContentRegionAvail().x - 90.0f, 0.0f ) );
            ImGui::Separator();
            ImGui::Text( "LOAD %03.0f%%", m_audoLoadAverage.m_average );


            ImGui::EndStatusBar();
        }
    }

#ifdef _DEBUG
    if ( demoWindowVisible )
        ImGui::ShowDemoWindow( &demoWindowVisible );
#endif

    ImGui::PushFont( m_mdFrontEnd->getFont( app::module::Frontend::FontChoice::FixedMain ) );

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
void CoreGUI::submitInterfaceLayout()
{
    ImGui::PopFont();

    // keep track of perf for the main loop
    {
        auto uiElapsed = m_perfData.m_moment.deltaMs();
        m_perfData.m_uiLastMicrosecondsPreRender = (double)std::chrono::duration_cast<std::chrono::microseconds>(uiElapsed).count() * 0.001;
    }

    m_mdFrontEnd->appRenderBegin();

//     if ( preImguiRenderCallback )
//         preImguiRenderCallback();

    m_mdFrontEnd->appRenderImguiDispatch();

    // perf post imgui render dispatch
    {
        auto uiElapsed = m_perfData.m_moment.deltaMs();
        m_perfData.m_uiLastMicrosecondsPostRender = (double)std::chrono::duration_cast<std::chrono::microseconds>(uiElapsed).count() * 0.001;
    }

//     if ( postImguiRenderCallback )
//         postImguiRenderCallback();

    m_mdFrontEnd->appRenderFinalise();
}


// ---------------------------------------------------------------------------------------------------------------------
void CoreGUI::ImGuiPerformanceTracker()
{
    ImGui::Begin( "Profiling" );

    const float column0size = 90.0f;
    const auto& aeState = m_mdAudio->getState();

    if ( ImGui::BeginTable( "##aengine", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg ) )
    {
        ImGui::PushStyleColor( ImGuiCol_Text, ImGui::GetStyleColorVec4( ImGuiCol_ResizeGripHovered ) );
        ImGui::TableSetupColumn( "Buffers", ImGuiTableColumnFlags_WidthFixed, column0size );
        ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_None );
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        ImGui::TableNextColumn(); ImGui::TextUnformatted( "Min" );
        ImGui::TableNextColumn(); ImGui::Text( "  %7i", m_mdAudio->getState().m_minBufferFillSize );
        ImGui::TableNextColumn(); ImGui::TextUnformatted( "Max" );
        ImGui::TableNextColumn(); ImGui::Text( "  %7i", m_mdAudio->getState().m_maxBufferFillSize );
        ImGui::TableNextColumn(); ImGui::TextUnformatted( "Rate" );
        ImGui::TableNextColumn(); ImGui::Text( "  %7i", m_mdAudio->getSampleRate() );

        ImGui::EndTable();
    }

    static constexpr auto executionStages = app::module::Audio::ExposedState::cNumExecutionStages - 1;
    using PerfPoints = std::array< double, executionStages >;

    static int32_t maxCountdown = 256;       // ignore early [max] readings to disregard boot-up spikes
    static PerfPoints maxPerf = { 0.0, 0.0, 0.0 };
    PerfPoints totalPerf;
    totalPerf.fill( 0.0 );

    for ( uint32_t pI = 0; pI < app::module::Audio::ExposedState::cPerfTrackSlots; pI++ )
    {
        // ignore perf counters that are mid-update (where the end counter hasn't been updated yet)
        if ( aeState.m_perfCounters[executionStages][pI] < aeState.m_perfCounters[0][pI] )
            continue;

        for ( auto cI = 0; cI < executionStages; cI++ )
        {
            const double perfValue = (double)(aeState.m_perfCounters[cI+1][pI] - aeState.m_perfCounters[cI][pI]) * 1000000.0 * aeState.m_perfCounterFreqRcp;

            // only update max scores once initial grace period has passed
            if ( maxCountdown == 0 )
            {
                maxPerf[cI] = std::max( maxPerf[cI], perfValue );
            }

            totalPerf[cI] += perfValue;
        }
    }
    maxCountdown = std::max( 0, maxCountdown - 1 );

    double totalSum = 0;
    for ( auto cI = 0; cI < executionStages; cI++ )
    {
        totalPerf[cI] *= app::module::Audio::ExposedState::cPerfSumRcp;
        totalSum += totalPerf[cI];
    }

    if ( ImGui::BeginTable( "##perf_stats_avg", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg ) )
    {
        ImGui::PushStyleColor( ImGuiCol_Text, ImGui::GetStyleColorVec4( ImGuiCol_ResizeGripHovered ) );
        ImGui::TableSetupColumn( "AVERAGE", ImGuiTableColumnFlags_WidthFixed, column0size );
        ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_None );
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        ImGui::TableNextColumn(); ImGui::TextUnformatted( "Mixer" );
        ImGui::TableNextColumn(); ImGui::Text( "%9.1f us", totalPerf[0] );
        ImGui::TableNextColumn(); ImGui::TextUnformatted( "VST" );
        ImGui::TableNextColumn(); ImGui::Text( "%9.1f us", totalPerf[1] );
        ImGui::TableNextColumn(); ImGui::TextUnformatted( "Interleave" );
        ImGui::TableNextColumn(); ImGui::Text( "%9.1f us", totalPerf[2] );
        ImGui::TableNextColumn(); ImGui::TextUnformatted( "Recorder" );
        ImGui::TableNextColumn(); ImGui::Text( "%9.1f us", totalPerf[3] );
        ImGui::TableNextColumn(); ImGui::TextUnformatted( "Total" );
        ImGui::TableNextColumn(); ImGui::Text( "%9.1f us", totalSum );
        ImGui::TableNextColumn(); ImGui::TextUnformatted( "Load" );
        ImGui::TableNextColumn(); ImGui::Text( "%9.1f %%", m_mdAudio->getAudioEngineCPULoadPercent() );

        ImGui::EndTable();
    }
    if ( ImGui::BeginTable( "##perf_stats_max", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg ) )
    {
        ImGui::PushStyleColor( ImGuiCol_Text, ImGui::GetStyleColorVec4( ImGuiCol_ResizeGripHovered ) );
        ImGui::TableSetupColumn( "MAXIMUM", ImGuiTableColumnFlags_WidthFixed, column0size );
        ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_None );
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        ImGui::TableNextColumn(); ImGui::TextUnformatted( "Mixer" );
        ImGui::TableNextColumn(); ImGui::Text( "%9.1f us", maxPerf[0] );
        ImGui::TableNextColumn(); ImGui::TextUnformatted( "VST" );
        ImGui::TableNextColumn(); ImGui::Text( "%9.1f us", maxPerf[1] );
        ImGui::TableNextColumn(); ImGui::TextUnformatted( "Recorder" );
        ImGui::TableNextColumn(); ImGui::Text( "%9.1f us", maxPerf[2] );

        ImGui::TableNextColumn(); 
        ImGui::Spacing();
        if ( ImGui::Button( "Reset" ) )
        {
            maxPerf.fill( 0.0 );
        }
        ImGui::Spacing(); ImGui::TableNextColumn();

        ImGui::EndTable();
    }
    if ( ImGui::BeginTable( "##perf_stats_ext", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg ) )
    {
        ImGui::PushStyleColor( ImGuiCol_Text, ImGui::GetStyleColorVec4( ImGuiCol_ResizeGripHovered ) );
        ImGui::TableSetupColumn( "UI", ImGuiTableColumnFlags_WidthFixed, column0size );
        ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_None );
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        ImGui::TableNextColumn(); ImGui::TextUnformatted( "Build" );
        ImGui::TableNextColumn(); ImGui::Text( "%9.1f ms", m_perfData.m_uiLastMicrosecondsPreRender );
        ImGui::TableNextColumn(); ImGui::TextUnformatted( "Render" );
        ImGui::TableNextColumn(); ImGui::Text( "%9.1f ms", m_perfData.m_uiLastMicrosecondsPostRender );

        ImGui::EndTable();
    }

    ImGui::End();
}

} // namespace app
