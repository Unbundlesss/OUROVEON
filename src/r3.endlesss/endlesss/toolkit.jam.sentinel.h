//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#pragma once


#include "base/construction.h"

#include "endlesss/core.types.h"
#include "endlesss/core.services.h"
#include "endlesss/live.riff.h"

namespace endlesss {
namespace api { struct NetConfiguration; }
namespace toolkit {

// ---------------------------------------------------------------------------------------------------------------------
// watching jams, watching jams, it's a jam watcher
//
// the better approach would be to use longpoll couch connections to avoid polling, but for now
// we just poke the server every N seconds and see what it's thinking; keep this high enough to not be a bother
//
struct Sentinel
{
    DECLARE_NO_COPY( Sentinel );

    // callback fired when there's a new riff in town
    using RiffLoadCallback = std::function<void( endlesss::live::RiffPtr& riffPtr )>;

    Sentinel( const services::RiffFetchProvider& riffFetchProvider, const RiffLoadCallback& riffLoadCallback );
    ~Sentinel();

    void startTracking( const types::Jam& jamToTrack );
    void stopTracking();

    ouro_nodiscard bool isTrackerRunning() const { return m_runThread && (m_threadFailed == false); }
    ouro_nodiscard bool isTrackerBroken() const  { return m_threadFailed; }

private:

    void sentinelThreadLoop();
    void sentinelThreadFetchLatest();

    services::RiffFetchProvider         m_riffFetchProvider;
    types::Jam                          m_trackedJam;

    std::unique_ptr< std::thread >      m_thread;
    std::atomic_bool                    m_runThread;
    std::atomic_bool                    m_threadFailed;
    std::string                         m_lastSeenSequence;
    endlesss::types::RiffCouchID        m_lastFetchedRiffCouchID;
    RiffLoadCallback                    m_callback;
    int32_t                             m_pollRateDelaySecs;
};

} // namespace toolkit
} // namespace endlesss
