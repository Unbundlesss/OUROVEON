//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"

#include "base/instrumentation.h"

#include "endlesss/toolkit.riff.pipeline.h"

#include "live.riff.cache.h"
#include "endlesss/api.h"

namespace endlesss {
namespace toolkit {

// ---------------------------------------------------------------------------------------------------------------------
Pipeline::Pipeline(
    endlesss::services::RiffFetchProvider& riffFetchProvider,
    const std::size_t liveRiffCacheSize,
    const RiffDataResolver& riffDataResolver,
    const RiffLoadCallback& riffLoadCallback,
    const QueueClearedCallback& queueClearedCallback )
    : m_riffFetchProvider( riffFetchProvider )
    , m_cacheSize( liveRiffCacheSize )
    , m_resolver( riffDataResolver )
    , m_callbackRiffLoad( riffLoadCallback )
    , m_callbackQueueCleared( queueClearedCallback )
{
    m_pipelineThreadRun = true;
    m_pipelineThread = std::make_unique<std::thread>( &Pipeline::pipelineThread, this );
}

// ---------------------------------------------------------------------------------------------------------------------
Pipeline::~Pipeline()
{
    m_pipelineThreadRun = false;
    m_pipelineThread->join();
    m_pipelineThread.reset();
}

// ---------------------------------------------------------------------------------------------------------------------
void Pipeline::requestRiff(const Request& riff)
{
    m_requests.emplace( riff );
    m_pipelineRequestSema.signal();
}

// ---------------------------------------------------------------------------------------------------------------------
void Pipeline::requestClear()
{
    m_pipelineClear = true;
    m_pipelineRequestSema.signal();
}


// ---------------------------------------------------------------------------------------------------------------------
bool Pipeline::resolveStandardRiff(
    const endlesss::api::NetConfiguration& ncfg,
    const endlesss::types::RiffIdentity& request,
          endlesss::types::RiffComplete& result )
{
    result.jam.couchID = request.getJamID();

    api::JamProfile jamProfile;
    if ( !jamProfile.fetch( ncfg, result.jam.couchID ) )
        return false;

    result.jam.displayName = jamProfile.displayName;

    api::RiffDetails riffDetails;
    if ( !riffDetails.fetch( ncfg, result.jam.couchID, request.getRiffID() ) )
        return false;

    result.riff = types::Riff( result.jam.couchID, riffDetails.rows[0].doc );

    // get all the active stem IDs from this riff
    const auto stemCouchIDs = result.riff.getActiveStemIDs();

    // .. and go fetch their metadata
    api::StemDetails stemDetails;
    if ( !stemDetails.fetchBatch( ncfg, result.jam.couchID, stemCouchIDs ) )
        return false;

    // refit the fetched stem data to the original riff data, by matching couch IDs
    const auto& allStemRows = stemDetails.rows;
    for ( size_t stemI = 0; stemI < 8; stemI++ )
    {
        if ( result.riff.stemsOn[stemI] )
        {
            bool foundMatchingStem = false;
            for ( const auto& stemRow : allStemRows )
            {
                if ( stemRow.id == result.riff.stems[stemI] )
                {
                    result.stems[stemI] = { result.jam.couchID, stemRow.doc };
                    foundMatchingStem = true;
                    break;
                }
            }
            // we expect to be able to match a fetched stem to the existing data
            // given that's where we got the IDs from that we just fetched
            ABSL_ASSERT( foundMatchingStem );
        }
        else
        {
            result.stems[stemI] = {};
        }
    }
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
bool Pipeline::resolveSharedRiff(
    const endlesss::api::NetConfiguration& ncfg,
    const endlesss::types::RiffIdentity& request,
          endlesss::types::RiffComplete& result )
{
    // use the single-shared-riff-by-id endpoint to grab everything we need about playing back this thing
    api::SharedRiffsByUser sharedRiffData;
    sharedRiffData.fetchSpecific( ncfg, endlesss::types::SharedRiffCouchID{ request.getRiffID().c_str() } );

    api::SharedRiffsByUser::Data riffData = sharedRiffData.data[0];

    result.jam.couchID = riffData.band;
    result.jam.displayName = "shared riff";

    result.riff = endlesss::types::Riff( result.jam.couchID, riffData.rifff );

    std::size_t stemIndex = 0;
    for ( const auto& loop : riffData.loops )
    {
        result.stems[stemIndex] = endlesss::types::Stem( result.jam.couchID, loop );
        stemIndex++;
    }

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
bool Pipeline::defaultNetworkResolver(
    const endlesss::api::NetConfiguration& ncfg,
    const endlesss::types::RiffIdentity& request,
          endlesss::types::RiffComplete& result )
{
    // branch to resolve shared riffs with special handling
    if ( request.getJamID() == endlesss::types::Constants::SharedRiffJam() )
    {
        return resolveSharedRiff( ncfg, request, result );
    }
    // otherwise its business as usual
    else
    {
        return resolveStandardRiff( ncfg, request, result );
    }
}

// ---------------------------------------------------------------------------------------------------------------------
void Pipeline::pipelineThread()
{
    OuroveonThreadScope ots( OURO_THREAD_PREFIX "Riff-Pipeline" );

    ABSL_ASSERT( m_cacheSize > 0 );
    endlesss::live::RiffCacheLRU liveRiffMiniCache( m_cacheSize );

    Request riffRequest;

    for (;;)
    {
        if ( !m_pipelineThreadRun )
            break;

        if ( m_pipelineRequestSema.wait( 100000 ) )
        {
            base::instr::ScopedEvent se( "riff-load", base::instr::PresetColour::Emerald );

            // if a purge was requested, drain the whole queue into the bin
            if ( m_pipelineClear )
            {
                endlesss::live::RiffPtr nullRiff;

                while ( m_requests.try_dequeue( riffRequest ) )
                {
                    // we still report that a request was "processed", just with a null result as it was skipped
                    // systems using the pipeline may need to know outflow of requests even if they weren't loaded
                    m_callbackRiffLoad( riffRequest.m_riff, nullRiff, riffRequest.m_playback );
                }

                m_pipelineClear = false;

                // ping the callback
                if ( m_callbackQueueCleared )
                    m_callbackQueueCleared();

                continue;
            }

            // pick something off the request queue
            if ( m_requests.try_dequeue( riffRequest ) )
            {
                ABSL_ASSERT( riffRequest.m_riff.hasData() );

                endlesss::live::RiffPtr riffToPlay;

                // rummage through our little local cache of live riff instances to see if we can re-use one
                if ( !liveRiffMiniCache.search( riffRequest.m_riff.getRiffID(), riffToPlay ) )
                {
                    endlesss::types::RiffComplete riffComplete;
                    if ( m_resolver( riffRequest.m_riff, riffComplete ) )
                    {
                        riffToPlay = std::make_shared< endlesss::live::Riff >( riffComplete );
                        riffToPlay->fetch( m_riffFetchProvider );

                        // stash new riff in cache
                        liveRiffMiniCache.store( riffToPlay );
                    }
                    else
                    {
                        blog::error::api( FMTX( "riff pipeline resolver failed to fetch [{}]" ), riffRequest.m_riff.getRiffID() );
                    }
                }

                m_callbackRiffLoad( riffRequest.m_riff, riffToPlay, riffRequest.m_playback );
            }
        }
        std::this_thread::yield();
    }
}

} // namespace toolkit
} // namespace endlesss
