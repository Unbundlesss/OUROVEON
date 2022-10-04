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

OURO_CONFIG( Frontend )
{
    // data routing
    static constexpr auto StoragePath       = IPathProvider::PathFor::PerAppConfig;
    static constexpr auto StorageFilename   = "frontend.json";

    static constexpr uint32_t DefaultWidth  = 1600;
    static constexpr uint32_t DefaultHeight = 900;

    uint32_t        appWidth     = DefaultWidth;
    uint32_t        appHeight    = DefaultHeight;

    int32_t         appPositionX = 0;
    int32_t         appPositionY = 0;
    bool            appPositionValid = false;

    template<class Archive>
    void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( appWidth )
               , CEREAL_NVP( appHeight )
               , CEREAL_OPTIONAL_NVP( appPositionX )
               , CEREAL_OPTIONAL_NVP( appPositionY )
               , CEREAL_OPTIONAL_NVP( appPositionValid )
        );
    }

    bool postLoad()
    {
        // check canvas sizes aren't bonkers
        if ( appWidth  < 640 || appWidth  > 16 * 1024 ||
             appHeight < 480 || appHeight > 16 * 1024 )
        {
            blog::error::cfg( "app canvas size seems invalid [{} x {}], resetting", appWidth, appHeight );
            appWidth  = DefaultWidth;
            appHeight = DefaultHeight;
        }
        return true;
    }
};
using FrontendOptional = std::optional< Frontend >;

} // namespace config

