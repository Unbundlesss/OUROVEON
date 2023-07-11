//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "spacetime/chronicle.h"
#include "endlesss/ids.h"
#include "endlesss/api.h"

namespace app { struct StoragePaths; }

namespace endlesss {

namespace cache { struct Jams; }

namespace toolkit {

// ---------------------------------------------------------------------------------------------------------------------
//
struct Warehouse
{
    struct ContentsReport
    {
        std::vector< types::JamCouchID >    m_jamCouchIDs;
        std::vector< int64_t >              m_populatedRiffs;
        std::vector< int64_t >              m_unpopulatedRiffs;
        std::vector< int64_t >              m_populatedStems;
        std::vector< int64_t >              m_unpopulatedStems;
    };
    using ContentsReportCallback = std::function<void( const ContentsReport& report )>;

    // SoA extraction of a set of riff data
    struct JamSlice
    {
        DECLARE_NO_COPY_NO_MOVE( JamSlice );

        JamSlice() = delete;
        JamSlice( const types::JamCouchID& jamID, const size_t elementsToReserve )
            : m_jamID( jamID )
        {
            reserve( elementsToReserve );
        }

        types::JamCouchID                           m_jamID;

        // per-riff information
        std::vector< types::RiffCouchID >           m_ids;
        std::vector< spacetime::InSeconds >         m_timestamps;
        std::vector< uint64_t >                     m_userhash;
        std::vector< uint32_t >                     m_roots;
        std::vector< uint32_t >                     m_scales;
        std::vector< float >                        m_bpms;

        // riff-adjacency information
        std::vector< int32_t >                      m_deltaSeconds;
        std::vector< int8_t >                       m_deltaStem;

    protected:
        inline void reserve( const size_t elements )
        {
            m_ids.reserve( elements );
            m_timestamps.reserve( elements );
            m_userhash.reserve( elements );
            m_roots.reserve( elements );
            m_scales.reserve( elements );
            m_bpms.reserve( elements );

            m_deltaSeconds.reserve( elements );
            m_deltaStem.reserve( elements );
        }
    };
    using JamSlicePtr = std::unique_ptr<JamSlice>;
    using JamSliceCallback = std::function<void( const types::JamCouchID& jamCouchID, JamSlicePtr&& resultSlice )>;


    struct ITask;
    struct INetworkTask;

    using WorkUpdateCallback = std::function<void( const bool tasksRunning, const std::string& currentTask ) >;

    Warehouse( const app::StoragePaths& storagePaths, api::NetConfiguration::Shared& networkConfig );
    ~Warehouse();

    static std::string  m_databaseFile;
    using SqlDB = sqlite::Database<m_databaseFile>;

    void setNetworkConfiguration( const api::NetConfiguration& ncfg );

    void setCallbackWorkReport( const WorkUpdateCallback& cb );
    void setCallbackContentsReport( const ContentsReportCallback& cb );

    void upsertJamDictionaryFromCache( const cache::Jams& jamCache );
    void extractJamDictionary( types::JamIDToNameMap& jamDictionary ) const;
    
    void addOrUpdateJamSnapshot( const types::JamCouchID& jamCouchID );

    void addJamSliceRequest( const types::JamCouchID& jamCouchID, const JamSliceCallback& callbackOnCompletion );

    bool fetchSingleRiffByID( const endlesss::types::RiffCouchID& riffID, endlesss::types::RiffComplete& result );

    // control background task processing; pausing will stop any new work from being generated
    void workerTogglePause();
    ouro_nodiscard bool workerIsPaused() const { return m_workerThreadPaused; }

    // erase the given jam from the warehouse database entirely
    void requestJamPurge( const types::JamCouchID& jamCouchID );

    // passing in the NetConfiguration for API access to Endlesss is optional; users should not enqueue tasks
    // that require it if it isn't present (and tasks will check and bail in error)
    ouro_nodiscard bool hasFullEndlesssNetworkAccess() const { return m_networkConfiguration->hasAccess( api::NetConfiguration::Access::Authenticated ); }

protected:

    friend ITask;
    struct TaskSchedule;

    void threadWorker();

    api::NetConfiguration::Shared           m_networkConfiguration;

    std::unique_ptr<TaskSchedule>           m_taskSchedule;

    WorkUpdateCallback                      m_cbWorkUpdate              = nullptr;
    WorkUpdateCallback                      m_cbWorkUpdateToInstall     = nullptr;
    ContentsReportCallback                  m_cbContentsReport          = nullptr;
    ContentsReportCallback                  m_cbContentsReportToInstall = nullptr;
    std::mutex                              m_cbMutex;

    std::unique_ptr<std::thread>            m_workerThread;
    std::atomic_bool                        m_workerThreadAlive;
    std::atomic_bool                        m_workerThreadPaused;
};

} // namespace toolkit
} // namespace endlesss
