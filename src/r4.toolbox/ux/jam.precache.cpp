//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"
#include "ux/jam.precache.h"

#include "spacetime/moment.h"
#include "spacetime/chronicle.h"

#include "app/imgui.ext.h"

#include "endlesss/cache.stems.h"
#include "endlesss/live.stem.h"
#include "endlesss/toolkit.warehouse.h"

namespace ux {

// ---------------------------------------------------------------------------------------------------------------------
void modalJamPrecache(
    const char* title,
    JamPrecacheState& jamPrecacheState,
    const struct endlesss::toolkit::Warehouse& warehouse,
    endlesss::services::RiffFetchProvider& fetchProvider,
    tf::Executor& taskExecutor )
{
    const ImVec2 configWindowSize = ImVec2( 830.0f, 230.0f );
    ImGui::SetNextWindowContentSize( configWindowSize );

    ImGui::PushStyleColor( ImGuiCol_PopupBg, ImGui::GetStyleColorVec4( ImGuiCol_ChildBg ) );

    if ( ImGui::BeginPopupModal( title, nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize ) )
    {
        jamPrecacheState.imgui( warehouse, fetchProvider, taskExecutor );

        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
}

// ---------------------------------------------------------------------------------------------------------------------
void JamPrecacheState::imgui(
    const endlesss::toolkit::Warehouse& warehouse,
    endlesss::services::RiffFetchProvider& fetchProvider,
    tf::Executor& taskExecutor )
{
    const ImVec2 buttonSize( 240.0f, 32.0f );

    switch ( m_state )
    {
        case State::Intro:
        {
            ImGui::TextWrapped( "This tool allows you to download every stem associated with the chosen jam. This allows for playback of the jam completely offline or in the case where the Endlesss CDN is not available.\n\nDepending on the jam size, it may consume a reasonable amount of disk space and network bandwidth. The process is intentionally slower to avoid undue load on the Endlesss CDN servers.\n" );
            ImGui::Spacing();
            ImGui::TextColored( colour::shades::toast.light(), "Click below to fetch the initial stem workload from the database." );
            ImGui::SeparatorBreak();

            if ( ImGui::Button( "Fetch Stem Workload", buttonSize ) )
            {
                const bool bFoundStems = warehouse.fetchAllStemsForJam( m_jamCouchID, m_stemIDs );
                if ( bFoundStems )
                    m_state = State::Preflight;
                else
                    m_state = State::Aborted;
            }
        }
        break;

        case State::Aborted:
        {
            ImGui::TextWrapped( "No stems were found to work with, or there was an internal database error. Cannot continue" );
        }
        break;

        case State::Preflight:
        {
            ImGui::Text( "Total stems : %u", m_stemIDs.size() );
            ImGui::Spacing();
            ImGui::TextWrapped( "Note that you may already have some of these stems in your cache - they will not be re-downloaded if so.\nThe download process may take some time but can be aborted if required." );
            ImGui::Spacing();
            ImGui::TextColored( colour::shades::toast.light(), "Click below to begin the process." );
            ImGui::SeparatorBreak();

            if ( ImGui::Button( "Begin Download", buttonSize ) )
            {
                ABSL_ASSERT( m_currentStemIndex == 0 );
                m_state = State::Download;
            }
        }
        break;

        case State::Download:
        {
            if ( m_currentStemIndex >= m_stemIDs.size() )
            {
                taskExecutor.wait_for_all();
                m_state = State::Complete;
                break;
            }

            const float progressFraction = (1.0f / static_cast<float>(m_stemIDs.size()))* static_cast<float>(m_currentStemIndex);

            ImGui::ProgressBar( progressFraction, ImVec2( -1, 30.0f ), fmt::format(FMTX("{} of {}"), m_currentStemIndex + 1, m_stemIDs.size() ).c_str() );

            const endlesss::types::StemCouchID stemID = m_stemIDs[m_currentStemIndex];

            endlesss::types::Stem stemData;
            bool bFoundStemData = warehouse.fetchSingleStemByID( stemID, stemData );
            if ( bFoundStemData )
            {
                const fs::path stemCachePath = fetchProvider->getStemCache().getCachePathForStem( stemData );

                // early out if the stem already exists in the cache -- although this is checked in the stem-live code,
                // saves on allocation and work if we check it here too
                if ( fs::exists( stemCachePath / stemData.couchID.value() ) )
                {
                    m_statsStemsAlreadyInCache++;
                }
                else
                {
                    if ( m_downloadsDispatched >= 6 )
                    {
                        taskExecutor.wait_for_all();
                        m_downloadsDispatched = 0;
                    }

                    ++m_downloadsDispatched;

                    taskExecutor.silent_async( [=]()
                    {
                        spacetime::Moment downloadTimer;

                        auto stemLivePtr = std::make_shared<endlesss::live::Stem>( stemData, 8000 );   // any sample rate is fine, we aren't keeping the data
                        stemLivePtr->fetch(
                            fetchProvider->getNetConfiguration(),
                            stemCachePath );

                        if ( stemLivePtr->hasFailed() )
                        {
                            blog::error::app( FMTX( "failed to download stem to cache : [{}]" ), stemID );
                            ++ m_statsStemsFailedToDownload;
                        }
                        else
                        {
                            ++ m_statsStemsDownloaded;

                            // stash average download time; crap mutex here but eh
                            const auto downloadTimeMillis = downloadTimer.delta< std::chrono::milliseconds >();
                            {
                                std::scoped_lock<std::mutex> sl( m_averageDownloadTimeMutex );
                                m_averageDownloadTimeMillis.update( static_cast<double>(downloadTimeMillis.count()) );
                            }
                            ++ m_averageDownloadMeasurements;
                        }
                    });
                }
            }
            else
            {
                blog::error::app( FMTX( "was unable to fetch stem data from warehouse during precache : [{}]" ), stemID );
                m_statsStemsMissingFromDb++;
            }

            ImGui::Spacing();
            ImGui::Spacing();

            // do some estimation of how long the full download will take based on how long the average download is taking
            // (this assumes the whole jam requires downloading)
            if ( m_averageDownloadMeasurements < 5 )
            {
                ImGui::TextColored( colour::shades::toast.dark(), "Estimated time remaining : Calculating ..." );
            }
            else
            {
                uint32_t averageDownloadTimeMs = 0;
                {
                    std::scoped_lock<std::mutex> sl( m_averageDownloadTimeMutex );
                    averageDownloadTimeMs = static_cast<uint32_t>(m_averageDownloadTimeMillis.m_average);
                }
                std::chrono::milliseconds perDownloadMs( averageDownloadTimeMs );
                float perDownloadSecF = static_cast<float>( perDownloadMs.count() ) / 1000.0f;
                
                // multiply per-download time by the remaining stem count
                perDownloadMs *= m_stemIDs.size() - m_currentStemIndex;
                std::chrono::minutes fullDownloadMinutes = std::chrono::duration_cast<std::chrono::minutes>( perDownloadMs / 4.0f ); // divide to simulate the wider async

                ImGui::TextColored( colour::shades::toast.light(), "Estimated download time remaining : %u minutes (~%.1f seconds per stem)",
                    fullDownloadMinutes.count(),
                    perDownloadSecF
                    );
            }

            m_currentStemIndex++;
        }
        // fallthrough

        case State::Complete:
        {
            if ( m_state == State::Complete )
                ImGui::TextWrapped( "Process complete" );

            ImGui::Spacing();
            ImGui::SeparatorBreak();
            ImGui::TextColored( colour::shades::white.light(), "[%5i] Total stems in jam", static_cast<int32_t>( m_stemIDs.size() ) );
            ImGui::SeparatorBreak();
            ImGui::TextColored( colour::shades::toast.light(), "[%5i] Stems already in cache", m_statsStemsAlreadyInCache.load() );
            ImGui::TextColored( colour::shades::callout.light(), "[%5i] Stems downloaded", m_statsStemsDownloaded.load() );
            if ( m_statsStemsFailedToDownload > 0 )
                ImGui::TextColored( colour::shades::errors.light(), "[%5i] Stems failed to download", m_statsStemsFailedToDownload.load() );
            if ( m_statsStemsMissingFromDb > 0 )
                ImGui::TextColored( colour::shades::errors.light(), "[%5i] Stems missing from Warehouse", m_statsStemsMissingFromDb.load() );
        }
        break;
    }

    ImGui::SeparatorBreak();

    const auto panelRegionAvail = ImGui::GetContentRegionAvail();
    {
        ImGui::Dummy({ 0, panelRegionAvail.y - buttonSize.y - 6.0f });
    }
    ImGui::Dummy( { 0,0 } );
    ImGui::SameLine( 0, panelRegionAvail.x - buttonSize.x - 6.0f );
    if ( ImGui::Button( "Close", buttonSize ) )
    {
        taskExecutor.wait_for_all();
        ImGui::CloseCurrentPopup();
    }
}

} // namespace ux
