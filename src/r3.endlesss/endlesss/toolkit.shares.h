//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#pragma once

#include "endlesss/config.h"
#include "endlesss/ids.h"
#include "endlesss/api.h"

namespace endlesss {

namespace toolkit {

// code used mostly by shared-riff resolvers where Endlesss doesn't give us the jam couch ID and we have to go rummaging
// through the loops data to find something usable
struct RiffBandExtractor
{
    RiffBandExtractor();

    std::string estimateJamCouchID( const api::SharedRiffsByUser::Data& sharedData ) const;

private:
    std::regex          m_regexExtractBandName;
};

// ---------------------------------------------------------------------------------------------------------------------
// a tool for asynchronously downloading and processing the shared riffs data for any Endlesss user
//
struct Shares
{
    using SharedData    = std::shared_ptr< config::endlesss::SharedRiffsCache >;
    using StatusOrData  = absl::StatusOr<SharedData>;

    // produce Tf graph to execute; requests a download of new shared riff data for the given Endlesss username;
    // pulls all the pages of data, crunches them down and calls completionFunc() with the result 
    tf::Taskflow taskFetchLatest(
        const endlesss::api::NetConfiguration& apiCfg,
        std::string username,
        std::function< void( StatusOrData ) > completionFunc );

protected:

    RiffBandExtractor    m_riffBandExtractor;
};


} // namespace toolkit
} // namespace endlesss
