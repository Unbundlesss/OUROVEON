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

#include "base/metaenum.h"
#include "base/utils.h"

namespace config {

// ---------------------------------------------------------------------------------------------------------------------
OURO_CONFIG( Audio )
{
    // data routing
    static constexpr auto StoragePath       = IPathProvider::PathFor::SharedConfig;
    static constexpr auto StorageFilename   = "audio.json";


    uint32_t        sampleRate = 44100;
    std::string     lastDevice = "";
    bool            lowLatency = true;
    uint32_t        bufferSize = 0;


    template<class Archive>
    void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( sampleRate )
               , CEREAL_NVP( lastDevice )
               , CEREAL_NVP( lowLatency )
               , CEREAL_OPTIONAL_NVP( bufferSize )
        );
    }
};
using AudioOptional = std::optional< Audio >;



} // namespace config

