//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  base class for general app framework
//

#pragma once

#include "base/perf.h"

#include "config/data.h"
#include "config/frontend.h"

#include "endlesss/config.h"
#include "endlesss/api.h"
#include "endlesss/cache.jams.h"
#include "endlesss/cache.stems.h"
#include "endlesss/toolkit.exchange.h"


namespace app {

struct StoragePaths
{
    StoragePaths() = delete;
    StoragePaths( const config::Data& configData, const char* appName );

    fs::path     cacheCommon;    // config::data::storageRoot / cache / common name
    fs::path     cacheApp;       // config::data::storageRoot / cache / app name

    fs::path     outputApp;      // config::data::storageRoot / output / app name
};


namespace module { struct Audio; }
using AudioModule = std::unique_ptr<module::Audio>;

// some access to Core app instances without having to hand over the keys
struct ICoreServices
{
    virtual app::AudioModule& getAudioModule() = 0;
    virtual const endlesss::Exchange& getEndlesssExchange() = 0;
};

// ---------------------------------------------------------------------------------------------------------------------
// abstract application base class that implements basic services but no UI; for that, see CoreGUI
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

    bool tryCreateAndValidateStoragePaths( const StoragePaths& storagePaths ) const;


    fs::path                                m_sharedConfigPath;             // R/W path for config shared between apps
    fs::path                                m_sharedDataPath;               // RO  path for app shared data in the install folder
    fs::path                                m_appConfigPath;                // R/W path for config for this specific app

    config::DataOptional                    m_configData = std::nullopt;
    config::endlesss::API                   m_configEndlessAPI;             // loaded from app install, will be used with
                                                                            // an auth block to initialise NetConfiguration

    // mash of Endlesss access configuration and authentication credentials, required for all our API calls
    // <optional> as this may require a login process during boot sequence
    std::optional < endlesss::api::NetConfiguration >
                                            m_apiNetworkConfiguration = std::nullopt;

    // multithreading bro ever heard of it
    tf::Executor                            m_taskExecutor;

    // the cached jam metadata - public names et al
    endlesss::cache::Jams                   m_jamLibrary;

    // the global live-instance stem cache, used to make riffs live
    endlesss::cache::Stems                  m_stemCache;

    // standard state exchange data, filled when possible with the current playback state
    endlesss::Exchange                      m_endlesssExchange;

    // the interface to portaudio, make noise go bang
    app::AudioModule                        m_mdAudio;

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
struct CoreGUI : public Core
{
    struct PerfData
    {
        perf::TimingPoint::HighResTimePoint   m_startTime;

        double          m_uiLastMicrosecondsPreRender;
        double          m_uiLastMicrosecondsPostRender;
    };

    using RenderHookCallback = std::function<void()>;


    // ImGui panel displaying gathered performance metrics in a table
    void ImGuiPerformanceTracker();

protected:

    // app can declare its own frontend configuration blob
    virtual config::Frontend createDefaultFrontendConfig() const;


    // from Core
    // implementation of Core entrypoint to adorn with GUI components, then calling the expanded one below
    virtual int Entrypoint() override;

    // once services all started, this will be called to begin app-specific main loop; return exit value 
    virtual int EntrypointGUI() = 0;


    config::Frontend        m_configFrontend;

    PerfData                m_perfData;
    app::FrontendModule     m_mdFrontEnd;       // UI canvas management

    // call inside app main loop to perform pre/post core functions (eg. checking for exit, submitting rendering)
    bool MainLoopBegin( bool withDockSpace = true, bool withDefaultMenu = true );
    virtual void MainMenuCustom() {}
    void MainLoopEnd( 
        const RenderHookCallback& preImguiRenderCallback,
        const RenderHookCallback& postImguiRenderCallback );
    
};

} // namespace app

