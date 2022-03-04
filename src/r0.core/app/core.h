//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  base class for general app framework
//

#pragma once

#include "spacetime/moment.h"

#include "config/data.h"
#include "config/frontend.h"

#include "endlesss/config.h"
#include "endlesss/api.h"
#include "endlesss/cache.jams.h"
#include "endlesss/cache.stems.h"
#include "endlesss/toolkit.exchange.h"

// ---------------------------------------------------------------------------------------------------------------------
// on Windows, declare and enable use of the global memory IPC object for pushing Exchange data out to external apps
#if OURO_PLATFORM_WIN

#include "win32/ipc.h"
#define OURO_EXCHANGE_IPC   1

namespace endlesss {
using ExchangeIPC = win32::GlobalSharedMemory< endlesss::Exchange >;
} //namespace endlesss

#else // OURO_PLATFORM_WIN

#define OURO_EXCHANGE_IPC   0

#endif


// ---------------------------------------------------------------------------------------------------------------------
namespace app {

// #HDD todo, move this somewhere, add accessors etc
struct StoragePaths
{
    StoragePaths() = delete;
    StoragePaths( const config::Data& configData, const char* appName );

    fs::path     cacheCommon;    // config::data::storageRoot / cache / common name
    fs::path     cacheApp;       // config::data::storageRoot / cache / app name

    fs::path     outputApp;      // config::data::storageRoot / output / app name

    // utility to try and ensure the populated paths exist, (try to) create them if they don't;
    // returns false if this process fails, logs out all the attempts
    bool tryToCreateAndValidate() const;
};


namespace module { struct Audio; struct Midi; }
using AudioModule = std::unique_ptr<module::Audio>;
using MidiModule  = std::unique_ptr<module::Midi>;


// some access to Core app instances without having to hand over the keys
struct ICoreServices
{
    virtual app::AudioModule& getAudioModule() = 0;
    virtual const endlesss::Exchange& getEndlesssExchange() = 0;
};

// ---------------------------------------------------------------------------------------------------------------------
// application base class that implements basic services for headless use
//
struct Core : public config::IPathProvider,
              public ICoreServices
{
    Core();
    virtual ~Core();

    // high-level entrypoint, called by main()
    int Run();

    static void waitForConsoleKey();

    const fs::path& getSharedConfigPath() const { return m_sharedConfigPath; }
    const fs::path& getSharedDataPath() const   { return m_sharedDataPath;   }
    const fs::path& getAppConfigPath() const    { return m_appConfigPath;    }

protected:

    // called once basic initial configuration is done for the application to continue work
    virtual int Entrypoint() = 0;

    // used for visual identity as well as cache path differentiation
    virtual const char* GetAppName() const = 0;             // 'FOO'
    virtual const char* GetAppNameWithVersion() const = 0;  // 'FOO 0.1.2-beta'
    virtual const char* GetAppCacheName() const = 0;        // must be filename/path friendly

    inline const char* getOuroveonPlatform() const {
#if OURO_PLATFORM_WIN
        return "Win64";
#elif OURO_PLATFORM_OSX
        return "OSX";
#elif OURO_PLATFORM_NIX
        return "Linux";
#else
        return "Unknown";
#endif
    }


    // call to transmit or broadcast the currently populated endlesss::Exchange block if such methods are enabled
    // followed by clearing it ready for re-populating
    void emitAndClearExchangeData();


    fs::path                                m_sharedConfigPath;             // R/W path for config shared between apps
    fs::path                                m_sharedDataPath;               // RO  path for app shared data in the install folder
    fs::path                                m_appConfigPath;                // R/W path for config for this specific app

    config::DataOptional                    m_configData = std::nullopt;
    config::endlesss::API                   m_configEndlesssAPI;            // loaded from app install, will be used with
                                                                            // an auth block to initialise NetConfiguration


    // mash of Endlesss access configuration and authentication credentials, required for all our API calls
    // <optional> as this may require a login process during boot sequence
    std::optional < endlesss::api::NetConfiguration >
                                            m_apiNetworkConfiguration = std::nullopt;

    // multithreading bro ever heard of it
    tf::Executor                            m_taskExecutor;

    // the cached jam metadata - public names et al
    endlesss::cache::Jams                   m_jamLibrary;

    // standard state exchange data, filled when possible with the current playback state
    endlesss::Exchange                      m_endlesssExchange;

#if OURO_EXCHANGE_IPC
    // constantly-updated globally-shared data block
    endlesss::ExchangeIPC                   m_endlesssExchangeIPC;
    uint32_t                                m_endlesssExchangeWriteCounter = 1;
#endif

    // the interface to portaudio, make noise go bang
    app::AudioModule                        m_mdAudio;

    // interface to MIDI capture/production
    app::MidiModule                         m_mdMidi;

public:

    // config::IPathProvider
    fs::path getPath( const PathFor p ) const override
    { 
        switch ( p )
        {
            case config::IPathProvider::PathFor::SharedConfig:  return m_sharedConfigPath;
            case config::IPathProvider::PathFor::SharedData:    return m_sharedDataPath;
            case config::IPathProvider::PathFor::PerAppConfig:  return m_appConfigPath;
        }
        assert( 0 );
        return { "" };
    }

    // ICoreServices
    virtual app::AudioModule& getAudioModule() override { return m_mdAudio; }
    virtual const endlesss::Exchange& getEndlesssExchange() override { return m_endlesssExchange; }
};


// ---------------------------------------------------------------------------------------------------------------------
namespace module { struct Frontend; }
using FrontendModule = std::unique_ptr<module::Frontend>;

// ---------------------------------------------------------------------------------------------------------------------
// next layer up from Core is a Core app with a UI; bringing GL and ImGUI into the mix
//
struct CoreGUI : public Core
{
    // UI injection for inserting custom things into generic structure - eg. main menu items, status bar blocks
    using UIInjectionHandle     = uint32_t;
    using UIInjectionCallback   = std::function< void() >;

    using ModalPopupExecutor    = std::function< void( const char* ) >;
    
    using FileDialogInst        = std::unique_ptr<ImGuiFileDialog>;
    using FileDialogCallback    = std::function< void( ImGuiFileDialog& ) >;


    enum ViewportFlags
    {
        VF_None             = 0,
        VF_WithDocking      = 1 << 1,
        VF_WithMainMenu     = 1 << 2,
        VF_WithStatusBar    = 1 << 3,
    };

    using MainLoopCallback = std::function<void()>;

    // ImGui panel displaying gathered performance metrics in a table
    void ImGuiPerformanceTracker();


    enum class StatusBarAlignment
    {
        Left,
        Right
    };

    UIInjectionHandle registerStatusBarBlock( const StatusBarAlignment alignment, const float size, const UIInjectionCallback& callback );
    bool unregisterStatusBarBlock( const UIInjectionHandle handle );

    UIInjectionHandle registerMainMenuEntry( const int32_t ordering, const std::string& menuName, const UIInjectionCallback& callback );
    bool unregisterMainMenuEntry( const UIInjectionHandle handle );


    // push a popup label to execute in the edges of the main loop, with [executor] being called to actually display
    // whatever popup window you have in mind.
    void activateModalPopup( const char* label, const ModalPopupExecutor& executor );

    // submit an ImGui file dialog instance for display as part of the main loop; we can only have one live at once
    bool activateFileDialog( FileDialogInst&& dialogInstance, const FileDialogCallback& onOK )
    {
        if ( m_activeFileDialog == nullptr )
        {
            m_activeFileDialog       = std::move(dialogInstance);
            m_fileDialogCallbackOnOK = onOK;
            return true;
        }
        return false;
    }


protected:

    struct StatusBarBlock
    {
        StatusBarBlock( const UIInjectionHandle& handle,
                        const float size,
                        const UIInjectionCallback& callback )
            : m_handle( handle )
            , m_size( size )
            , m_callback( callback )
        {}

        UIInjectionHandle       m_handle;
        float                   m_size;
        UIInjectionCallback     m_callback;
    };
    using StatusBarBlockList = std::vector< StatusBarBlock >;

    struct MenuMenuEntry
    {
        int32_t                             m_ordering = 0;
        std::string                         m_name;
        std::vector< UIInjectionHandle >    m_handles;
        std::vector< UIInjectionCallback >  m_callbacks;
    };
    using MenuMenuEntryList     = std::vector< MenuMenuEntry >;


    // generic modal-popup tracking types to simplify client code opening and running dialog boxes
    using ModalPopupsWaiting    = std::vector< std::string >;
    using ModalPopupsActive     = std::vector< std::tuple< std::string, ModalPopupExecutor > >;


    // to avoid flickering values on the status bar; distracting and not particularly useful
    using AudioLoadAverage      = base::RollingAverage< 60 >;


    // app can declare its own frontend configuration blob
    virtual config::Frontend createDefaultFrontendConfig() const;


    // from Core
    // implementation of Core entrypoint to adorn with GUI components, then calling the expanded one below
    virtual int Entrypoint() override;

    // once services all started, this will be called to begin app-specific main loop; return exit value 
    virtual int EntrypointGUI() = 0;


    // call inside app main loop to perform pre/post core functions (eg. checking for exit, submitting rendering)
    bool beginInterfaceLayout( const ViewportFlags viewportFlags );
    void submitInterfaceLayout();

    static constexpr bool hasViewportFlag( const ViewportFlags viewportFlags, ViewportFlags vFlag )
    {
        return ( viewportFlags & vFlag ) == vFlag;
    }


    struct PerfData
    {
        spacetime::Moment   m_moment;

        double              m_uiLastMicrosecondsPreRender;
        double              m_uiLastMicrosecondsPostRender;
    };

    config::Frontend        m_configFrontend;
    app::FrontendModule     m_mdFrontEnd;       // UI canvas management

    PerfData                m_perfData;
    AudioLoadAverage        m_audoLoadAverage;


    UIInjectionHandle       m_injectionHandleCounter;

    // registered new core items like menu entries, status bar sections
    MenuMenuEntryList       m_mainMenuEntries;
    StatusBarBlockList      m_statusBarBlocksLeft;
    StatusBarBlockList      m_statusBarBlocksRight;

    // modal dialog registration to unify the handling of arbitrary popups during input loop
    ModalPopupsWaiting      m_modalsWaiting;
    ModalPopupsActive       m_modalsActive;

    FileDialogInst          m_activeFileDialog;
    FileDialogCallback      m_fileDialogCallbackOnOK;

private:

#if OURO_DEBUG
    bool                    m_showImGuiDebugWindow    = false;
#endif // OURO_DEBUG
    bool                    m_resetLayoutInNextUpdate = false;
};

} // namespace app

