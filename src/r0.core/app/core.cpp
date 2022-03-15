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


// ---------------------------------------------------------------------------------------------------------------------
// pch global function implementations

void ouroveonThreadEntry( const char* name )
{
    rpmalloc_thread_initialize();
    base::instr::setThreadName( name );
}

void ouroveonThreadExit()
{
    rpmalloc_thread_finalize( 1 );
}

OuroveonThreadScope::OuroveonThreadScope( const char* threadName )
{
    ouroveonThreadEntry( threadName );
    m_name = static_cast<char*>( rpmalloc(strlen(threadName) + 1) );
    strcpy( m_name, threadName );

    blog::instr( FMT_STRING("{{thread}} => {}"), m_name );
}

OuroveonThreadScope::~OuroveonThreadScope()
{
    blog::instr( FMT_STRING( "{{thread}} <= {}" ), m_name );
    rpfree( m_name );
    ouroveonThreadExit();
}


// ---------------------------------------------------------------------------------------------------------------------
namespace tf
{
    // injected from taskflow executor, name the worker threads 
    void _taskflow_worker_thread_init( size_t threadID )
    {
        ouroveonThreadEntry( fmt::format( OURO_THREAD_PREFIX "TaskFlow:{}", threadID ).c_str() );
    }
    void _taskflow_worker_thread_exit( size_t threadID )
    {
        ouroveonThreadExit();
    }
}

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
CoreStart::CoreStart()
{
    rpmalloc_initialize();
    ouroveonThreadEntry( OURO_THREAD_PREFIX "$::main-thread" );
}

CoreStart::~CoreStart()
{
    ouroveonThreadExit();
    rpmalloc_finalize();
}

// ---------------------------------------------------------------------------------------------------------------------
Core::Core()
    : CoreStart()
    , m_taskExecutor( std::clamp( std::thread::hardware_concurrency(), 2U, 8U ) )
{
}

Core::~Core()
{
    
}

#if OURO_PLATFORM_OSX
std::string osxGetBundlePath()
{
    CFURLRef url;
    CFStringRef CFExePath;
    CFBundleRef Bundle;

    Bundle = CFBundleGetMainBundle();
    url = CFBundleCopyExecutableURL(Bundle);

    CFExePath = CFURLCopyFileSystemPath( url, kCFURLPOSIXPathStyle );

    char exeBuffer[1024];
    CFStringGetCString( CFExePath, exeBuffer, 1024, kCFStringEncodingISOLatin1 );

    CFRelease( url );
    CFRelease( CFExePath );

    return std::string( exeBuffer );
}
#endif // OURO_PLATFORM_OSX

// ---------------------------------------------------------------------------------------------------------------------
int Core::Run()
{
#if OURO_PLATFORM_WIN
    // ansi colouring via fmt{} seems to fail on consoles launched from outside VS without jamming these on
    ::SetConsoleMode( ::GetStdHandle( STD_OUTPUT_HANDLE ), ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING );
#endif

    // http://www.cplusplus.com/reference/ios/ios_base/sync_with_stdio/
    std::ios_base::sync_with_stdio( false );

    // sup
    blog::core( "Hello from OUROVEON {} [{}]", GetAppNameWithVersion(), getOuroveonPlatform() );

    // big and wide
    blog::core( "launched taskflow {} with {} worker threads", tf::version(), m_taskExecutor.num_workers() );

    // we load configuration data from the known system config directory
    m_sharedConfigPath  = fs::path( sago::getConfigHome() ) / cOuroveonRootName;
    m_appConfigPath     = m_sharedConfigPath / GetAppCacheName();

#if OURO_PLATFORM_OSX
    // on MacOS we may be running as a THING.app bundle with our own copy of the 
    // shared resources files in a local /Contents/Resources tree - check for this first
    const auto osxBundlePath = osxGetBundlePath();
    auto sharedResRoot = ( fs::path( osxBundlePath ).parent_path().parent_path() ) / "Resources";

    // if running outside of a distribution bundle, assume the unpacked file structure
    if ( !fs::exists(sharedResRoot) )
    {
        blog::core( "osx launched unpacked" );
        sharedResRoot = fs::current_path().parent_path().parent_path();
    }
    else
    {
        blog::core( "osx bundle path : {}", osxBundlePath );
    }
#else
    const auto sharedResRoot = fs::current_path().parent_path().parent_path();
#endif 

    m_sharedDataPath    = sharedResRoot / "shared";

    blog::core( "core filesystem :" );
    blog::core( "  shared config : {}", m_sharedConfigPath.string() );
    blog::core( "     app config : {}", m_appConfigPath.string() );
    blog::core( "    shared data : {}", m_sharedDataPath.string() );

    // point at TZ database
    date::set_install( ( m_sharedDataPath / "timezone" ).string() );

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

#if OURO_FEATURE_VST24
    vst::ScopedInitialiseVSTHosting scopedVST;
#endif // OURO_FEATURE_VST24
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
#if OURO_PLATFORM_WIN
    blog::core( "[press any key]\n" );
    _getch();
#endif     
}

// ---------------------------------------------------------------------------------------------------------------------
void Core::emitAndClearExchangeData()
{
#if OURO_EXCHANGE_IPC
    if ( m_endlesssExchangeIPC.canWrite() )
        m_endlesssExchangeIPC.writeType( m_endlesssExchange );

    m_endlesssExchange.m_dataWriteCounter = m_endlesssExchangeWriteCounter++;
#endif // OURO_EXCHANGE_IPC

    m_endlesssExchange.clear();
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

    registerMainMenuEntry( -1, "LAYOUT", [this]()
    {
        if ( ImGui::MenuItem( "Reset" ) )
        {
            m_resetLayoutInNextUpdate = true;
        }
    });

#if OURO_DEBUG
    registerMainMenuEntry( 1000, "DEBUG", [this]()
    {
        ImGui::MenuItem( "ImGUI Demo", "", &m_showImGuiDebugWindow );
    });
#endif

    // run the app main loop
    int appResult = EntrypointGUI();

    // unwind started services
    m_mdFrontEnd->destroy();

    return appResult;
}

// ---------------------------------------------------------------------------------------------------------------------
void CoreGUI::activateModalPopup( const char* label, const ModalPopupExecutor& executor )
{
    blog::core( "activating modal [{}]", label );

    m_modalsWaiting.emplace_back( label );
    m_modalsActive.emplace_back( label, executor );
}

// ---------------------------------------------------------------------------------------------------------------------
bool CoreGUI::beginInterfaceLayout( const ViewportFlags viewportFlags )
{
    // resetting/loading new layouts has to happen before layout is underway
    if ( m_resetLayoutInNextUpdate )
    {
        blog::core( "resetting layout from default ..." );

        app::module::Frontend::reloadImguiLayoutFromDefault();
        m_resetLayoutInNextUpdate = false;
    }

    // tick the presentation layer, begin a new imgui Frame
    if ( m_mdFrontEnd->appTick() )
        return false;

    m_perfData.m_moment.restart();

    // inject dock space if we're expecting to lay the imgui out with docking
    if ( hasViewportFlag( viewportFlags, VF_WithDocking ) )
        ImGui::DockSpaceOverViewport( ImGui::GetMainViewport() );

    // add main menu bar
    if ( hasViewportFlag( viewportFlags, VF_WithMainMenu ) )
    {
        if ( ImGui::BeginMainMenuBar() )
        {
            // the only uncustomisable, first-entry menu item; everything else uses hooks
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

            for ( const auto& mainMenuEntry : m_mainMenuEntries )
            {
                if ( ImGui::BeginMenu( mainMenuEntry.m_name.c_str() ) )
                {
                    for ( const auto& callbacks : mainMenuEntry.m_callbacks )
                    {
                        callbacks();
                    }
                    ImGui::EndMenu();
                }
            }

            ImGui::EndMainMenuBar();
        }
    }

    // status bar at base of viewport
    if ( hasViewportFlag( viewportFlags, VF_WithStatusBar ) )
    {
        if ( ImGui::BeginStatusBar() )
        {
            // not white, choose a bright but blended theme colour for text on the status bar
            ImGui::PushStyleColor( ImGuiCol_Text, GImGui->Style.Colors[ImGuiCol_ResizeGripActive] );

            ImGui::TextUnformatted( GetAppName() );
            ImGui::Separator();

            // left-aligned blocks; adds in order, dummy-bulks out space to the specified minimum size as we go.
            // no controls on blocks that end up larger than the specified size, no clipping done

            for ( const auto& statusBlock : m_statusBarBlocksLeft )
            {
                const float regionBefore = ImGui::GetContentRegionAvail().x;
                statusBlock.m_callback();
                const float regionAfter = ImGui::GetContentRegionAvail().x;

                const float padSpace = statusBlock.m_size - (regionBefore - regionAfter);
                if ( padSpace > 0 )
                {
                    ImGui::Dummy( ImVec2( padSpace, 0.0f ) );
                }
                ImGui::Separator();
            }

            // right alignment; we save the left-hand cursor and then re-apply dummy elements on each
            //      iteration to add the expected spacing - note there is no clipping yet so right-aligned things may overflow

            constexpr float audioLoadBlockSize = 90.0f;
            const float contentRemainingWidth = ImGui::GetContentRegionAvail().x;

            float cursorSave = ImGui::GetCursorPosX();
            float rightLoad  = audioLoadBlockSize;

            // add the audio load indicator first, far right
            {
                ImGui::Dummy( ImVec2( contentRemainingWidth - audioLoadBlockSize, 0.0f ) );
                ImGui::Separator();

                const double audioEngineLoad = m_mdAudio->getAudioEngineCPULoadPercent();
                m_audoLoadAverage.update( audioEngineLoad );

                ImGui::Text( "LOAD %03.0f%%", m_audoLoadAverage.m_average );
            }

            for ( const auto& statusBlock : m_statusBarBlocksRight )
            {
                ImGui::SetCursorPosX( cursorSave );

                ImGui::Dummy( ImVec2( contentRemainingWidth - ( statusBlock.m_size + rightLoad ), 0.0f ) );
                ImGui::Separator();

                statusBlock.m_callback();

                rightLoad += statusBlock.m_size;
            }

            ImGui::PopStyleColor();
            ImGui::EndStatusBar();
        }
    }

#if OURO_DEBUG
    if ( m_showImGuiDebugWindow )
        ImGui::ShowDemoWindow( &m_showImGuiDebugWindow );
#endif // OURO_DEBUG

    ImGui::PushFont( m_mdFrontEnd->getFont( app::module::Frontend::FontChoice::FixedMain ) );


    // run active modal dialog callbacks
    for ( const auto& modalPair : m_modalsActive )
    {
        std::get<1>( modalPair )( std::get<0>( modalPair ).c_str() );
    }
    if ( !m_modalsActive.empty() )
    {
        // purge any modals that ImGui no longer claims as Open
        auto new_end = std::remove_if( m_modalsActive.begin(),
                                       m_modalsActive.end(),
                                       []( const std::tuple< std::string, ModalPopupExecutor >& ma )
                                       {
                                           return !ImGui::IsPopupOpen( std::get<0>( ma ).c_str() );
                                       });

        for ( auto it = new_end; it != m_modalsActive.end(); ++it )
        {
            blog::core( "modal discard : [{}]", std::get<0>( *it ) );
        }
        m_modalsActive.erase( new_end, m_modalsActive.end() );
    }

    // run file dialog
    if ( m_activeFileDialog != nullptr )
    {
        ImGuiIO& io = ImGui::GetIO();
        const auto maxSize = ImVec2( io.DisplaySize.x, io.DisplaySize.y );
        const auto minSize = ImVec2( maxSize.x * 0.75f, maxSize.y * 0.5f );

        const auto dialogKey = m_activeFileDialog->GetOpenedKey();
        if ( m_activeFileDialog->Display( dialogKey.c_str(), ImGuiWindowFlags_NoCollapse, minSize, maxSize ) )
        {
            if ( m_activeFileDialog->IsOk() )
            {
                m_fileDialogCallbackOnOK( *m_activeFileDialog );
            }

            m_activeFileDialog->Close();

            m_activeFileDialog       = nullptr;
            m_fileDialogCallbackOnOK = nullptr;
        }
    }


    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
void CoreGUI::submitInterfaceLayout()
{
    // trigger popups that are waiting after we're clear of the imgui UI stack
    for ( const auto& modalToPop : m_modalsWaiting )
    {
        blog::core( "modal open : [{}]", modalToPop );
        ImGui::OpenPopup( modalToPop.c_str() );
    }
    m_modalsWaiting.clear();


    ImGui::PopFont();

    // keep track of perf for the main loop
    {
        auto uiElapsed = m_perfData.m_moment.deltaMs();
        m_perfData.m_uiLastMicrosecondsPreRender = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(uiElapsed).count()) * 0.001;
    }

    m_mdFrontEnd->appRenderBegin();

//     if ( preImguiRenderCallback )
//         preImguiRenderCallback();

    m_mdFrontEnd->appRenderImguiDispatch();

    // perf post imgui render dispatch
    {
        auto uiElapsed = m_perfData.m_moment.deltaMs();
        m_perfData.m_uiLastMicrosecondsPostRender = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(uiElapsed).count()) * 0.001;
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

    static constexpr auto executionStages = app::module::Audio::ExposedState::cNumExecutionStages;
    using PerfPoints = std::array< uint64_t, executionStages >;

    static int32_t maxCountdown = 256;       // ignore early [max] readings to disregard boot-up spikes
    static PerfPoints maxPerf = { 0, 0, 0 };
    PerfPoints totalPerf;
    totalPerf.fill( 0 );

    for ( uint32_t pI = 0; pI < app::module::Audio::ExposedState::cPerfTrackSlots; pI++ )
    {
        for ( auto cI = 1; cI < executionStages; cI++ )
        {
            const uint64_t perfValue = aeState.m_perfCounters[cI][pI];

            // only update max scores once initial grace period has passed
            if ( maxCountdown == 0 )
            {
                maxPerf[cI] = std::max( maxPerf[cI], perfValue );
            }

            totalPerf[cI] += perfValue;
        }
    }
    maxCountdown = std::max( 0, maxCountdown - 1 );

    uint64_t totalSum = 0;
    for ( auto cI = 0; cI < executionStages; cI++ )
    {
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
        ImGui::TableNextColumn(); ImGui::Text( "%9" PRIu64 " us", totalPerf[1] );
        ImGui::TableNextColumn(); ImGui::TextUnformatted( "VST" );
        ImGui::TableNextColumn(); ImGui::Text( "%9" PRIu64 " us", totalPerf[2] );
        ImGui::TableNextColumn(); ImGui::TextUnformatted( "Interleave" );
        ImGui::TableNextColumn(); ImGui::Text( "%9" PRIu64 " us", totalPerf[3] );
        ImGui::TableNextColumn(); ImGui::TextUnformatted( "Recorder" );
        ImGui::TableNextColumn(); ImGui::Text( "%9" PRIu64 " us", totalPerf[4] );
        ImGui::TableNextColumn(); ImGui::TextUnformatted( "Total" );
        ImGui::TableNextColumn(); ImGui::Text( "%9" PRIu64 " us", totalSum );
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
        ImGui::TableNextColumn(); ImGui::Text( "%9" PRIu64 " us", maxPerf[1] );
        ImGui::TableNextColumn(); ImGui::TextUnformatted( "VST" );
        ImGui::TableNextColumn(); ImGui::Text( "%9" PRIu64 " us", maxPerf[2] );
        ImGui::TableNextColumn(); ImGui::TextUnformatted( "Recorder" );
        ImGui::TableNextColumn(); ImGui::Text( "%9" PRIu64 " us", maxPerf[3] );

        ImGui::TableNextColumn(); 
        ImGui::Spacing();
        if ( ImGui::Button( "Reset" ) )
        {
            maxPerf.fill( 0 );
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

app::CoreGUI::UIInjectionHandle CoreGUI::registerStatusBarBlock( const StatusBarAlignment alignment, const float size, const UIInjectionCallback& callback )
{
    const auto newHandle = ++m_injectionHandleCounter;

    if ( alignment == StatusBarAlignment::Left )
        m_statusBarBlocksLeft.emplace_back( newHandle, size, callback );
    else
        m_statusBarBlocksRight.emplace_back( newHandle, size, callback );

    return newHandle;
}

bool CoreGUI::unregisterStatusBarBlock( const UIInjectionHandle handle )
{
    bool foundSomething = false;
    {
        auto new_end = std::remove_if( m_statusBarBlocksLeft.begin(),
                                       m_statusBarBlocksLeft.end(),
                                       [handle]( const StatusBarBlock& sbb )
                                       {
                                           return sbb.m_handle == handle;
                                       });
        foundSomething |= (new_end != m_statusBarBlocksLeft.end());
        m_statusBarBlocksLeft.erase( new_end, m_statusBarBlocksLeft.end() );
    }
    {
        auto new_end = std::remove_if( m_statusBarBlocksRight.begin(),
                                       m_statusBarBlocksRight.end(),
                                       [handle]( const StatusBarBlock& sbb )
                                       {
                                           return sbb.m_handle == handle;
                                       });
        foundSomething |= (new_end != m_statusBarBlocksRight.end());
        m_statusBarBlocksRight.erase( new_end, m_statusBarBlocksRight.end() );
    }

    return foundSomething;
}

app::CoreGUI::UIInjectionHandle CoreGUI::registerMainMenuEntry( const int32_t ordering, const std::string& menuName, const UIInjectionCallback& callback )
{
    const auto newHandle = ++m_injectionHandleCounter;

    MenuMenuEntry* menuEntry = nullptr;

    // find existing top level menu? 
    for ( auto& existingMenu : m_mainMenuEntries )
    {
        if ( existingMenu.m_name == menuName )
        {
            menuEntry = &existingMenu;
            break;
        }
    }
    // .. or not, so create new entry for this name
    if ( menuEntry == nullptr )
    {
        auto& newEntry = m_mainMenuEntries.emplace_back();
        newEntry.m_name = menuName;
        menuEntry = &newEntry;
    }

    menuEntry->m_ordering = ordering;
    menuEntry->m_handles.emplace_back( newHandle );
    menuEntry->m_callbacks.emplace_back( callback );

    // resort menu stack each time we add anything
    std::sort( m_mainMenuEntries.begin(), m_mainMenuEntries.end(), []( const MenuMenuEntry& lhs, const MenuMenuEntry& rhs )
        {
            return lhs.m_ordering < rhs.m_ordering;
        });

    return newHandle;
}

bool CoreGUI::unregisterMainMenuEntry( const UIInjectionHandle handle )
{
    return false;
}

} // namespace app
