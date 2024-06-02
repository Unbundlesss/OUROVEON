//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#pragma once

#include "app/core.h"

#include "config/nonet.h"
#include "discord/config.h"
#include "endlesss/core.services.h"

#if !OURO_HAS_NDLS_ONLINE
#include "ux/user.selector.h"
#endif // OURO_HAS_NDLS_ONLINE

namespace rec { struct IRecordable; }
namespace app {

// ---------------------------------------------------------------------------------------------------------------------
// OuroApp is a small shim for UI apps to build upon that adds some basic Endlesss features out the gate; like a standard
// "login" screen that offers audio/services configuration and validating a connection to the Endlesss backend
//
struct OuroApp : public CoreGUI,
                 public endlesss::services::IRiffFetchService,          // natively support using the built-in member services to load riffs
                 public endlesss::services::IJamNameResolveService      // assume base level support for abstract jam name resolution
{
    OuroApp()
        : CoreGUI()
    {}

    endlesss::toolkit::Warehouse* getWarehouseInstance() { return m_warehouse.get(); }
    const endlesss::toolkit::Warehouse* getWarehouseInstance() const { return m_warehouse.get(); }

protected:

    // from CoreGUI
    // this inserts the generic ouro app configuration preflight UI; when that closes, we pass to EntrypointOuro
    virtual int EntrypointGUI() override;

    // inheritants implement this as app entrypoint
    virtual int EntrypointOuro() = 0;

protected:

    // endlesss::services::IRiffFetchService
    int32_t                                 getSampleRate() const override;
    const endlesss::api::NetConfiguration&  getNetConfiguration() const override { return *m_networkConfiguration; }
    endlesss::cache::Stems&                 getStemCache() override { return m_stemCache; }
    tf::Executor&                           getTaskExecutor() override { return m_taskExecutor; }

    // endlesss::services::IJamNameCacheServices
    LookupResult lookupJamNameAndTime(
        const endlesss::types::JamCouchID& jamID,
        std::string& resultJamName,
        uint64_t& resultTimestamp ) const override;



    // validated storage locations for the app
    // <optional> because this is expected to be configured once the boot sequence has checked / changed
    //              the root storage path; once the app starts for real, this can be assumed to be valid
    std::optional< StoragePaths >           m_storagePaths = std::nullopt;


    // -----------------------------------------------------------------------------------------------------------------

    // stem cache maintenance; checks for memory pressure, triggers async prune operation to try and guide towards
    // chosen memory limit
    void maintainStemCacheAsync();
    void ensureStemCacheChecksComplete();

    // the global live-instance stem cache, used to populate riffs when preparing for playback
    endlesss::cache::Stems                  m_stemCache;
    
    // timer used to check in and auto-prune the stem cache if it busts past the set memory usage targets
    static constexpr auto                   c_stemCachePruneCheckDuration = std::chrono::seconds( 30 );
    spacetime::Moment                       m_stemCacheLastPruneCheck;

    // async bits to run the prune() fn via TF
    tf::Taskflow                            m_stemCachePruneTask;
    std::optional< tf::Future<void> >       m_stemCachePruneFuture = std::nullopt;


    // -----------------------------------------------------------------------------------------------------------------

    // instance of the endlesss data warehouse, holding all locally synced jam/riff/stem data
    endlesss::toolkit::Warehouse::Instance  m_warehouse;

    // jam ID -> public name data that has been cached in the Warehouse database, used as secondary lookup
    // for IDs that we don't recognise as the normal jam library might be missing jams the user has left etc
    endlesss::types::JamIDToNameMap         m_jamHistoricalFromWarehouse;

    // -----------------------------------------------------------------------------------------------------------------

#if OURO_HAS_NDLS_ONLINE
    endlesss::api::MyClubs::ChannelsList    m_clubsChannels;
    std::atomic_bool                        m_clubsIntegrationEnabled = false;  // if true, we have valid Clubs data fetched
#endif // OURO_HAS_NDLS_ONLINE

#if !OURO_HAS_NDLS_ONLINE
    config::NoNet                           m_configNoNet;
    ImGui::ux::UserSelector                 m_noNetImpersonationUser;
#endif // OURO_HAS_NDLS_ONLINE

    // -----------------------------------------------------------------------------------------------------------------

    void event_ExportRiff( const events::ExportRiff* eventData );
    void event_RequestToShareRiff( const events::RequestToShareRiff* eventData );

    // riff export config
    config::endlesss::Export                m_configExportOutput;

    base::EventListenerID                   m_eventLID_ExportRiff = base::EventListenerID::invalid();
    base::EventListenerID                   m_eventLID_RequestToShareRiff = base::EventListenerID::invalid();


    // ---------------------------------------------------------------------------------------------------------------------

    using JamNameRemoteResolution       = std::pair< endlesss::types::JamCouchID, std::string >;
    using JamNameRemoteFetchResultQueue = mcc::ConcurrentQueue< JamNameRemoteResolution >;

    // take a band### id and go find a public name for it, via circuitous means
    void event_BNSCacheMiss( const events::BNSCacheMiss* eventData );

    // run on main thread to work with the results of async jam name resolution
    void updateJamNameResolutionTasks( float deltaTime );

    base::EventListenerID                   m_eventLID_BNSCacheMiss = base::EventListenerID::invalid();
    JamNameRemoteFetchResultQueue           m_jamNameRemoteFetchResultQueue;
    float                                   m_jamNameRemoteFetchUpdateBroadcastTimer = 0;
};

} // namespace app
