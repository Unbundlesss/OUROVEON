//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#pragma once

#include "base/service.h"

namespace endlesss {

namespace cache { struct Stems; }
namespace api   { struct NetConfiguration; }

namespace services {

// base::ServiceInstance<>         // the original instance that can provide the services
//                                 // 
// base::ServiceBound<>            // a bound object that ensures the availability of the instance
                                   // this is what is passed to functions and toolkit systems to use

// ---------------------------------------------------------------------------------------------------------------------
// define set of systems one requires to bring a riff live and ready to play; fetching from the network, load/saving
// from/to the stem cache, the playback sample rate
//
struct IRiffFetchService
{
    virtual ~IRiffFetchService() = default;

    ouro_nodiscard virtual int32_t                                  getSampleRate() const = 0;          // target stem sample rate
    ouro_nodiscard virtual const endlesss::api::NetConfiguration&   getNetConfiguration() const = 0;    // access keys
    ouro_nodiscard virtual endlesss::cache::Stems&                  getStemCache() = 0;                 // cache storage for stems
    ouro_nodiscard virtual tf::Executor&                            getTaskExecutor() = 0;              // parallelisation
};

using RiffFetchInstance = base::ServiceInstance<IRiffFetchService>;
using RiffFetchProvider = base::ServiceBound<IRiffFetchService>;

// ---------------------------------------------------------------------------------------------------------------------
struct IJamNameResolveService
{
    virtual ~IJamNameResolveService() = default;

    enum class LookupResult
    {
        NotFound,                   // no clue :'(
        FoundInPrimarySource,       // subscribed stuff, public jams
        FoundInArchives,            // historical data, scraped names, misc junk
        PresumedPersonal            // everything not band#######, presumed to be personal jams
    };

    // ask to lookup a public name for a given couch ID, returning true if it was found, false if not
    ouro_nodiscard virtual LookupResult lookupJamNameAndTime(
        const endlesss::types::JamCouchID& jamID,
        std::string& resultJamName,
        uint64_t& resultTimestamp ) const = 0;      // if no timestamp can be resolved, this will be 0

    // version that eats the timestamp
    ouro_nodiscard LookupResult lookupJamName(
        const endlesss::types::JamCouchID& jamID,
        std::string& resultJamName ) const
    {
        uint64_t timestampDiscarded = 0;
        return lookupJamNameAndTime( jamID, resultJamName, timestampDiscarded );
    }
};

using JamNameResolveInstance = base::ServiceInstance<IJamNameResolveService>;
using JamNameResolveProvider = base::ServiceBound<IJamNameResolveService>;

} // namespace services
} // namespace endlesss
