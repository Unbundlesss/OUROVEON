//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#pragma once

#include "config/base.h"

namespace config {

OURO_CONFIG( NoNet )
{
    // data routing
    static constexpr auto StoragePath       = IPathProvider::PathFor::SharedConfig;
    static constexpr auto StorageFilename   = "nonet.json";

    std::string     impersonationUsername;        // username we can mock when in no-network mode for any other code that needs it


    template<class Archive>
    void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( impersonationUsername )
        );
    }
};

} // namespace config

