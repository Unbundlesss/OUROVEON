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

struct Midi : public Base
{
    // data routing
    static constexpr auto StoragePath       = IPathProvider::PathFor::SharedConfig;
    static constexpr auto StorageFilename   = "midi.json";

    std::vector< uint8_t >      relativeDecrementSignals;
    std::vector< uint8_t >      relativeIncrementSignals;

    template<class Archive>
    void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( relativeDecrementSignals )
               , CEREAL_NVP( relativeIncrementSignals )
        );
    }
};
using MidiOptional = std::optional< Midi >;

} // namespace config

