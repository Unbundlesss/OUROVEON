//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"

#include "endlesss/toolkit.exchange.h"
#include "endlesss/live.riff.h"
#include "endlesss/live.stem.h"

namespace endlesss {

void Exchange::populatePartialFromRiffPtr( const live::RiffPtr& riff, Exchange& data )
{
    const auto* currentRiff = riff.get();

    {
        data.m_timestamp              = currentRiff->m_stTimestamp.time_since_epoch().count();

        data.m_riffRoot               = currentRiff->m_riffData.riff.root;
        data.m_riffScale              = currentRiff->m_riffData.riff.scale;
        data.m_riffBPM                = currentRiff->m_timingDetails.m_bpm;
        data.m_riffBeatSegmentCount   = currentRiff->m_timingDetails.m_quarterBeats;
    }

    const uint64_t currentRiffHash = currentRiff->getCIDHash().getID();
    data.m_riffHash = currentRiffHash;

    for ( size_t sI = 0; sI < 8; sI++ )
    {
        const endlesss::live::Stem* stem = currentRiff->m_stemPtrs[sI];
        if ( stem != nullptr )
        {
            data.m_stemColour[sI] = stem->m_colourU32;
            data.m_stemGain[sI] = currentRiff->m_stemGains[sI];

            data.setJammerName( sI, stem->m_data.user.c_str() );
        }
    }
}

} // namespace endlesss
