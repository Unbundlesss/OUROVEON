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
    if ( samplesToWrite == 0 )
        return;

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

        m_riffTransitionedInMix = true;
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

        const auto shiftTransisionSample = m_lockTransitionOnBeat * ( m_riffCurrent->m_timingDetails.m_lengthInSamplesPerBar / m_riffCurrent->m_timingDetails.m_quarterBeats );

        auto segmentLengthInSamples = (int64_t)m_riffCurrent->m_timingDetails.m_lengthInSamplesPerBar;
        switch ( m_lockTransitionBarCount )
        {
            case TransitionBarCount::Eighth:    segmentLengthInSamples /= 8; break;
            case TransitionBarCount::Quarter:   segmentLengthInSamples /= 4; break;
            case TransitionBarCount::Half:      segmentLengthInSamples /= 2; break;
            case TransitionBarCount::Two:       segmentLengthInSamples *= 2; break;
            case TransitionBarCount::Four:      segmentLengthInSamples *= 4; break;
            default:
            case TransitionBarCount::One:
                break;
        }
        

        auto shiftedPlaybackSample = m_riffPlaybackSample - shiftTransisionSample;
        if ( shiftedPlaybackSample < 0 )
        {
            shiftedPlaybackSample += segmentLengthInSamples;
        }

        auto samplesUntilNextSegment = (uint32_t)(segmentLengthInSamples - (shiftedPlaybackSample % segmentLengthInSamples));

        if ( samplesUntilNextSegment > samplesToWrite )
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

ImVec4 GetTransitionColourVec4( const float lerpT )
{
    const auto colour1 = ImGui::GetStyleColorVec4( ImGuiCol_PlotHistogram );
    const auto colour2 = ImGui::GetStyleColorVec4( ImGuiCol_Text );
    return lerpVec4( colour1, colour2, lerpT );
}

// ---------------------------------------------------------------------------------------------------------------------
void Preview::imgui( const app::StoragePaths& storagePaths )
{
    const auto panelVolumeModule = 75.0f;

    const auto panelRegionAvailable = ImGui::GetContentRegionAvail();
    if ( panelRegionAvailable.x < panelVolumeModule )
        return;

    // simple pulse from a riff change in the mixer, pulse our transition bits
    m_riffTransitionedUI += ( 0.0f - m_riffTransitionedUI ) * 0.165f;
    if ( m_riffTransitionedInMix )
    {
        m_riffTransitionedUI = 1.0f;
        m_riffTransitionedInMix = false;
    }

    const auto riffTransitColourU32 = ImGui::GetColorU32( GetTransitionColourVec4( m_riffTransitionedUI ) );

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

        // toggle transition mode; take copy of atomic to update/use in ui
        bool lockTransitionLocal = m_lockTransitionToNextBar.load();
        {
            {
                ImGui::Scoped::ToggleButton highlightButton( lockTransitionLocal, true );
                if ( ImGui::Button( ICON_FA_RULER_HORIZONTAL, ImVec2( 30.0f, 0.0f ) ) )
                {
                    lockTransitionLocal = !lockTransitionLocal;
                    m_lockTransitionToNextBar = lockTransitionLocal;
                }
            }
            ImGui::CompactTooltip( lockTransitionLocal ? "Transition Timing : On Chosen Bar" : "Transition Timing : Instant" );
            ImGui::SameLine();

            ImGui::BeginDisabledControls( !currentRiffIsValid );
            if ( ImGui::Button( ICON_FA_STOP, ImVec2( 30.0f, 0.0f ) ) )
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
            ImGui::BeatSegments( "##bars_play", currentRiff->m_timingDetails.m_barCount, m_riffPlaybackBar, 3.0f, riffTransitColourU32 );
        }


        {
            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::BeginDisabledControls( !lockTransitionLocal );

            ImGui::NextColumn();
            ImGui::TextUnformatted( "Offset" );
            ImGui::NextColumn();
            {
                if ( currentRiffIsValid )
                {
                    float barBlockArea = ImGui::GetContentRegionAvail().x;
                    switch ( m_lockTransitionBarCount )
                    {
                        case TransitionBarCount::Eighth:    barBlockArea *= 0.125f; break;
                        case TransitionBarCount::Quarter:   barBlockArea *= 0.25f;  break;
                        case TransitionBarCount::Half:      barBlockArea *= 0.5f;   break;
                        default:
                            break;
                    }
                    const float barButtonWidth  = 10.0f;
                    const float barBlockWidth   = ( barBlockArea - barButtonWidth ) / (float)currentRiff->m_timingDetails.m_quarterBeats;
                    const float barSpacerWidth  = ( barBlockWidth - barButtonWidth );

                    const auto barButtonDim = ImVec2( barButtonWidth, ImGui::GetTextLineHeight() );

                    for ( auto qb = 0; qb < currentRiff->m_timingDetails.m_quarterBeats; qb++ )
                    {
                        if ( qb > 0 )
                            ImGui::SameLine( 0, barSpacerWidth );

                        ImGui::PushID( qb );
                        ImGui::Scoped::ToggleButtonLit lit( m_lockTransitionOnBeat == qb, riffTransitColourU32 );
                        if ( ImGui::Button( "##btr", barButtonDim ) )
                        {
                            m_lockTransitionOnBeat = qb;
                        }
                        ImGui::PopID();
                    }
                    {
                        ImGui::SameLine( 0, barSpacerWidth );
                        ImGui::BeginDisabledControls( true );
                        ImGui::Button( "##endpt", barButtonDim );
                        ImGui::EndDisabledControls( true );
                    }
                }
                else
                {
                    ImGui::TextUnformatted( "" );
                }

                ImGui::NextColumn();
                ImGui::TextUnformatted( "Repeat" );
                ImGui::NextColumn();

                const auto buttonSize = ImVec2( ( ImGui::GetContentRegionAvail().x - ( 4.0f * 5.0f ) ) / 6.0f, ImGui::GetTextLineHeight() * 1.25f );

                META_FOREACH( TransitionBarCount, lb )
                {
                    if ( lb != TransitionBarCount::Eighth )
                        ImGui::SameLine(0, 4.0f);

                    ImGui::Scoped::ToggleButtonLit lit( m_lockTransitionBarCount == lb, riffTransitColourU32 );
                    if ( ImGui::Button( TransitionBarCount::toString( lb ), buttonSize ) )
                    {
                        m_lockTransitionBarCount = lb;
                    }
                }
            }
            ImGui::EndDisabledControls( !lockTransitionLocal );
        }

        ImGui::Columns( 1 );
    }
}

}