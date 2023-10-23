//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//

#include "pch.h"

#include "app/module.frontend.fonts.h"
#include "colour/preset.h"
#include "endlesss/all.h"

namespace ImGui {
namespace ux {

struct StemAnalysisState
{
    endlesss::types::RiffCouchID
                        m_currentRiffID;

    int32_t             m_stemIndex = 1;
    int32_t             m_dataView = 0;

    bool                m_runAnalysis = true;

    endlesss::live::Stem::Processing::UPtr  m_processing;
    endlesss::live::StemAnalysisData        m_analysis;
};

void StemAnalysis( endlesss::live::RiffPtr& liveRiff, const int32_t audioSampleRate )
{
    static StemAnalysisState state;

    if ( state.m_processing == nullptr )
        state.m_processing = endlesss::live::Stem::createStemProcessing( audioSampleRate );

    if ( ImGui::Begin( ICON_FA_CIRCLE_INFO " Stem Analysis###stem_analysis" ) )
    {
        if ( liveRiff == nullptr )
        {
            ImGui::TextUnformatted( "No riff loaded" );
            ImGui::End();
            return;
        }

        // re-run analysis on riff change
        if ( liveRiff->m_riffData.riff.couchID != state.m_currentRiffID )
        {
            state.m_currentRiffID = liveRiff->m_riffData.riff.couchID;
            state.m_runAnalysis = true;
        }

        if ( ImGui::InputFloat( "Peak Follow Duration", &state.m_processing->m_tuning.m_beatFollowDuration, 0.01f, 0.05f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue ) )
            state.m_runAnalysis = true;
        if ( ImGui::InputFloat( "RMS Follow Duration", &state.m_processing->m_tuning.m_waveFollowDuration, 0.01f, 0.05f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue ) )
            state.m_runAnalysis = true;
        if ( ImGui::InputFloat( "Tracker Sensitivity", &state.m_processing->m_tuning.m_trackerSensitivity, 0.01f, 0.05f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue ) )
            state.m_runAnalysis = true;
        if ( ImGui::InputFloat( "Tracker Hysteresis", &state.m_processing->m_tuning.m_trackerHysteresis, 0.01f, 0.05f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue ) )
            state.m_runAnalysis = true;
        if ( ImGui::SliderInt( "Stem", &state.m_stemIndex, 1, 8 ) )
            state.m_runAnalysis = true;

        // load requested stem from riff
        const endlesss::live::Stem* liveStem = liveRiff->m_stemPtrs[state.m_stemIndex - 1];
        if ( liveStem == nullptr || 
             liveStem->getAnalysisState() != endlesss::live::Stem::AnalysisState::AnalysisValid )
        {
            ImGui::TextUnformatted( "Stem empty or analysis incomplete" );
            ImGui::End();
            return;
        }

        static constexpr int32_t sampleStep = 128;
        float approxWaveformViewWidth = ImGui::GetContentRegionAvail().x;
        int32_t approxSampleStepRate = static_cast<int32_t>( std::floor( liveStem->m_sampleCount / approxWaveformViewWidth ) );
        int32_t steppedSampleCount = liveStem->m_sampleCount / sampleStep;

        ImPlot::SetNextAxesLimits(
            0,
            (double)steppedSampleCount,
            -1.0,
            1.0,
            ImGuiCond_Always );

        // constantly update a local analysis chunk
        if ( state.m_runAnalysis )
        {
            liveStem->analyse( *state.m_processing, state.m_analysis );
            state.m_runAnalysis = false;
        }

        if ( ImPlot::BeginPlot( "##StemDataPlot_0", ImVec2( -1, 200 ), ImPlotFlags_None ) )
        {
            ImPlot::SetupAxis( ImAxis_X1, nullptr, ImPlotAxisFlags_NoDecorations );
            ImPlot::SetupAxis( ImAxis_Y1, nullptr, ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_LockMin );

            ImPlot::SetNextFillStyle( colour::shades::blue_gray.dark(), 0.8f );
            ImPlot::PlotBars( "##waveform", liveStem->m_channel[0], steppedSampleCount, 0.67, 0, 0, 0, sizeof(float) * sampleStep );

            ImPlot::EndPlot();
        }

        ImPlot::SetNextAxesLimits(
            0,
            (double)steppedSampleCount,
            0.0,
            255.0,
            ImGuiCond_Always );

        ImGui::SliderInt( "View", &state.m_dataView, 0, 3 );

        if ( ImPlot::BeginPlot( "##StemDataPlot_1", ImVec2( -1, 200 ), ImPlotFlags_None ) )
        {
            ImPlot::SetupAxis( ImAxis_X1, nullptr, ImPlotAxisFlags_NoDecorations );
            ImPlot::SetupAxis( ImAxis_Y1, nullptr, ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_LockMin );

            ImPlot::SetNextFillStyle( colour::shades::toast.neutral() );

            switch ( state.m_dataView )
            {
            case 0:
                ImPlot::PlotBars( "##energy", state.m_analysis.m_psaWave.data(), steppedSampleCount, 0.67, 0, 0, 0, sizeof( uint8_t ) * sampleStep );
                break;
            case 1:
                ImPlot::PlotBars( "##energy", state.m_analysis.m_psaBeat.data(), steppedSampleCount, 0.67, 0, 0, 0, sizeof( uint8_t ) * sampleStep );
                break;
            case 2:
                ImPlot::PlotBars( "##energy", state.m_analysis.m_psaLowFreq.data(), steppedSampleCount, 0.67, 0, 0, 0, sizeof( uint8_t ) * sampleStep );
                break;
            case 3:
                ImPlot::PlotBars( "##energy", state.m_analysis.m_psaHighFreq.data(), steppedSampleCount, 0.67, 0, 0, 0, sizeof( uint8_t ) * sampleStep );
                break;
            }

            for ( auto aI = 0; aI < state.m_analysis.m_beatBitfield.size(); aI++ )
            {
                for ( int32_t bI = 0; bI < 64; bI++ )
                {
                    if ( state.m_analysis.m_beatBitfield[aI] & 1ULL << bI )
                    {
                        uint64_t bitSample = ( ( aI * 64 ) + bI );
                        ImPlot::OverlayLineX( (float)bitSample / (float)sampleStep, colour::shades::lime.neutral(0.6f) );
                    }
                }
            }

            ImPlot::EndPlot();
        }

    }
    ImGui::End();
}

} // namespace ux
} // name
