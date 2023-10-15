//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  configuration for FFT processing used in Scope / wherever else
//

#pragma once

#include "config/base.h"
#include "base/utils.h"

// q
#include <q/support/decibel.hpp>

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

    // convert value to decibels and normalise between the current min/max levels
    ouro_nodiscard inline float headroomNormaliseDb( float linearValue ) const
    {
        const double headroom   = static_cast<double>(maxDb) - static_cast<double>(minDb);

        // convert to dB to help normalise the range
        double fftDb = cycfi::q::lin_to_db( linearValue ).rep;

        // then clip and rescale to 0..1 via headroom dB range
        fftDb = std::max( 0.0, fftDb + std::abs( minDb ) );
        fftDb = std::min( 1.0, fftDb / headroom );

        return static_cast<float>(fftDb);
    }
};
using SpectrumOptional = std::optional< Spectrum >;

} // namespace config

