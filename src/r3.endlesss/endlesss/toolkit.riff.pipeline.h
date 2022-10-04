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
#include "endlesss/live.riff.h"

namespace endlesss {
namespace toolkit {

// ---------------------------------------------------------------------------------------------------------------------
// 
//
struct Pipeline
{
    struct Request
    {
        Request() = default;
        Request( const endlesss::types::RiffIdentity& riff )
            : m_riff( riff )
        {}
        Request( const endlesss::types::RiffIdentity& riff, const endlesss::types::RiffPlaybackPermutation& permutation )
            : m_riff( riff )
            , m_playback( permutation )
        {}
        Request( const endlesss::types::RiffIdentity& riff, const endlesss::types::RiffPlaybackPermutationOpt& permutationOpt )
            : m_riff( riff )
            , m_playback( permutationOpt )
        {}

        endlesss::types::RiffIdentity                   m_riff;         // what to play
        endlesss::types::RiffPlaybackPermutationOpt     m_playback;     // how to play, optional
    };

    using RiffDataResolver      = std::function<bool( const endlesss::types::RiffIdentity&, endlesss::types::RiffComplete& )>;
    using RiffLoadCallback      = std::function<void( const endlesss::types::RiffIdentity&, endlesss::live::RiffPtr&, const endlesss::types::RiffPlaybackPermutationOpt& )>;
    using QueueClearedCallback  = std::function<void()>;


    Pipeline(
        endlesss::services::RiffFetchProvider&  riffFetchProvider,          // api required for riff fetching / caching
        const std::size_t                       liveRiffCacheSize,          // number of live riffs to hold in the local pipeline cache
        const RiffDataResolver&                 riffDataResolver,           // resolver function that can process a request into riff data
        const RiffLoadCallback&                 riffLoadCallback,           // callback for when a request is processed (successfully or not)
        const QueueClearedCallback&             queueClearedCallback );     // callback for when a clear-queue has happened

    ~Pipeline();


    // add a new riff request to the pipeline
    void requestRiff( const Request& request );

    // request to purge all currently enqueued pipeline requests
    void requestClear();


    // given a jam+riff, this resolver will fetch all the metadata from the endlesss backend from scratch
    static bool defaultNetworkResolver(
        const endlesss::api::NetConfiguration& ncfg,
        const endlesss::types::RiffIdentity& request,
        endlesss::types::RiffComplete& result );

private:

    void pipelineThread();

    using RiffIDQueue = mcc::ReaderWriterQueue< Request >;

    services::RiffFetchProvider     m_riffFetchProvider;

    RiffIDQueue                     m_requests;         // riffs to fetch & play - written to by main thread, read from worker

    std::size_t                     m_cacheSize = 0;
    RiffDataResolver                m_resolver;
    RiffLoadCallback                m_callbackRiffLoad;
    QueueClearedCallback            m_callbackQueueCleared;

    std::unique_ptr< std::thread >  m_pipelineThread;
    std::atomic_bool                m_pipelineThreadRun = false;
    mcc::LightweightSemaphore       m_pipelineRequestSema;

    std::atomic_bool                m_pipelineClear = false;
};

} // namespace toolkit
} // namespace endlesss
