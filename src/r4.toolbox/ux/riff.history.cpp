//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#include "pch.h"

#include "base/paging.h"
#include "base/eventbus.h"
#include "base/text.h"

#include "app/imgui.ext.h"
#include "app/module.frontend.fonts.h"
#include "colour/preset.h"

#include "endlesss/core.types.h"

#include "mix/common.h"

#include "ux/riff.history.h"


namespace ux {

struct HistoryRecords
{
    static constexpr std::size_t cMaximumHistorySize = 256;

    std::size_t     m_recordsUsed = 0;

    std::array< endlesss::types::JamCouchID,    cMaximumHistorySize >   m_jamCouchIDs;
    std::array< endlesss::types::RiffCouchID,   cMaximumHistorySize >   m_riffCouchIDs;
    std::array< spacetime::InSeconds,           cMaximumHistorySize >   m_timestamps;
};

// ---------------------------------------------------------------------------------------------------------------------
struct RiffHistory::State
{
    State( base::EventBusClient eventBus )
        : m_eventBusClient( std::move( eventBus ) )
    {
        blog::app( FMTX( "RiffHistory memory block : {}" ), base::humaniseByteSize( "", sizeof(HistoryRecords) ) );

        m_enqueuedRiffIDs.reserve( 64 );

        APP_EVENT_BIND_TO( MixerRiffChange );
    }

    ~State()
    {
        APP_EVENT_UNBIND( MixerRiffChange );
    }

    void event_MixerRiffChange( const events::MixerRiffChange* eventData );


    void imgui( app::CoreGUI& coreGUI );


    base::EventBusClient            m_eventBusClient;

    base::EventListenerID           m_eventLID_MixerRiffChange = base::EventListenerID::invalid();

    bool                            m_isRecording = true;
    HistoryRecords                  m_historyRecords;

    endlesss::types::RiffCouchID    m_currentlyPlayingRiffID;

    endlesss::types::RiffCouchIDs   m_enqueuedRiffIDs;
    endlesss::types::RiffCouchIDSet m_enqueuedRiffIDSet;

    std::vector< std::size_t >      m_replayIndices;
};


// ---------------------------------------------------------------------------------------------------------------------
void RiffHistory::State::event_MixerRiffChange( const events::MixerRiffChange* eventData )
{
    // might be a empty riff, only track actual riffs
    if ( eventData->m_riff != nullptr )
    {
        const auto changedToRiffID = eventData->m_riff->m_riffData.riff.couchID;
        m_currentlyPlayingRiffID = changedToRiffID;

        // handle shared_riff magic tagging
        auto changedToJamID  = eventData->m_riff->m_riffData.jam.couchID;
        if ( eventData->m_riff->m_riffData.jam.displayName.starts_with( endlesss::types::Constants::SharedRiffJam() ) )
        {
            changedToJamID = endlesss::types::Constants::SharedRiffJam();
        }
        ABSL_ASSERT( !changedToJamID.empty() );
        
        // check if we just sent this riff to play, in which case don't re-add it to the history list
        // remove one entry from the list, we may have multiple of the same queued 
        auto enqIt = std::find( m_enqueuedRiffIDs.begin(), m_enqueuedRiffIDs.end(), changedToRiffID );
        if ( enqIt != m_enqueuedRiffIDs.end() )
        {
            std::iter_swap( enqIt, m_enqueuedRiffIDs.end() - 1 );
            m_enqueuedRiffIDs.erase( m_enqueuedRiffIDs.end() - 1 );
            return;
        }

        // ignore doing anything else if we're not recording
        if ( !m_isRecording )
            return;

        // ignore if the cancellation flag was set, mixer didn't actually play this
        if ( eventData->m_wasCancelled )
            return;

        const std::size_t writeIndex = m_historyRecords.m_recordsUsed % HistoryRecords::cMaximumHistorySize;

        m_historyRecords.m_jamCouchIDs[writeIndex]  = changedToJamID;
        m_historyRecords.m_riffCouchIDs[writeIndex] = changedToRiffID;
        m_historyRecords.m_timestamps[writeIndex]   = spacetime::InSeconds{ spacetime::getUnixTimeNow() };

        m_historyRecords.m_recordsUsed++;
    }
    else
    {
        // nothing playing
        m_currentlyPlayingRiffID = {};
    }
}

// ---------------------------------------------------------------------------------------------------------------------
void RiffHistory::State::imgui( app::CoreGUI& coreGUI )
{
    // management window
    if ( ImGui::Begin( ICON_FA_CLOCK_ROTATE_LEFT " Playback History###playback_history" ) )
    {
        const ImVec2 toolButtonSize( 24.0f, 24.0f );

        if ( m_isRecording )
        {
            if ( ImGui::Button( " " ICON_FA_PAUSE  " Pause Recording  " ) )
                m_isRecording = false;
        }
        else
        {
            if ( ImGui::Button( " " ICON_FA_CIRCLE " Resume Recording " ) )
                m_isRecording = true;
        }
        ImGui::SameLine();
        if ( ImGui::Button( " Clear " ) )
        {
            m_historyRecords = {};
        }
        ImGui::SameLine();
        bool bReRunSequenceOnIteration = false;
        if ( ImGui::Button( fmt::format( FMTX( " Replay ({:03}) " ), std::min( m_historyRecords.m_recordsUsed, HistoryRecords::cMaximumHistorySize ) ).c_str() ) )
        {
            bReRunSequenceOnIteration = true;
            m_replayIndices.clear();
        }
        ImGui::CompactTooltip( "From oldest to newest, re-enqueue all riffs in the playback recorder\nNB. you will need transition timing on for this to make any sense!" );



        ImGui::SeparatorBreak();

        const std::size_t recordsToIterate = std::min( m_historyRecords.m_recordsUsed, HistoryRecords::cMaximumHistorySize );
        std::size_t readIndex = m_historyRecords.m_recordsUsed % HistoryRecords::cMaximumHistorySize;

        const auto decrementReadIndex = [&readIndex]()
            {
                if ( readIndex == 0 )
                    readIndex = HistoryRecords::cMaximumHistorySize - 1;
                else
                    readIndex--;
            };
        decrementReadIndex();

        // hold onto the original read point in case we need it for the delete phase later
        const std::size_t originalReadIndex = readIndex;

        // cache local set of enqueued IDs
        {
            m_enqueuedRiffIDSet.clear();
            for ( const auto& enqueuedID : m_enqueuedRiffIDs )
                m_enqueuedRiffIDSet.emplace( enqueuedID );
        }

        std::size_t entryToDelete = 0;
        for ( std::size_t rI = 0; rI < recordsToIterate; rI++ )
        {
            const auto& currentJamID    = m_historyRecords.m_jamCouchIDs[readIndex];
            const auto& currentRiffID   = m_historyRecords.m_riffCouchIDs[readIndex];

            const bool bIsPlaying       = m_currentlyPlayingRiffID == currentRiffID;
            const bool bRiffWasEnqueued = m_enqueuedRiffIDSet.contains( currentRiffID );

            if ( bReRunSequenceOnIteration )
            {
                m_replayIndices.emplace_back( rI );
            }

            ImGui::PushID( (int32_t)rI );
            if ( ImGui::Button( ICON_FA_CIRCLE_XMARK ) )
            {
                entryToDelete = readIndex + 1;
            }
            ImGui::SameLine( 0, 2.0f );
            {
                ImGui::Scoped::Disabled disabledButton( bRiffWasEnqueued );
                ImGui::Scoped::ToggleButton highlightButton( bIsPlaying, true );
                if ( ImGui::Button( currentRiffID.value().c_str() ) )
                {
                    // don't re-enqueue if this riff is already in the enqueued-list, nor if it's already playing
                    if ( bRiffWasEnqueued == false && bIsPlaying == false )
                    {
                        // ask for this riff to play but stash the ID so we can ignore it when it immediately arrives in event_MixerRiffChange
                        m_enqueuedRiffIDs.emplace_back( currentRiffID );
                        m_eventBusClient.Send< ::events::EnqueueRiffPlayback >( currentJamID, currentRiffID );
                    }
                }
            }
            ImGui::SameLine();
            ImGui::TextUnformatted( fmt::format( FMTX( "{}" ), m_historyRecords.m_timestamps[readIndex] ).c_str() );
            ImGui::PopID();

            decrementReadIndex();
        }

        // handle the replay sequence request - enqueue riffs in reverse order
        if ( bReRunSequenceOnIteration )
        {
            for ( auto rIt = m_replayIndices.begin(); rIt != m_replayIndices.end(); ++rIt )
            {
                const std::size_t replayIndex = *rIt;

                const auto& currentJamID    = m_historyRecords.m_jamCouchIDs[replayIndex];
                const auto& currentRiffID   = m_historyRecords.m_riffCouchIDs[replayIndex];

                m_enqueuedRiffIDs.emplace_back( currentRiffID );
                m_eventBusClient.Send< ::events::EnqueueRiffPlayback >( currentJamID, currentRiffID );
            }
        }

        // check if something needs removing
        if ( entryToDelete != 0 )
        {
            entryToDelete--;

            // 0  1  2  3  4  5  6  7
            // i  j  c  d  e  f  g  h
            //       ^w
            //    ^r
            //                ^x <  <
            // <  <

            // 0  1  2  3  4  5  6  7
            // a  b  c  d  -  -  -  -
            //             ^w
            //          ^r
            //    ^x

            std::size_t overwriteIndex = entryToDelete;
            std::size_t copyFromIndex = overwriteIndex + 1;

            // delete the initial deletion index, we might not be overwriting it if it's at the most-recent edge
            m_historyRecords.m_jamCouchIDs[overwriteIndex]  = {};
            m_historyRecords.m_riffCouchIDs[overwriteIndex] = {};
            m_historyRecords.m_timestamps[overwriteIndex]   = {};

            while ( overwriteIndex != originalReadIndex )
            {
                const std::size_t copyFromIndexWrapped = copyFromIndex % HistoryRecords::cMaximumHistorySize;

                m_historyRecords.m_jamCouchIDs[overwriteIndex]  = m_historyRecords.m_jamCouchIDs[copyFromIndexWrapped];
                m_historyRecords.m_riffCouchIDs[overwriteIndex] = m_historyRecords.m_riffCouchIDs[copyFromIndexWrapped];
                m_historyRecords.m_timestamps[overwriteIndex]   = m_historyRecords.m_timestamps[copyFromIndexWrapped];

                overwriteIndex = ( overwriteIndex + 1 ) % HistoryRecords::cMaximumHistorySize;
                copyFromIndex = overwriteIndex + 1;
            }
            m_historyRecords.m_recordsUsed--;
        }
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------------------------------------------------
RiffHistory::RiffHistory( base::EventBusClient eventBus )
    : m_state( std::make_unique<State>( std::move( eventBus ) ) )
{
}

// ---------------------------------------------------------------------------------------------------------------------
RiffHistory::~RiffHistory()
{
}

// ---------------------------------------------------------------------------------------------------------------------
void RiffHistory::imgui( app::CoreGUI& coreGUI )
{
    m_state->imgui( coreGUI );
}

} // namespace ux
