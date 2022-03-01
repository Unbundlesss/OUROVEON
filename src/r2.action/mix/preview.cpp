//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//

#include "pch.h"

#include "mix/preview.h"

#include "ssp/ssp.file.wav.h"
#include "ssp/ssp.file.flac.h"

#include "app/core.h"
#include "app/imgui.ext.h"
#include "app/module.frontend.fonts.h"

#include "endlesss/core.constants.h"
#include "endlesss/live.stem.h"
#include "endlesss/toolkit.export.h"


namespace mix {

// ---------------------------------------------------------------------------------------------------------------------
Preview::Preview( const int32_t maxBufferSize, const int32_t sampleRate, const RiffChangeCallback& riffChangeCallback )
    : RiffMixerBase( maxBufferSize, sampleRate )
    , m_riffPlaybackSample( 0 )
    , m_riffChangeCallback( riffChangeCallback )
{

}

// ---------------------------------------------------------------------------------------------------------------------
Preview::~Preview()
{

}

// ---------------------------------------------------------------------------------------------------------------------
void Preview::renderCurrentRiff(
    const uint32_t      outputOffset,
    const uint32_t      samplesToWrite,
    const uint64_t      samplePosition )
{
    std::array< float, 8 >                  stemTimeStretch;
    std::array< float, 8 >                  stemGains;
    std::array< endlesss::live::Stem*, 8 >  stemPtr;

    const endlesss::live::Riff* currentRiff = m_riffCurrent.get();

    for ( auto stemI = 0U; stemI < 8; stemI++ )
    {
        stemTimeStretch[stemI]  = currentRiff->m_stemTimeScales[stemI];
        stemGains[stemI]        = currentRiff->m_stemGains[stemI];
        stemPtr[stemI]          = currentRiff->m_stemPtrs[stemI];
    }

    // keep note of where we are mixing in terms of the 0..N sample count of the current riff
    const auto riffLengthInSamples      = currentRiff->m_timingDetails.m_lengthInSamples;

    while ( m_riffPlaybackSample >= riffLengthInSamples )
    {
        m_riffPlaybackSample -= riffLengthInSamples;
    }

    const auto riffWrappedSampleStart   = m_riffPlaybackSample;

    for ( auto stemI = 0U; stemI < 8; stemI++ )
    {
        const auto  stemInst = stemPtr[stemI];
        const float stemGain = stemGains[stemI];

        // any stem problem -> silence
        if ( stemInst == nullptr || 
             stemInst->hasFailed() )
        {
            for ( auto sI = 0U; sI < samplesToWrite; sI++ )
            {
                m_mixChannelLeft[stemI][sI]  = 0;
                m_mixChannelRight[stemI][sI] = 0;
            }
            continue;
        }

        // get sample position in context of the riff
        uint64_t riffSample = riffWrappedSampleStart;

        for ( auto sI = 0U; sI < samplesToWrite; sI++ )
        {
            const auto sampleCount = stemInst->m_sampleCount;
            uint64_t finalSampleIdx = riffSample;

            if (stemTimeStretch[stemI] != 1.0f)
            {
                double scaleSample = (double)finalSampleIdx * stemTimeStretch[stemI];
                finalSampleIdx = (uint64_t)scaleSample;
            }
            finalSampleIdx %= sampleCount;


            m_mixChannelLeft[stemI][outputOffset + sI]  = stemInst->m_channel[0][finalSampleIdx] * stemGain;
            m_mixChannelRight[stemI][outputOffset + sI] = stemInst->m_channel[1][finalSampleIdx] * stemGain;

            riffSample++;
            if ( riffSample >= riffLengthInSamples )
                riffSample -= riffLengthInSamples;
        }
    }

    m_riffPlaybackSample += samplesToWrite;
}

// ---------------------------------------------------------------------------------------------------------------------
void Preview::update( 
    const AudioBuffer&  outputBuffer,
    const float         outputVolume,
    const uint32_t      samplesToWrite,
    const uint64_t      samplePosition )
{
    const bool shouldDrainAndStop = m_drainQueueAndStop.load();
    if ( shouldDrainAndStop )
    {
        while ( m_riffQueue.try_dequeue( m_riffCurrent ) )
        {
            if ( m_riffChangeCallback )
                m_riffChangeCallback( m_riffCurrent );
        }
        m_riffCurrent = nullptr;
        m_drainQueueAndStop = false;
    }

    bool        riffEnqueued = ( m_riffQueue.peek() != nullptr );
    const auto dequeNextRiff = [this, &riffEnqueued]()
    {
        if ( !m_riffQueue.try_dequeue( m_riffCurrent ) )
        {
            assert( false );
        }
        
        if ( m_riffChangeCallback )
            m_riffChangeCallback( m_riffCurrent );

        // update enqueued state now we just removed something from the pile
        riffEnqueued = ( m_riffQueue.peek() != nullptr );
    };

    const bool riffEmpty = ( m_riffCurrent == nullptr ||
                             m_riffCurrent->m_timingDetails.m_lengthInSamples == 0 );

    // early out when nothing's happening
    if ( riffEmpty && !riffEnqueued )
    {
        outputBuffer.applySilence();

        // reset computed riff playback variables
        m_riffPlaybackPercentage = 0;
        m_riffPlaybackBar        = 0;
        m_riffPlaybackBarSegment = 0;

        return;
    }


    if ( riffEnqueued )
    {
        if ( riffEmpty || m_lockTransitionToNextBar == false )
        {
            dequeNextRiff();

            if ( m_riffCurrent == nullptr )
            {
                outputBuffer.applySilence();
                return;
            }
        }

        const auto shiftTransisionSample = m_lockTransitionOnBeat * (m_riffCurrent->m_timingDetails.m_lengthInSamplesPerBar / m_riffCurrent->m_timingDetails.m_quarterBeats);

        const auto segmentLengthInSamples = (int64_t)m_riffCurrent->m_timingDetails.m_lengthInSamplesPerBar * m_lockTransitionBarCount;

        auto shiftedPlaybackSample = m_riffPlaybackSample - shiftTransisionSample;
        if ( shiftedPlaybackSample < 0 )
        {
            shiftedPlaybackSample += segmentLengthInSamples;
        }

        auto samplesUntilNextSegment = (uint32_t)(segmentLengthInSamples - (shiftedPlaybackSample % segmentLengthInSamples));

        if ( samplesUntilNextSegment >= samplesToWrite )
        {
            renderCurrentRiff( 0, samplesToWrite, samplePosition );
        }
        else
        {
            if ( samplesUntilNextSegment > 0 )
                renderCurrentRiff( 0, samplesUntilNextSegment, samplePosition );

            if ( riffEnqueued )
                dequeNextRiff();

            // in the case where we paused playback, write silence from the current offset
            if ( m_riffCurrent == nullptr )
            {
                for ( auto stemI = 0U; stemI < 8; stemI++ )
                {
                    for ( auto sI = samplesUntilNextSegment; sI < samplesToWrite - samplesUntilNextSegment; sI++ )
                    {
                        m_mixChannelLeft[stemI][sI] = 0;
                        m_mixChannelRight[stemI][sI] = 0;
                    }
                }
            }
            else
            {
                renderCurrentRiff( samplesUntilNextSegment, samplesToWrite - samplesUntilNextSegment, samplePosition );
            }
        }
    }
    else
    {
        renderCurrentRiff( 0, samplesToWrite, samplePosition );
    }


    // compute where we are (roughly) for the UI
    if ( m_riffCurrent )
    {
        const auto timingData = m_riffCurrent->getTimingDetails();
        timingData.ComputeProgressionAtSample(
            m_riffPlaybackSample,
            m_riffPlaybackPercentage,
            m_riffPlaybackBar,
            m_riffPlaybackBarSegment );

        m_timeInfo.samplePos          = (double)samplePosition;
        m_timeInfo.tempo              = timingData.m_bpm;
        m_timeInfo.timeSigNumerator   = timingData.m_quarterBeats;
        m_timeInfo.timeSigDenominator = 4;
    }

    mixChannelsToOutput( outputBuffer, outputVolume, samplesToWrite );
}

// ---------------------------------------------------------------------------------------------------------------------
void Preview::imgui( const app::StoragePaths& storagePaths )
{
    const auto panelVolumeModule = 105.0f;

    const auto panelRegionAvailable = ImGui::GetContentRegionAvail();
    if ( panelRegionAvailable.x < panelVolumeModule )
        return;

    // for rendering state from the current riff;
    // take a shared ptr copy here, just in case the riff is swapped out mid-tick
    endlesss::live::RiffPtr currentRiffPtr = m_riffCurrent;

    const auto currentRiff  = currentRiffPtr.get();
    bool currentRiffIsValid = ( currentRiff &&
                                currentRiff->m_syncState == endlesss::live::Riff::SyncState::Success );

    {
        if ( currentRiffIsValid )
        {
            // compute a time delta so we can format how long ago this riff was subbed
            const auto riffTimeDelta = spacetime::calculateDeltaFromNow( currentRiff->m_stTimestamp );

            // print the stats
            ImGui::TextUnformatted( currentRiff->m_uiDetails.c_str() );
            ImGui::Spacing();
            ImGui::Text( "%s | %s", currentRiff->m_uiIdentity.c_str(), riffTimeDelta.asPastTenseString( 3 ).c_str() );
            ImGui::TextUnformatted( currentRiff->m_uiTimestamp.c_str() );
        }
        else
        {
            ImGui::TextUnformatted( "" );
            ImGui::Spacing();
            ImGui::TextUnformatted( "" );
            ImGui::TextUnformatted( "" );
        }

        ImGui::Columns( 2, nullptr, false );
        ImGui::SetColumnWidth( 0, panelVolumeModule );
        ImGui::SetColumnWidth( 1, panelRegionAvailable.x - panelVolumeModule );

        // toggle transition mode
        {
            {
                ImGui::Scoped::ToggleButton highlightButton( m_lockTransitionToNextBar, true );
                if ( ImGui::Button( ICON_FA_RULER_HORIZONTAL ) )
                {
                    m_lockTransitionToNextBar = !m_lockTransitionToNextBar;
                }
            }
            ImGui::CompactTooltip( m_lockTransitionToNextBar ? "Transition Timing : On Chosen Bar" : "Transition Timing : Instant" );
            ImGui::SameLine();

            ImGui::BeginDisabledControls( !currentRiffIsValid );
            if ( ImGui::Button( ICON_FA_STOP ) )
            {
                stop();
            }
            ImGui::EndDisabledControls( !currentRiffIsValid );
            ImGui::CompactTooltip( "Stop Playback" );
        }

        ImGui::NextColumn();

        if ( currentRiffIsValid )
        {
            ImGui::BeatSegments( "##beats", currentRiff->m_timingDetails.m_quarterBeats, m_riffPlaybackBarSegment );
            ImGui::ProgressBar( (float)m_riffPlaybackPercentage, ImVec2( -1, 3.0f ), "" );
            ImGui::BeatSegments( "##bars_play", currentRiff->m_timingDetails.m_barCount, m_riffPlaybackBar, 3.0f, ImGui::GetColorU32( ImGuiCol_PlotHistogram ) );
        }

        ImGui::Columns( 1 );

        if ( currentRiffIsValid )
        {
            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::BeginDisabledControls( !m_lockTransitionToNextBar );
            {
                ImGui::TextUnformatted( "Transition On Beat" );
                ImGui::SameLine(); ImGui::SliderInt( "##tob", &m_lockTransitionOnBeat, 0, currentRiff->m_timingDetails.m_quarterBeats - 1 );

                ImGui::TextUnformatted( "Minimum Bars Before Transition" );
                ImGui::SameLine(); ImGui::RadioButton( "1x", &m_lockTransitionBarCount, 1 );
                ImGui::SameLine(); ImGui::RadioButton( "2x", &m_lockTransitionBarCount, 2 );
                ImGui::SameLine(); ImGui::RadioButton( "4x", &m_lockTransitionBarCount, 4 );
            }
            ImGui::EndDisabledControls( !m_lockTransitionToNextBar );
        }
    }
}

}