//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"
#include "spacetime/chronicle.h"

#include "endlesss/live.riff.h"
#include "endlesss/live.stem.h"
#include "endlesss/core.constants.h"
#include "endlesss/cache.stems.h"
#include "endlesss/api.h"

using namespace date;
using namespace std::chrono;

namespace endlesss {
namespace live {

// ---------------------------------------------------------------------------------------------------------------------
endlesss::live::Riff::RiffCIDHash Riff::computeHashForRiffCID( const endlesss::types::RiffCouchID& riffCID )
{
    const auto idToHash = riffCID.c_str();
    const auto idLength = riffCID.size();
    return RiffCIDHash{ CityHash64( idToHash, idLength ) };
}

// ---------------------------------------------------------------------------------------------------------------------
Riff::Riff( const endlesss::types::RiffComplete& riffData, const int32_t targetSampleRate )
    : m_riffData( riffData )
    , m_targetSampleRate( targetSampleRate )
    , m_syncState( SyncState::Waiting )
    , m_computedRiffCouchHash( RiffCIDHash::Invalid() )
{
    m_stemPtrs.reserve( 8 );
    m_stemGains.reserve( 8 );
    m_stemTimeScales.reserve( 8 );
    m_stemRepetitions.reserve( 8 );

    // store the jam name as uppercase for UI titles
    m_uiJamUppercase = m_riffData.jam.displayName;
    std::transform( m_uiJamUppercase.begin(), m_uiJamUppercase.end(), m_uiJamUppercase.begin(),
        []( unsigned char c ) { return std::toupper( c ); } );

    m_computedRiffCouchHash = computeHashForRiffCID( m_riffData.riff.couchID );

    blog::riff( "allocated C:{} | H:{:#x}", m_riffData.riff.couchID, m_computedRiffCouchHash.getID() );
}

// ---------------------------------------------------------------------------------------------------------------------
Riff::~Riff()
{
    blog::riff( "releasing C:{} | H:{:#x}", m_riffData.riff.couchID, m_computedRiffCouchHash.getID() );

    m_stemPtrs.clear();
}

// ---------------------------------------------------------------------------------------------------------------------
void Riff::fetch( const api::NetConfiguration& ncfg, endlesss::cache::Stems& stemCache, tf::Executor& taskExecutor )
{
    m_syncState = SyncState::Working;

    {
        const auto& theRiff = m_riffData.riff;

        // work out timing basics for the riff; this may mutate as we learn more about the stems that we'll be loading
        {
            m_timingDetails.m_rcpSampleRate = 1.0 / (double)m_targetSampleRate;

            m_timingDetails.m_quarterBeats = (int32_t)(theRiff.barLength / 4);
            m_timingDetails.m_bps = theRiff.BPS;
            m_timingDetails.m_bpm = m_timingDetails.m_bps * 60.0f;

            m_timingDetails.m_lengthInSecPerBar = (1.0 / m_timingDetails.m_bps) * (double)m_timingDetails.m_quarterBeats;

            // i think this is roughly what Endlesss is doing - we have a hard limit on how long a stem can be 
            // which is 60s (i think!) and if a jam is configured with 25 BPM and 16/4, portions of the looper are 
            // disabled to lock the time below that
            double maximumSegmentsBeforeLengthIsClamped = 8.0; // 8 = number of loop repetitions if all looper segments alive
            m_timingDetails.m_lengthInSec = 60.0;
            while ( m_timingDetails.m_lengthInSec >= 60.0 )
            {
                m_timingDetails.m_lengthInSec = m_timingDetails.m_lengthInSecPerBar * maximumSegmentsBeforeLengthIsClamped;

                // if the riff length is above 60, cut the number of "active looper segments" in half and try again
                maximumSegmentsBeforeLengthIsClamped *= 0.5;
            }

            // start bar count at 8, or whatever it was whittled down to while figuring out the length
            m_timingDetails.m_barCount = (int32_t)(maximumSegmentsBeforeLengthIsClamped * 2.0); // 2x restore the 0.5 at the end of the while()


            m_timingDetails.m_lengthInSamples = (uint32_t)(m_timingDetails.m_lengthInSec * (double)m_targetSampleRate);
            m_timingDetails.m_longestStemInBars = 1;

            blog::riff( "Riff is {:.2f} BPM, {:.2f} BPS, {} / 4, probably {:2.05f}s long, {} samples, {} bars",
                m_timingDetails.m_bpm,
                m_timingDetails.m_bps,
                m_timingDetails.m_quarterBeats,
                m_timingDetails.m_lengthInSec,
                m_timingDetails.m_lengthInSamples,
                m_timingDetails.m_barCount );
        }

        blog::riff( "Fetching stems for riff [{}]", theRiff.couchID );

        tf::Taskflow stemLoadFlow;
        tf::Taskflow stemAnalysisFlow;

        std::vector< endlesss::live::Stem* > stemsWithAsyncAnalysis;

        for ( size_t stemI = 0; stemI < 8; stemI++ )
        {
            const bool  stemIsOn = theRiff.stemsOn[stemI];
            const auto& stemData = m_riffData.stems[stemI];

            if ( stemIsOn )
            {
                const StemPtr& loopStemPtr = stemCache.request( stemData );
                m_stemOwnership.push_back( loopStemPtr );

                endlesss::live::Stem* loopStemRaw = loopStemPtr.get();

                // if this was a fresh stem, enqueue it for loading via task graph
                if ( loopStemRaw->m_state == endlesss::live::Stem::State::Empty )
                {
                    stemLoadFlow.emplace( [&, loopStemRaw]()
                    {
                        loopStemRaw->fetch( ncfg, stemCache.getCachePathForStem( stemData.couchID ) );
                    });
                    stemAnalysisFlow.emplace( [loopStemRaw]()
                    {
                        loopStemRaw->fft();
                    });
                    stemsWithAsyncAnalysis.push_back( loopStemRaw );
                }
                m_stemPtrs.push_back( loopStemRaw );

                // stems can be used across riffs with changed tempos, we have to scale to cope
                const auto stemTimeScale = theRiff.BPS / stemData.BPS;
                m_stemGains.push_back( theRiff.gains[stemI] );
                m_stemTimeScales.push_back( stemTimeScale );
            }
            else
            {
                m_stemOwnership.push_back( nullptr );
                m_stemPtrs.push_back( nullptr );
                m_stemGains.push_back( 0 );
                m_stemTimeScales.push_back( 0.0f );
            }
        }

        // spread out stem loading across task system
        auto stemLoadFuture = taskExecutor.run( stemLoadFlow );
        stemLoadFuture.wait();

        // with data loaded, enqueue the post-process analysis tasks; shift ownership of the graph and return
        // a future that all stems can wait() on pre-destruction to ensure the underlying data isn't tossed before the tasks complete
        std::shared_future<void> stemSharedAnalysis( taskExecutor.run( std::move(stemAnalysisFlow) ) );
        for ( endlesss::live::Stem* rawStem : stemsWithAsyncAnalysis )
            rawStem->keepFuture( stemSharedAnalysis );

        // once stems are loaded, work out their final lengths so we can determine the shape of the riff
        for ( std::size_t stemI = 0; stemI < m_stemPtrs.size(); stemI++ )
        {
            auto* loopStem = m_stemPtrs[stemI];
            auto  stemTimeScale = m_stemTimeScales[stemI];

            if ( loopStem == nullptr )
            {
                m_stemLengthInSec.push_back( 0 );
                m_stemLengthInSamples.push_back( 0 );
                continue;
            }

            const auto timeScaledSampleCount = (uint32_t)( (double)loopStem->m_sampleCount * (1.0 / (double)stemTimeScale) );
            const auto timeScaledStemLength = (double)timeScaledSampleCount / (double)m_targetSampleRate;

            m_stemLengthInSec.push_back( (float)timeScaledStemLength );
            m_stemLengthInSamples.push_back( timeScaledSampleCount );

            // push out riff length to fit if we have resident stems that are from different time signatures
            m_timingDetails.m_lengthInSec = std::max( m_timingDetails.m_lengthInSec, timeScaledStemLength );
            m_timingDetails.m_lengthInSamples = std::max( m_timingDetails.m_lengthInSamples, timeScaledSampleCount );

            blog::riff( "Stem {} [{:.03f} timescale] [{:2.05f} sec] [{} raw samples] [{} samples]",
                stemI + 1,
                stemTimeScale,
                timeScaledStemLength,
                loopStem->m_sampleCount,
                timeScaledSampleCount );
        }

        m_timingDetails.m_barCount = (int32_t)(m_timingDetails.m_lengthInSec / m_timingDetails.m_lengthInSecPerBar);
        m_timingDetails.m_lengthInSamplesPerBar = m_timingDetails.m_lengthInSamples / m_timingDetails.m_barCount;

        blog::riff( ".. adjusted riff lengths post-load; {:2.05f}s long, {} bars, {} samples",
            m_timingDetails.m_lengthInSec,
            m_timingDetails.m_barCount,
            m_timingDetails.m_lengthInSamples );
        blog::riff( "---------------------------------------------------------------------" );


        for ( std::size_t stemI = 0; stemI < m_stemPtrs.size(); stemI++ )
        {
            auto* loopStem = m_stemPtrs[stemI];

            if ( loopStem == nullptr || 
                 loopStem->hasFailed() )
            {
                m_stemRepetitions.push_back( 0 );
                continue;
            }

            auto repeats = (int32_t)std::round( (double)m_timingDetails.m_lengthInSamples / (double)m_stemLengthInSamples[stemI] );
            if ( repeats <= 0 )
            {
                blog::riff( ".. stem [{}] calculated {} repeats, invalid?", stemI, repeats );
                repeats = 1;
            }
            m_stemRepetitions.push_back( repeats );

            m_timingDetails.m_longestStemInBars = std::max( m_timingDetails.m_longestStemInBars, m_timingDetails.m_barCount / repeats );
        }

        m_stTimestamp = spacetime::InSeconds{ std::chrono::seconds{ theRiff.creationTimeUnix } };

        // preformat some state for UI display
        m_uiTimestamp = spacetime::datestampStringFromUnix( theRiff.creationTimeUnix );

        m_uiIdentity = theRiff.user;
        m_uiDetails = fmt::format( "{} {} | {:.1f} BPM | {} / 4 | {}[]:{}x | {:.1f}s",
            endlesss::constants::cRootNames[theRiff.root],
            endlesss::constants::cScaleNames[theRiff.scale],
            m_timingDetails.m_bpm,
            m_timingDetails.m_quarterBeats,
            m_timingDetails.m_barCount,
            m_timingDetails.m_longestStemInBars,
            m_timingDetails.m_lengthInSec
        );

        m_syncState = SyncState::Success;
        return;
    }

    // #HDD todo; this fn can "only succeed", need to check if / how we return stem sync issues
    m_syncState = SyncState::Failed;
}

// ---------------------------------------------------------------------------------------------------------------------
void Riff::exportToDisk( const std::function< ssp::SampleStreamProcessorInstance( const uint32_t stemIndex, const endlesss::live::Stem& stemData ) >& diskWriterForStem )
{
    for ( auto stemI = 0; stemI < 8; stemI++ )
    {
        const float stemTimeStretch   = m_stemTimeScales[stemI];
        const float stemGain          = m_stemGains[stemI];
        endlesss::live::Stem* stemPtr = m_stemPtrs[stemI];

        if ( stemPtr == nullptr   ||
             stemPtr->hasFailed() ||
             stemGain <= 0.0f )
            continue;

        // diskWriter could be null for dry-run mode
        auto diskWriter = diskWriterForStem( stemI, *stemPtr );
        if ( diskWriter != nullptr )
        {
            const int32_t sampleCount = stemPtr->m_sampleCount;
            const int32_t sampleCountTst = (int32_t)( (double)sampleCount / (double)stemTimeStretch );

            auto exportChannelLeft  = mem::malloc16AsSet<float>( sampleCountTst, 0.0f );
            auto exportChannelRight = mem::malloc16AsSet<float>( sampleCountTst, 0.0f );

            for ( int32_t sampleWrite = 0; sampleWrite < sampleCountTst; sampleWrite++ )
            {
                const int32_t readSampleUnscaled = (int32_t)( (double)sampleWrite * (double)stemTimeStretch );

                exportChannelLeft[sampleWrite] = stemPtr->m_channel[0][readSampleUnscaled] * stemGain;
                exportChannelRight[sampleWrite] = stemPtr->m_channel[1][readSampleUnscaled] * stemGain;
            }

            // output to disk, force flush immediately
            diskWriter->appendSamples( exportChannelLeft, exportChannelRight, sampleCountTst );
            diskWriter.reset();

            mem::free16( exportChannelLeft );
            mem::free16( exportChannelRight );
        }
    }
}

} // namespace endlesss
} // namespace live
