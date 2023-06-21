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

namespace config {

OURO_CONFIG( Display )
{
    // data routing
    static constexpr auto StoragePath       = IPathProvider::PathFor::SharedConfig;
    static constexpr auto StorageFilename   = "display.json";

    float       displayScale = 1.0f;
    bool        useDisplayScale = false;

    template<class Archive>
    void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( displayScale )
               , CEREAL_NVP( useDisplayScale )
        );
    }
};
using DisplayOptional = std::optional< Display >;

} // namespace config

