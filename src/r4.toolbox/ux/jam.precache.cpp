//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#include "pch.h"
#include "ux/jam.precache.h"

#include "spacetime/chronicle.h"

#include "app/imgui.ext.h"

#include "endlesss/cache.stems.h"
#include "endlesss/live.stem.h"
#include "endlesss/toolkit.warehouse.h"

namespace ux {

struct JamPrecacheState
{
    static constexpr std::size_t cSyncSamples = 16;

    using Instance = std::shared_ptr<JamPrecacheState>;

    enum class State
    {
        Intro,
        Aborted,
        Preflight,
        Download,
        Complete
    };

    JamPrecacheState() = delete;
    JamPrecacheState( const endlesss::types::JamCouchID& jamID )
        : m_jamCouchID( jamID )
    {
    }

#if OURO_DEBUG
    ~JamPrecacheState()
    {
        if ( m_failureDiagnosticLog != nullptr )
        {
            fclose( m_failureDiagnosticLog );
        }
    }
#endif // OURO_DEBUG

    void imgui(
        const endlesss::toolkit::Warehouse& warehouse,
        endlesss::services::RiffFetchProvider& fetchProvider,
        tf::Executor& taskExecutor );

    endlesss::types::JamCouchID     m_jamCouchID;
    endlesss::types::StemCouchIDs   m_stemIDs;

    bool                            m_enableSiphonMode = false;
    bool                            m_enableDryRun = false;

    // debug tool that writes out all failed download URLs; for giving them to Endlesss to see if they can fix the
    // access control issues on the CDN
#if OURO_DEBUG
    bool                            m_enableFailureLog = false;
    
    std::mutex                      m_failureDiagnosticMutex;
    FILE*                           m_failureDiagnosticLog = nullptr;
#endif // OURO_DEBUG

    int32_t                         m_maximumDownloadsInFlight = OURO_THREAD_LIMIT;

    State                           m_state = State::Intro;
    std::size_t                     m_currentStemIndex = 0;

    std::size_t                     m_stemPayloadFileSizeEstimation = 0;
    std::string                     m_stemPayloadFileSizeEstimationString;

    std::atomic_uint32_t            m_statsStemsAlreadyInCache = 0;
    std::atomic_uint32_t            m_statsStemsDownloaded = 0;
    std::atomic_uint32_t            m_statsStemsMissingFromDb = 0;
    std::atomic_uint32_t            m_statsStemsFailedToDownload = 0;

    std::atomic_uint32_t            m_downloadsDispatched = 0;

    spacetime::Moment               m_syncTimer;
    base::RollingAverage< cSyncSamples >
                                    m_averageSyncTimeMillis;
    std::size_t                     m_averageSyncMeasurements = 0;
};

// ---------------------------------------------------------------------------------------------------------------------
std::shared_ptr< JamPrecacheState > createJamPrecacheState( const endlesss::types::JamCouchID& jamID )
{
    return std::make_shared< JamPrecacheState >( jamID );
}

// ---------------------------------------------------------------------------------------------------------------------
void modalJamPrecache(
    const char* title,
    JamPrecacheState& jamPrecacheState,
    const struct endlesss::toolkit::Warehouse& warehouse,
    endlesss::services::RiffFetchProvider& fetchProvider,
    tf::Executor& taskExecutor )
{
    const ImVec2 configWindowSize = ImVec2( 830.0f, 260.0f );
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
        // lay out how the tool works; when ready, do a synchronous pull of all the stem IDs to examine from Warehouse
        case State::Intro:
        {
            ImGui::TextWrapped( "This tool allows you to download every stem associated with the chosen jam. This allows for playback of the jam completely offline or in the case where the Endlesss CDN is not available.\n\nDepending on the jam size, it may consume a reasonable amount of disk space and network bandwidth.\n" );
            ImGui::Spacing();
            ImGui::Spacing();
            {
                ImGui::Checkbox( "Enable Dry-Run Mode", &m_enableDryRun );
                ImGui::SameLine();
                ImGui::AlignTextToFramePadding();
                ImGui::TextDisabled( "[?]" );
                ImGui::CompactTooltip( "If enabled, no stems will be fetched from the server.\n\nThe tool will check for stems on-disk but not issue any network requests, allowing you to quickly see how full the cache is for this jam already" );
            }
#if OURO_DEBUG
            {
                ImGui::Checkbox( "Enable Failure Log", &m_enableFailureLog );
                ImGui::SameLine();
                ImGui::AlignTextToFramePadding();
                ImGui::TextDisabled( "[?]" );
                ImGui::CompactTooltip( "If enabled, all stems that fail to download will be recorded in a diagnostic log file for future analysis" );
            }
#endif // OURO_DEBUG
            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();

            if ( m_enableSiphonMode )
            {
                ImGui::TextColored( colour::shades::pink.light(), "Database siphon mode enabled. Complete stem manifest will be used." );
            }
            else
            {
                ImGui::TextColored( colour::shades::toast.light(), "Click below to fetch the initial stem workload from the database." );
            }
            if ( ImGui::IsMouseDoubleClicked( 1 ) )
            {
                m_enableSiphonMode = true;
            }
            ImGui::SeparatorBreak();

            if ( ImGui::Button( "Fetch Stem Workload", buttonSize ) )
            {
                bool bFoundStems = false;
                if ( m_enableSiphonMode )
                {
                    bFoundStems = warehouse.fetchAllStems( m_stemIDs, m_stemPayloadFileSizeEstimation );
                }
                else
                {
                    bFoundStems = warehouse.fetchAllStemsForJam( m_jamCouchID, m_stemIDs, m_stemPayloadFileSizeEstimation );
                }
                m_stemPayloadFileSizeEstimationString = base::humaniseByteSize( "", m_stemPayloadFileSizeEstimation );

                if ( bFoundStems )
                    m_state = State::Preflight;
                else
                    m_state = State::Aborted;
            }
        }
        break;

        // .. found no stems to fetch, bail out
        case State::Aborted:
        {
            ImGui::TextWrapped( "No stems were found to work with, or there was an internal database error. Cannot continue" );
        }
        break;

        // present the work to do, allow tuning of download limits
        case State::Preflight:
        {
            ImGui::TextColored( colour::shades::callout.light(),   "        Total stems : %u", static_cast<uint32_t>( m_stemIDs.size() ) );
            ImGui::TextColored( colour::shades::callout.neutral(), "Disk space estimate : %s", m_stemPayloadFileSizeEstimationString.c_str() );
            ImGui::Spacing();
            ImGui::TextWrapped( "Note that you may already have some of these stems in your cache - they will not be re-downloaded. Abort the download at any point by clicking [Close] - it may pause briefly to finish all active downloads." );
            ImGui::Spacing();
            ImGui::SeparatorBreak();
            {
                // allow choice of how many download tasks are kept in-flight; more tasks means larger network load / strain on endlesss
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted( "Maximum Simultaneous Downloads : " );
                ImGui::SameLine();
                ImGui::SetNextItemWidth( 200.0f );
                ImGui::SliderInt( "##simd", &m_maximumDownloadsInFlight, 1, OURO_THREAD_LIMIT );
            }
            ImGui::Spacing();
            if ( ImGui::Button( "Begin Download", buttonSize ) )
            {
                ABSL_ASSERT( m_currentStemIndex == 0 );

#if OURO_DEBUG
                if ( m_enableFailureLog )
                {
                    m_failureDiagnosticLog = fopen( "precache_failures.txt", "wt" );
                }
#endif // OURO_DEBUG

                m_syncTimer.setToNow();
                m_state = State::Download;
            }
        }
        break;

        // do the actual work; this runs through and triggers more download tasks if we don't have enough in-flight
        case State::Download:
        {
            const auto stemCount = m_stemIDs.size();

            if ( m_currentStemIndex >= stemCount )
            {
                taskExecutor.wait_for_all();
                m_state = State::Complete;
                break;
            }

            // show progress
            {
                const float progressFraction = (1.0f / static_cast<float>(stemCount)) * static_cast<float>(m_currentStemIndex);
                ImGui::ProgressBar( progressFraction, ImVec2( -1, 26.0f ), fmt::format( FMTX( "{} of {}" ), m_currentStemIndex + 1, m_stemIDs.size() ).c_str() );
            }

            // how many fs::exists() checks we can do in a sequence, otherwise we would do one per frame and 
            // that means iterating through larger jams that are already on-disk would be a chore
            int32_t fileOpsBurstCount = 100;

            // if there are not enough live tasks running, kick some off
            while ( m_downloadsDispatched < static_cast<uint32_t>(m_maximumDownloadsInFlight) && fileOpsBurstCount > 0 )
            {
                const endlesss::types::StemCouchID stemID = m_stemIDs[ ( stemCount - 1 ) - m_currentStemIndex ];

                // pull the full stem data we have on file
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
                        fileOpsBurstCount--;
                    }
                    else
                    {
                        if ( !m_enableDryRun )
                        {
                            ++m_downloadsDispatched;
                            fileOpsBurstCount = -1;

                            // keep timer on how long it takes to cycle through to dispatching new tasks; this gives
                            // a rough idea of how long the whole process will take
                            {
                                const auto downloadTimeMillis = m_syncTimer.delta< std::chrono::milliseconds >();
                                m_averageSyncTimeMillis.update( static_cast<double>(downloadTimeMillis.count()) );
                                m_syncTimer.setToNow();
                                ++m_averageSyncMeasurements;
                            }

                            // kick out an untethered async task to initialise a live Stem object
                            // doing so will go through the default machinery of downloading / validating it, same as
                            // when we do this for playing riffs back in the rest of the app - difference being that 
                            // we don't keep the live Stem around, it's just immediately tossed
                            taskExecutor.silent_async( [=, this]()
                                {
                                    auto stemLivePtr = std::make_shared<endlesss::live::Stem>( stemData, 8000 );   // any sample rate is fine, we aren't keeping the data
                                    stemLivePtr->fetch(
                                        fetchProvider->getNetConfiguration(),
                                        stemCachePath );

                                    if ( stemLivePtr->hasFailed() )
                                    {
#if OURO_DEBUG
                                        if ( m_enableFailureLog )
                                        {
                                            const auto failedEndpoint = stemData.fullEndpoint();
                                            fprintf( m_failureDiagnosticLog, "%s/%s\n", failedEndpoint.c_str(), stemData.fileKey.c_str() );
                                            fflush( m_failureDiagnosticLog );
                                        }
#endif // OURO_DEBUG
                                        blog::error::app( FMTX( "failed to download stem to cache : [{}]" ), stemID );
                                        ++m_statsStemsFailedToDownload;
                                    }
                                    else
                                    {
                                        ++m_statsStemsDownloaded;
                                    }

                                    // tag that we are done with this task so that the cycle can kick more off
                                    --m_downloadsDispatched;
                                });
                        }
                    }
                }
                else
                {
                    blog::error::app( FMTX( "was unable to fetch stem data from warehouse during precache : [{}]" ), stemID );
                    ++m_statsStemsMissingFromDb;
                }

                // we've done something with this one, next stem in the list...
                ++m_currentStemIndex;
                // break out of the burst loop if we're out of stems, it will flush the workers on the next loop round
                if ( m_currentStemIndex >= m_stemIDs.size() )
                    break;
            }

            ImGui::Spacing();
            ImGui::Spacing();

            // do some estimation of how long the full download will take based on how long the average download is taking
            // based on how long each stem has taken so far
            if ( m_averageSyncMeasurements < cSyncSamples * 2 )
            {
                ImGui::TextColored( colour::shades::toast.dark(), "Estimated time remaining : Calculating ..." );
            }
            else
            {
                const uint32_t averageSyncTimeMs = static_cast<uint32_t>(m_averageSyncTimeMillis.m_average);
                std::chrono::milliseconds perSyncMs( averageSyncTimeMs );
                float perSyncSecF = static_cast<float>( perSyncMs.count() ) / 1000.0f;
                
                // multiply per-sync time by the remaining stem count
                perSyncMs *= m_stemIDs.size() - m_currentStemIndex;
                std::chrono::minutes fullSyncMinutes = std::chrono::duration_cast<std::chrono::minutes>( perSyncMs );

                ImGui::TextColored( colour::shades::toast.light(), "Estimated time remaining : ~%u minute(s) (~%.1f seconds per stem)",
                    static_cast<uint32_t>( fullSyncMinutes.count() ),
                    perSyncSecF
                    );
            }
        }
        // fallthrough: always show the current stats as we are working

        case State::Complete:
        {
            if ( m_state == State::Complete )
            {
                ImGui::TextColored( colour::shades::green.light(), "Process complete" );
            }

            // work out percentages for how many stems are found on disk and how many have come down over the wire
            const auto stemsInCache         = m_statsStemsAlreadyInCache.load();
            const auto stemsDownloaded      = m_statsStemsDownloaded.load();
            const double totalStemsRecpPct  = 100.0 / static_cast<double>( m_stemIDs.size() );
            const double stemsInCachePct    = totalStemsRecpPct * static_cast<double>(stemsInCache);
            const double downloadedPct      = totalStemsRecpPct * static_cast<double>(stemsDownloaded);

            // display stats
            ImGui::Spacing();
            ImGui::SeparatorBreak();
            ImGui::TextColored( colour::shades::white.light(), "[ %6i ] Total stems in jam", static_cast<int32_t>( m_stemIDs.size() ) );
            ImGui::SeparatorBreak();
            ImGui::TextColored( colour::shades::toast.light(), "[ %6i ] Stems already in cache (%.1f%%)", stemsInCache, stemsInCachePct );
            ImGui::TextColored( colour::shades::callout.light(), "[ %6i ] Stems downloaded (%.1f%%)", stemsDownloaded, downloadedPct );
            if ( m_statsStemsFailedToDownload > 0 )
                ImGui::TextColored( colour::shades::errors.light(), "[ %6i ] Stems failed to download", m_statsStemsFailedToDownload.load() );
            if ( m_statsStemsMissingFromDb > 0 )
                ImGui::TextColored( colour::shades::errors.light(), "[ %6i ] Stems missing from Warehouse", m_statsStemsMissingFromDb.load() );
        }
        break;
    }

    ImGui::SeparatorBreak();

    if ( ImGui::BottomRightAlignedButton( "Close", buttonSize ) )
    {
        taskExecutor.wait_for_all();
        ImGui::CloseCurrentPopup();
    }
}

} // namespace ux
