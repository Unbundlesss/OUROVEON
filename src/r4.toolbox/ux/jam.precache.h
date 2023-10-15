//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#pragma once

#include "endlesss/core.types.h"
#include "endlesss/core.services.h"

#include "spacetime/moment.h"

namespace endlesss { namespace toolkit { struct Warehouse; } }

namespace ux {

    struct JamPrecacheState
    {
        static constexpr std::size_t cSyncSamples = 16;

        using Instance = std::shared_ptr<JamPrecacheState>;

        enum class State
        {
            Intro,
            Aborted,
            Preflight,
            Download,
            Complete
        };

        JamPrecacheState() = delete;
        JamPrecacheState( endlesss::types::JamCouchID& jamID )
            : m_jamCouchID( jamID )
        {
        }

        void imgui(
            const endlesss::toolkit::Warehouse& warehouse,
            endlesss::services::RiffFetchProvider& fetchProvider,
            tf::Executor& taskExecutor );

        endlesss::types::JamCouchID     m_jamCouchID;
        endlesss::types::StemCouchIDs   m_stemIDs;

        bool                            m_enableSiphonMode = false;

        int32_t                         m_maximumDownloadsInFlight = 4;

        State                           m_state = State::Intro;
        std::size_t                     m_currentStemIndex = 0;

        std::atomic_uint32_t            m_statsStemsAlreadyInCache = 0;
        std::atomic_uint32_t            m_statsStemsDownloaded = 0;
        std::atomic_uint32_t            m_statsStemsMissingFromDb = 0;
        std::atomic_uint32_t            m_statsStemsFailedToDownload = 0;

        std::atomic_uint32_t            m_downloadsDispatched = 0;

        spacetime::Moment               m_syncTimer;
        base::RollingAverage< cSyncSamples >
                                        m_averageSyncTimeMillis;
        std::size_t                     m_averageSyncMeasurements = 0;
    };

    // 
    void modalJamPrecache(
        const char* title,                                      // a imgui label to use with ImGui::OpenPopup
        JamPrecacheState& jamPrecacheState,                     // UI state 
        const endlesss::toolkit::Warehouse& warehouse,          // warehouse access to pull stem data
        endlesss::services::RiffFetchProvider& fetchProvider,   // network fetch services
        tf::Executor& taskExecutor );                           // async support via tf

} // namespace ux
