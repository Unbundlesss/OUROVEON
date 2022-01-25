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

struct Frontend
{
    // data routing
    static constexpr auto StoragePath       = IPathProvider::PathFor::PerAppConfig;
    static constexpr auto StorageFilename   = "frontend.json";

    uint32_t        appWidth  = 1600;
    uint32_t        appHeight = 900;


    template<class Archive>
    void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( appWidth )
               , CEREAL_NVP( appHeight )
        );
    }

    bool postLoad()
    {
        // check canvas sizes aren't bonkers
        if ( appWidth  < 640 || appWidth  > 16 * 1024 ||
             appHeight < 480 || appHeight > 16 * 1024 )
        {
            blog::error::cfg( "app canvas size seems invalid [{} x {}], resetting", appWidth, appHeight );
            appWidth  = 1600;
            appHeight = 900;
        }
        return true;
    }
};
using FrontendOptional = std::optional< Frontend >;

} // namespace config

