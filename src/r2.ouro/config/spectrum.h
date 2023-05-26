//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  configuration for FFT processing used in Scope / wherever else
//

#pragma once

#include "config/base.h"

#include "base/utils.h"

namespace config {

OURO_CONFIG( Spectrum )
{
    // data routing
    static constexpr auto StoragePath       = IPathProvider::PathFor::SharedConfig;
    static constexpr auto StorageFilename   = "spectrum.json";

    bool            applyHannWindow = true;
    float           minDb = -8.0f;
    float           maxDb = 50.0f;

    template<class Archive>
    void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( applyHannWindow )
               , CEREAL_NVP( minDb )
               , CEREAL_NVP( maxDb )
        );
    }
};
using SpectrumOptional = std::optional< Spectrum >;

} // namespace config

