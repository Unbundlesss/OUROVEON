//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "endlesss/core.types.h"
#include "endlesss/core.services.h"

namespace endlesss { namespace toolkit { struct Warehouse; } }

namespace ux {

    struct JamPrecacheState
    {
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

        State                           m_state = State::Intro;
        std::size_t                     m_currentStemIndex = 0;

        std::atomic_uint32_t            m_statsStemsAlreadyInCache = 0;
        std::atomic_uint32_t            m_statsStemsDownloaded = 0;
        std::atomic_uint32_t            m_statsStemsMissingFromDb = 0;
        std::atomic_uint32_t            m_statsStemsFailedToDownload = 0;

        std::atomic_uint32_t            m_downloadsDispatched = 0;

        std::mutex                      m_averageDownloadTimeMutex;
        base::RollingAverage< 6 >       m_averageDownloadTimeMillis;
        std::size_t                     m_averageDownloadMeasurements = 0;
    };

    // 
    void modalJamPrecache(
        const char* title,                                      // a imgui label to use with ImGui::OpenPopup
        JamPrecacheState& jamPrecacheState,                     // UI state 
        const endlesss::toolkit::Warehouse& warehouse,          // warehouse access to pull stem data
        endlesss::services::RiffFetchProvider& fetchProvider,   // network fetch services
        tf::Executor& taskExecutor );                           // async support via tf

} // namespace ux
