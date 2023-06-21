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

#include "endlesss/config.h"
#include "endlesss/ids.h"

namespace endlesss {

namespace api { struct NetConfiguration; }

namespace cache {

// ---------------------------------------------------------------------------------------------------------------------
// A cache for all a users' shared riffs, filled via public api
//
struct Shares
{
    static constexpr auto cFilename = "cache.shares.json";

    Shares();
    ~Shares();

    struct Data
    {
        uint32_t                            m_version = 1;
        std::string                         m_username;
        std::size_t                         m_count = 0;        // number of retrieved shares

        // flat storage of m_count shared riff details
        std::vector< std::string >          m_names;
        std::vector< types::RiffCouchID >   m_riffIDs;
        std::vector< types::JamCouchID >    m_jamIDs;
        std::vector< uint64_t >             m_timestamps;
        std::vector< spacetime::Delta >     m_timestampDeltas;
    };
    using SharedData = std::shared_ptr< Data >;


    // returns true if the worker task(s) are running, fetching new data
    ouro_nodiscard inline bool isFetchingData() const
    {
        return m_asyncRefreshing;
    }

    // request a download of new shared riff data for the given Endlesss username
    bool fetchAsync(
        const endlesss::api::NetConfiguration& apiCfg,
        const std::string& username,
        tf::Executor& taskExecutor );

    // return the current data set, on success; otherwise, a failure with the current reason we have no data
    ouro_nodiscard inline absl::StatusOr<SharedData> getCurrentData()
    {
        if ( isFetchingData() )
            return absl::NotFoundError( "Data refreshing" );
        if ( m_data == nullptr )
            return absl::NotFoundError( "Data absent / failed to update" );

        return m_data;
    }

protected:

    std::regex          m_regexExtractBandName;

    std::atomic_bool    m_asyncRefreshing = false;
    tf::Taskflow        m_taskFlow;
    SharedData          m_data;
};

} // namespace cache
} // namespace endlesss
