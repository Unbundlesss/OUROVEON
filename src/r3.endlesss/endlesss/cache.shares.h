//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "endlesss/config.h"
#include "endlesss/ids.h"

namespace endlesss {

namespace api { struct NetConfiguration; }

namespace cache {

// ---------------------------------------------------------------------------------------------------------------------
// a tool for downloading and processing the shared riffs data for any Endlesss user
//
struct Shares
{
    using SharedData    = std::shared_ptr< config::endlesss::SharedRiffsCache >;
    using StatusOrData  = absl::StatusOr<SharedData>;

    Shares();

    // request a download of new shared riff data for the given Endlesss username;
    // pulls all the pages of data, crunches them down and calls completionFunc() with the result 
    bool fetchAsync(
        const endlesss::api::NetConfiguration& apiCfg,
        const std::string_view username,
        tf::Executor& taskExecutor,
        const std::function< void( StatusOrData ) > completionFunc );

protected:

    std::regex          m_regexExtractBandName;

    tf::Taskflow        m_taskFlow;
};

} // namespace cache
} // namespace endlesss
