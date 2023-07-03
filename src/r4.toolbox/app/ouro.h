//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "app/core.h"

#include "discord/config.h"
#include "endlesss/core.services.h"

namespace rec { struct IRecordable; }
namespace app {

// ---------------------------------------------------------------------------------------------------------------------
// OuroApp is a small shim for UI apps to build upon that adds some basic Endlesss features out the gate; like a standard
// "login" screen that offers audio/services configuration and validating a connection to the Endlesss backend
//
struct OuroApp : public CoreGUI,
                 public endlesss::services::RiffFetch,              // natively support using the built-in member services to load riffs
                 public endlesss::services::IJamNameCacheServices   // assume base level support for abstract jam name resolution
{
    OuroApp()
        : CoreGUI()
    {}

protected:

    // from CoreGUI
    // this inserts the generic ouro app configuration preflight UI; when that closes, we pass to EntrypointOuro
    virtual int EntrypointGUI() override;

    // inheritants implement this as app entrypoint
    virtual int EntrypointOuro() = 0;

protected:

    // endlesss::services::RiffFetch
    int32_t                                 getSampleRate() const override;
    const endlesss::api::NetConfiguration&  getNetConfiguration() const override { return *m_networkConfiguration; }
    endlesss::cache::Stems&                 getStemCache() override { return m_stemCache; }
    tf::Executor&                           getTaskExecutor() override { return m_taskExecutor; }

    // endlesss::services::IJamNameCacheServices
    bool lookupNameForJam( const endlesss::types::JamCouchID& jamID, std::string& result ) const override;


    // validated storage locations for the app
    // <optional> because this is expected to be configured once the boot sequence has checked / changed
    //              the root storage path; once the app starts for real, this can be assumed to be valid
    std::optional< StoragePaths >           m_storagePaths = std::nullopt;

    // -------------

    // stem cache maintenance; checks for memory pressure, triggers async prune operation to try and guide towards
    // chosen memory limit
    void maintainStemCacheAsync();
    void ensureStemCacheChecksComplete();

    // the global live-instance stem cache, used to populate riffs when preparing for playback
    endlesss::cache::Stems                  m_stemCache;
    // timer used to check in and auto-prune the stem cache if it busts past the set memory usage targets
    spacetime::Moment                       m_stemCacheLastPruneCheck;
    // async bits to run the prune() fn via TF
    tf::Taskflow                            m_stemCachePruneTask;
    std::optional< tf::Future<void> >       m_stemCachePruneFuture = std::nullopt;


    // -------------

    void onEvent_ExportRiff( const base::IEvent& eventRef );

    // riff export config
    config::endlesss::Export                m_configExportOutput;

    base::EventListenerID                   m_eventListenerRiffExport;

};

} // namespace app
