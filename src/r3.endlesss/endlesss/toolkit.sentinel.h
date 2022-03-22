//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "endlesss/core.types.h"

namespace endlesss {

namespace api { struct NetConfiguration; }

// ---------------------------------------------------------------------------------------------------------------------
// watching jams, watching jams, it's a jam watcher
//
// the better approach would be to use longpoll couch connections to avoid polling, but for now
// we just poke the server every N seconds and see what it's thinking; keep this high enough to not be a bother
//
struct Sentinel
{
    // callback fired when there's been a change
    using ChangeCallback = std::function<void( const types::Jam& trackedJam )>;

    Sentinel( const api::NetConfiguration& ncfg );
    ~Sentinel();

    bool startTracking( ChangeCallback cb, const types::Jam& jamToTrack );
    void stopTracking();

    inline bool isTrackerRunning() const { return m_runThread && (m_threadFailed == false); }
    inline bool isTrackerBroken() const  { return m_threadFailed; }

private:

    void checkerThreadFn();

    const api::NetConfiguration&    m_netConfig;
    types::Jam                      m_trackedJam;

    std::unique_ptr< std::thread >  m_thread;
    std::atomic_bool                m_runThread;
    std::atomic_bool                m_threadFailed;
    std::string                     m_lastSeenSequence;
    ChangeCallback                  m_callback;
    int32_t                         m_pollRateDelaySecs;
};

} // namespace endlesss
