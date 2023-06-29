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

namespace toolkit {

// ---------------------------------------------------------------------------------------------------------------------
// a tool for asynchronously downloading and processing the shared riffs data for any Endlesss user
//
struct Shares
{
    using SharedData    = std::shared_ptr< config::endlesss::SharedRiffsCache >;
    using StatusOrData  = absl::StatusOr<SharedData>;

    Shares();

    // produce Tf graph to execute; requests a download of new shared riff data for the given Endlesss username;
    // pulls all the pages of data, crunches them down and calls completionFunc() with the result 
    tf::Taskflow taskFetchLatest(
        const endlesss::api::NetConfiguration& apiCfg,
        std::string username,
        std::function< void( StatusOrData ) > completionFunc );

protected:

    std::regex          m_regexExtractBandName;
};

} // namespace toolkit
} // namespace endlesss
