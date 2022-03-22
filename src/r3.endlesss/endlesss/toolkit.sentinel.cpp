//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"

#include "endlesss/toolkit.sentinel.h"
#include "endlesss/api.h"

using namespace std::chrono_literals;

namespace endlesss {

// ---------------------------------------------------------------------------------------------------------------------
Sentinel::Sentinel( const api::NetConfiguration& ncfg ) 
    : m_netConfig( ncfg )
    , m_runThread( false )
    , m_threadFailed( false )
    , m_pollRateDelaySecs( ncfg.api().jamSentinelPollRateInSeconds )
{

}

Sentinel::~Sentinel()
{
    stopTracking();
}

// ---------------------------------------------------------------------------------------------------------------------
bool Sentinel::startTracking( ChangeCallback cb, const types::Jam& jamToTrack )
{
    if ( m_runThread )
    {
        stopTracking();
    }

    blog::app( "[ SNTL ] starting jam tracker thread @ {} [{}]", jamToTrack.displayName, jamToTrack.couchID );
    blog::app( "[ SNTL ] manual polling every {} seconds", m_pollRateDelaySecs );

    m_trackedJam    = jamToTrack;

    m_callback      = cb;
    m_runThread     = true;
    m_threadFailed  = false;
    m_thread        = std::make_unique<std::thread>( &Sentinel::checkerThreadFn, this );

    return true;
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
void Sentinel::checkerThreadFn()
{
    OuroveonThreadScope ots( "JamSentinel" );

    // now we should be doing this with couch longpoll and such but ah well
    // pull the current sequence ID
    {
        endlesss::api::JamChanges jamChange;
        if ( !jamChange.fetch( m_netConfig, m_trackedJam.couchID ) )
        {
            m_threadFailed = true;
            return;
        }

        m_lastSeenSequence = jamChange.last_seq;
        blog::app( "[ SNTL ] captured initial change sequence" );

        // initial arrival, trigger callback
        m_callback( m_trackedJam );
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
        if ( !jamChange.fetchSince( m_netConfig, m_trackedJam.couchID, m_lastSeenSequence ) )
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
            m_callback( m_trackedJam );
        }

        m_lastSeenSequence = jamChange.last_seq;
    }
}

} // namespace endlesss
