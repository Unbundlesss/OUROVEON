//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "config/base.h"

#include "base/utils.h"

namespace config {

OURO_CONFIG( Performance )
{
    // data routing
    static constexpr auto StoragePath       = IPathProvider::PathFor::SharedConfig;
    static constexpr auto StorageFilename   = "performance.json";

    // to avoid prune-thrashing (ask your mother), set some kind of reasonable minimum cache size lower bound
    static constexpr int32_t stemCachePruneLevelMinimumMb = 200;


    // approximate size (in Mb) of live stem cache before we run some garbage collection to trim it down
    int32_t         stemCacheAutoPruneAtMemoryUsageMb = 2048;

    // when possible viable, keep this number of live full riff instances alive once they are fully loaded
    int32_t         liveRiffInstancePoolSize = 64;

    // optionally enable/disable the Vibes rendering system at the root to avoid burning any memory or GPU if desired
    bool            enableVibesRenderer = true;


    template<class Archive>
    void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( stemCacheAutoPruneAtMemoryUsageMb )
               , CEREAL_NVP( liveRiffInstancePoolSize )
               , CEREAL_OPTIONAL_NVP( enableVibesRenderer )
        );
    }

    inline void clampLimits()
    {
        stemCacheAutoPruneAtMemoryUsageMb   = std::max( stemCacheAutoPruneAtMemoryUsageMb, stemCachePruneLevelMinimumMb );
        liveRiffInstancePoolSize            = std::max( liveRiffInstancePoolSize, 1 );
    }

    // ensure nothing weird arriving
    bool postLoad()
    {
        clampLimits();
        return true;
    }
};
using PerformanceOptional = std::optional< Performance >;

} // namespace config

