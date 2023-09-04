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
    return RiffCIDHash{ absl::Hash< endlesss::types::RiffCouchID >{}(riffCID) };
}

// ---------------------------------------------------------------------------------------------------------------------
Riff::Riff( const endlesss::types::RiffComplete& riffData )
    : m_riffData( riffData )
    , m_syncState( SyncState::Waiting )
    , m_computedRiffCouchHash( RiffCIDHash::Invalid() )
{
    m_stemSampleRate = 0;

    m_stemOwnership.fill( nullptr );
    m_stemPtrs.fill( nullptr );
    m_stemGains.fill( 0 );
    m_stemLengthInSec.fill( 0 );
    m_stemTimeScales.fill( 0 );
    m_stemRepetitions.fill( 0 );
    m_stemLengthInSamples.fill( 0 );

    // store the jam name as uppercase for UI titles
    m_uiJamUppercase = m_riffData.jam.displayName;
    std::transform( m_uiJamUppercase.begin(), m_uiJamUppercase.end(), m_uiJamUppercase.begin(),
        []( unsigned char c ) { return std::toupper( c ); } );

    m_computedRiffCouchHash = computeHashForRiffCID( m_riffData.riff.couchID );

    blog::riff( FMTX( "[R:{}] allocated | H:{:#x}" ), m_riffData.riff.couchID, m_computedRiffCouchHash.getID() );
}

// ---------------------------------------------------------------------------------------------------------------------
Riff::~Riff()
{
    blog::riff( FMTX( "[R:{}] released | H:{:#x}" ), m_riffData.riff.couchID, m_computedRiffCouchHash.getID() );

    for ( auto& sP : m_stemOwnership )
        sP.reset();

    m_stemPtrs.fill( nullptr );
}

// ---------------------------------------------------------------------------------------------------------------------
void Riff::fetch( services::RiffFetchProvider& services )
{
    m_syncState = SyncState::Working;
    
    // take a short ID snippet to use as a more readable tag in the log in front of everything related to this riff
    const std::string riffCouchSnip = m_riffData.riff.couchID.substr( 8 );

    const auto& stemProcessing = services->getStemCache().getStemProcessing();

    m_stemSampleRate = services->getSampleRate();
    const double targetSampleRateD = (double)m_stemSampleRate;

    {
        const auto& theRiff = m_riffData.riff;

        // work out timing basics for the riff; this may mutate as we learn more about the stems that we'll be loading
        {
            m_timingDetails.m_rcpSampleRate = 1.0 / targetSampleRateD;

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


            m_timingDetails.m_lengthInSamples = (uint32_t)(m_timingDetails.m_lengthInSec * targetSampleRateD);
            m_timingDetails.m_longestStemInBars = 1;


//             float beatLength = 60.0f / m_timingDetails.m_bpm;
//             float beat32nd   = beatLength / 8.0f;
//             float total32nds = m_timingDetails.m_lengthInSec / beat32nd;
// 
//             auto samplesPer32nd = m_timingDetails.m_lengthInSamples / (int32_t)total32nds;

            blog::riff( FMTX( "[R:{}..] {:.2f} BPM, {:.2f} BPS, {} / 4, probably {:2.05f}s long, {} samples, {} bars" ),
                riffCouchSnip,
                m_timingDetails.m_bpm,
                m_timingDetails.m_bps,
                m_timingDetails.m_quarterBeats,
                m_timingDetails.m_lengthInSec,
                m_timingDetails.m_lengthInSamples,
                m_timingDetails.m_barCount );
        }

        blog::riff( FMTX( "[R:{}..] riff stem resolution ..." ), riffCouchSnip );

        tf::Taskflow stemLoadFlow;
        tf::Taskflow stemAnalysisFlow;

        std::vector< endlesss::live::Stem* > stemsWithAsyncAnalysis;

        for ( size_t stemI = 0; stemI < 8; stemI++ )
        {
            const bool  stemIsOn = theRiff.stemsOn[stemI];
            const auto& stemData = m_riffData.stems[stemI];

            if ( stemIsOn )
            {
                const StemPtr& loopStemPtr = services->getStemCache().request( stemData );
                m_stemOwnership[stemI] = loopStemPtr;

                endlesss::live::Stem* loopStemRaw = loopStemPtr.get();

                // if this was a fresh stem, enqueue it for loading via task graph
                if ( loopStemRaw->m_state == endlesss::live::Stem::State::Empty )
                {
                    stemLoadFlow.emplace( [&stemData, &services, loopStemRaw]()
                    {
                        loopStemRaw->fetch( services->getNetConfiguration(), services->getStemCache().getCachePathForStem( stemData ) );
                    });
                    stemAnalysisFlow.emplace( [&stemProcessing, loopStemRaw]()
                    {
                        loopStemRaw->analyse( stemProcessing );
                    });
                    stemsWithAsyncAnalysis.push_back( loopStemRaw );
                }
                m_stemPtrs[stemI] = loopStemRaw;

                // stems can be used across riffs with changed tempos, we have to scale to cope
                const auto stemTimeScale = theRiff.BPS / stemData.BPS;
                m_stemGains[stemI] = theRiff.gains[stemI];
                m_stemTimeScales[stemI] = stemTimeScale;
            }
        }

        // spread out stem loading across task system
        auto stemLoadFuture = services->getTaskExecutor().run( stemLoadFlow );
        stemLoadFuture.wait();

        // with data loaded, enqueue the post-process analysis tasks; shift ownership of the graph and return
        // a future that all stems can wait() on pre-destruction to ensure the underlying data isn't tossed before the tasks complete
        std::shared_future<void> stemSharedAnalysis( services->getTaskExecutor().run( std::move(stemAnalysisFlow) ) );
        for ( endlesss::live::Stem* rawStem : stemsWithAsyncAnalysis )
            rawStem->keepFuture( stemSharedAnalysis );

        // once stems are loaded, work out their final lengths so we can determine the shape of the riff
        for ( std::size_t stemI = 0; stemI < m_stemPtrs.size(); stemI++ )
        {
            auto* loopStem = m_stemPtrs[stemI];
            auto  stemTimeScale = m_stemTimeScales[stemI];

            if ( loopStem == nullptr )
            {
                continue;
            }

            const auto timeScaledSampleCount = (uint32_t)( (double)loopStem->m_sampleCount * (1.0 / (double)stemTimeScale) );
            const auto timeScaledStemLength  = (double)timeScaledSampleCount / targetSampleRateD;

            m_stemLengthInSec[stemI]     = (float)timeScaledStemLength;
            m_stemLengthInSamples[stemI] = timeScaledSampleCount;

            // push out riff length to fit if we have resident stems that are from different time signatures
            m_timingDetails.m_lengthInSec     = std::max( m_timingDetails.m_lengthInSec,     timeScaledStemLength  );
            m_timingDetails.m_lengthInSamples = std::max( m_timingDetails.m_lengthInSamples, timeScaledSampleCount );

            blog::riff( FMTX( "[R:{}..] stem {} [{:>6.03f} timescale] [{:>10.05f} sec] [{:>10} raw samples] [{:>10} samples]" ),
                riffCouchSnip,
                stemI + 1,
                stemTimeScale,
                timeScaledStemLength,
                loopStem->m_sampleCount,
                timeScaledSampleCount );
        }

        m_timingDetails.m_barCount = (int32_t)(m_timingDetails.m_lengthInSec / m_timingDetails.m_lengthInSecPerBar);
        m_timingDetails.m_lengthInSamplesPerBar = m_timingDetails.m_lengthInSamples / m_timingDetails.m_barCount;

        blog::riff( FMTX( "[R:{}..] adjusted riff lengths post-load; {:2.05f}s long, {} bars, {} samples" ),
            riffCouchSnip,
            m_timingDetails.m_lengthInSec,
            m_timingDetails.m_barCount,
            m_timingDetails.m_lengthInSamples );
        blog::riff( "---------------------------------------------------------------------------------" );


        // compute the longest stem length
        for ( std::size_t stemI = 0; stemI < m_stemPtrs.size(); stemI++ )
        {
            auto* loopStem = m_stemPtrs[stemI];

            if ( loopStem == nullptr || 
                 loopStem->hasFailed() )
            {
                continue;
            }

            auto repeats = (int32_t)std::round( (double)m_timingDetails.m_lengthInSamples / (double)m_stemLengthInSamples[stemI] );
            if ( repeats <= 0 )
            {
                blog::riff( FMTX( "[R:{}..] .. stem [{}] calculated {} repeats, invalid?" ), riffCouchSnip, stemI, repeats );
                repeats = 1;
            }
            m_stemRepetitions[stemI] = repeats;

            m_timingDetails.m_longestStemInBars = std::max( m_timingDetails.m_longestStemInBars, m_timingDetails.m_barCount / repeats );
        }

        m_stTimestamp = spacetime::InSeconds{ std::chrono::seconds{ theRiff.creationTimeUnix } };

        // preformat some state for UI display
        m_uiTimestamp = spacetime::datestampStringFromUnix( theRiff.creationTimeUnix );

        m_uiDetails = fmt::format( FMTX( "{} | {} {} | {:.1f} BPM | {} / 4 | {}" ),
            theRiff.user,
            endlesss::constants::cRootNames[theRiff.root],
            endlesss::constants::cScaleNames[theRiff.scale],
            m_timingDetails.m_bpm,
            m_timingDetails.m_quarterBeats,
            theRiff.couchID.substr(4)
        );

        m_uiPlaybackDebug = fmt::format( FMTX( "| {}[]:{}x | {:.1f}s" ),
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
void Riff::exportToDisk( const streamProcessorFactoryFn& diskWriterForStem, const int32_t sampleOffset )
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
            const int32_t sampleCountTimeScaled = (int32_t)( (double)sampleCount / (double)stemTimeStretch );

            auto exportChannelLeft  = mem::alloc16To<float>( sampleCountTimeScaled, 0.0f );
            auto exportChannelRight = mem::alloc16To<float>( sampleCountTimeScaled, 0.0f );

            const int32_t sampleOffsetTimeScaled = (int32_t)( (double)sampleOffset * (double)stemTimeStretch );

            for ( int32_t sampleWrite = 0; sampleWrite < sampleCountTimeScaled; sampleWrite++ )
            {
                const int32_t readSampleTimeScaled           = (int32_t)( (double)sampleWrite * (double)stemTimeStretch );
                const int32_t readSampleTimeScaledWithOffset = ( readSampleTimeScaled + sampleOffsetTimeScaled ) % sampleCount;

                exportChannelLeft[sampleWrite]  = stemPtr->m_channel[0][readSampleTimeScaledWithOffset] * stemGain;
                exportChannelRight[sampleWrite] = stemPtr->m_channel[1][readSampleTimeScaledWithOffset] * stemGain;
            }

            // output to disk, force flush immediately
            diskWriter->appendSamples( exportChannelLeft, exportChannelRight, sampleCountTimeScaled );
            diskWriter.reset();

            mem::free16( exportChannelLeft );
            mem::free16( exportChannelRight );
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
std::string Riff::generateMetadataReport() const
{
    std::string result;
    try
    {
        std::ostringstream iss;
        cereal::JSONOutputArchive archive( iss );

        Riff* mutableThis = const_cast<Riff*>(this);
        mutableThis->metadata( archive );
        result = iss.str();
    }
    catch ( cereal::Exception& cEx )
    {
        result = cEx.what();
    }
    return result;
}

} // namespace endlesss
} // namespace live
