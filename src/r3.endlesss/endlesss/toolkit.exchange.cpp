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
namespace toolkit {

// ---------------------------------------------------------------------------------------------------------------------
void Exchange::copyDetailsFromRiff( Exchange& data, const live::RiffPtr& riff, const char* jamName )
{
    const auto* currentRiff = riff.get();
    if ( currentRiff != nullptr )
    {
        data.m_dataflags |= DataFlags_Riff;
    }
    else
    {
        data.m_dataflags  = DataFlags_Empty;
        return;
    }

    strncpy(
        data.m_jamName,
        jamName,
        endlesss::toolkit::Exchange::MaxJamName - 1 );

    const uint64_t currentRiffHash = currentRiff->getCIDHash().getID();
    data.m_riffHash = currentRiffHash;

    {
        data.m_riffTimestamp          = currentRiff->m_stTimestamp.time_since_epoch().count();
        data.m_riffRoot               = currentRiff->m_riffData.riff.root;
        data.m_riffScale              = currentRiff->m_riffData.riff.scale;
        data.m_riffBPM                = currentRiff->m_timingDetails.m_bpm;
        data.m_riffBeatSegmentCount   = currentRiff->m_timingDetails.m_quarterBeats;
    }

    for ( size_t sI = 0; sI < 8; sI++ )
    {
        const endlesss::live::Stem* stem = currentRiff->m_stemPtrs[sI];
        if ( stem != nullptr )
        {
            data.m_stemColour[sI]   = stem->m_colourU32;
            data.m_stemGain[sI]     = currentRiff->m_stemGains[sI];
            data.m_stemAnalysed[sI] = stem->isAnalysisComplete() ? 1U : 0U;

            data.setJammerName( sI, stem->m_data.user.c_str() );
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
void Exchange::copyDetailsFromProgression( Exchange& data, const live::RiffProgression& progression )
{
    data.m_riffBeatSegmentActive = progression.m_playbackBarSegment;
    data.m_riffPlaybackProgress  = (float)progression.m_playbackPercentage;
}

} // namespace toolkit
} // namespace endlesss
