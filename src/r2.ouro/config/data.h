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

OURO_CONFIG( Data )
{
    // data routing
    static constexpr auto StoragePath       = IPathProvider::PathFor::SharedConfig;
    static constexpr auto StorageFilename   = "data.json";

    std::string     storageRoot;        // where to store the global cache & per-app outputs


    template<class Archive>
    void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( storageRoot )
        );
    }
};
using DataOptional = std::optional< Data >;

} // namespace config

