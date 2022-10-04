//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"

#include "endlesss/toolkit.jam.sentinel.h"
#include "endlesss/api.h"

using namespace std::chrono_literals;

namespace endlesss {
namespace toolkit {

// ---------------------------------------------------------------------------------------------------------------------
Sentinel::Sentinel( const services::RiffFetchProvider& riffFetchProvider, const RiffLoadCallback& riffLoadCallback )
    : m_riffFetchProvider( riffFetchProvider )
    , m_runThread( false )
    , m_threadFailed( false )
    , m_callback( riffLoadCallback )
    , m_pollRateDelaySecs( riffFetchProvider->getNetConfiguration().api().jamSentinelPollRateInSeconds )
{

}

Sentinel::~Sentinel()
{
    stopTracking();
}

// ---------------------------------------------------------------------------------------------------------------------
void Sentinel::startTracking( const types::Jam& jamToTrack )
{
    if ( m_runThread )
    {
        stopTracking();
    }

    blog::app( "[ SNTL ] starting jam tracker thread @ {} [{}]", jamToTrack.displayName, jamToTrack.couchID );
    blog::app( "[ SNTL ] manual polling every {} seconds", m_pollRateDelaySecs );

    m_trackedJam    = jamToTrack;

    m_runThread     = true;
    m_threadFailed  = false;
    m_thread        = std::make_unique<std::thread>( &Sentinel::sentinelThreadLoop, this );
}

// ---------------------------------------------------------------------------------------------------------------------
void Sentinel::stopTracking()
{
    if ( m_runThread )
    {
        blog::app( "[ SNTL ] halting jam tracker ..." );

        m_runThread = false;
        m_thread->join();
        m_thread = nullptr;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
void Sentinel::sentinelThreadLoop()
{
    OuroveonThreadScope ots( "JamSentinel" );

    // now we should be doing this with couch longpoll and such but ah well
    // pull the current sequence ID
    {
        endlesss::api::JamChanges jamChange;
        if ( !jamChange.fetch( m_riffFetchProvider->getNetConfiguration(), m_trackedJam.couchID ) )
        {
            m_threadFailed = true;
            return;
        }

        m_lastSeenSequence = jamChange.last_seq;
        blog::app( "[ SNTL ] captured initial change sequence" );

        // initial arrival, trigger callback
        sentinelThreadFetchLatest();
    }

    // every N seconds, fetch the riff again and trigger if we see a new sequence ID
    // note this can be a chat message or whatnot, it's just tracking the database shifting 
    while ( m_runThread )
    {
        // we chunk up the polling sleep so the thread can die more reactively
        const int32_t waitStepSubdiv = 1 + ( m_pollRateDelaySecs * 4 );
        for ( int waitSteps = 0; waitSteps < waitStepSubdiv; waitSteps++ )
        {
            std::this_thread::sleep_for( 250ms );
            if ( !m_runThread )
                return;
        }

        endlesss::api::JamChanges jamChange;
        if ( !jamChange.fetchSince( m_riffFetchProvider->getNetConfiguration(), m_trackedJam.couchID, m_lastSeenSequence ) )
        {
            blog::app( "[ SNTL ] fetchSince() failed, aborting tracker thread" );
            m_threadFailed = true;
            m_runThread = false;
            return;
        }

        const auto numberOfNewSeq = jamChange.results.size();
        const bool difference = ( numberOfNewSeq > 0 ) && 
                                ( jamChange.last_seq != m_lastSeenSequence );
        if ( difference )
        {
            blog::app( "[ SNTL ] {} change(s) detected", numberOfNewSeq );
            sentinelThreadFetchLatest();
        }

        m_lastSeenSequence = jamChange.last_seq;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
void Sentinel::sentinelThreadFetchLatest()
{
    //riffSyncInProgress = true;

    std::this_thread::sleep_for( 1000ms );

    // get the current riff from the jam
    endlesss::api::pull::LatestRiffInJam latestRiff( m_trackedJam.couchID, m_trackedJam.displayName );

    if ( !latestRiff.trySynchronousLoad( m_riffFetchProvider->getNetConfiguration() ) )
    {
        blog::error::app( "[ SYNC ] failed to read latest riff in jam" );
        //riffSyncInProgress = false;
        return;
    }

    // construct our version of the riff dataset; this we can then bring online by loading all the audio data for it
    endlesss::types::RiffComplete completeRiffData( latestRiff );


    auto riff = std::make_shared<endlesss::live::Riff>( completeRiffData );
    riff->fetch( m_riffFetchProvider );

    // check to see if the riff contents actually changed; it might have been other metadata (like chats arriving)
    const auto& syncRiffId          = completeRiffData.riff.couchID;
    const bool  isActualNewRiffData = (m_lastFetchedRiffCouchID != syncRiffId );
    m_lastFetchedRiffCouchID = syncRiffId;

    blog::app( "[ SYNC ] last id = [{}], new id = [{}] | {}", m_lastFetchedRiffCouchID, syncRiffId, isActualNewRiffData ? "is-new" : "no-new-data" );

    // if new data has arrived, tell our friends
    if ( isActualNewRiffData )
    {
        m_callback( riff );
    }

   // riffSyncInProgress = false;
}

} // namespace toolkit
} // namespace endlesss
