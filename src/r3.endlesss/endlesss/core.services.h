//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "base/service.h"

namespace endlesss {

namespace cache { struct Stems; }
namespace api   { struct NetConfiguration; }

namespace services {

// ---------------------------------------------------------------------------------------------------------------------
// define set of systems one requires to bring a riff live and ready to play; fetching from the network, load/saving
// from/to the stem cache, the playback sample rate
//
struct RiffFetch
{
    virtual ~RiffFetch() = default;

    ouro_nodiscard virtual int32_t                                  getSampleRate() const = 0;          // target stem sample rate
    ouro_nodiscard virtual const endlesss::api::NetConfiguration&   getNetConfiguration() const = 0;    // access keys
    ouro_nodiscard virtual endlesss::cache::Stems&                  getStemCache() = 0;                 // cache storage for stems
    ouro_nodiscard virtual tf::Executor&                            getTaskExecutor() = 0;              // parallelisation
};

using RiffFetchInstance = base::ServiceInstance<RiffFetch>;     // the original instance that can provide the services
                                                                // 
using RiffFetchProvider = base::ServiceBound<RiffFetch>;        // a bound object that ensures the availability of the instance
                                                                // this is what is passed to functions and toolkit systems to use

} // namespace services
} // namespace endlesss
