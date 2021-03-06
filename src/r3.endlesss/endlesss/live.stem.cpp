//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"

#include "spacetime/moment.h"
#include "math/rng.h"
#include "dsp/fft.h"
#include "app/module.frontend.h"
#include "base/utils.h"
#include "filesys/fsutil.h"

#include "endlesss/live.stem.h"

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"


namespace endlesss {
namespace live {

// ---------------------------------------------------------------------------------------------------------------------
Stem::Stem( const types::Stem& stemData, const uint32_t targetSampleRate )
    : m_data( stemData )
    , m_state( State::Empty )
    , m_sampleRate( targetSampleRate )
    , m_sampleCount( 0 )
    , m_hasValidAnalysis( false )
{
    m_channel.fill( nullptr );

    m_colourU32 = ImGui::ParseHexColour( m_data.colour.c_str() );

    blog::stem( "allocated C:{}", m_data.couchID );
}

// ---------------------------------------------------------------------------------------------------------------------
Stem::~Stem()
{
    // let any outstanding background functions finish
    if ( m_analysisFuture.valid() )
        m_analysisFuture.wait();

    blog::stem( "releasing C:{}", m_data.couchID );

    mem::free16( m_channel[0] );
    mem::free16( m_channel[1] );

    m_sampleCount = 0;
    m_state       = State::Empty;
}

// ---------------------------------------------------------------------------------------------------------------------
void Stem::fetch( const api::NetConfiguration& ncfg, const fs::path& cachePath )
{
    // ensure we have a space to write the stem back out to
    const absl::Status cachePathAvailable = filesys::ensureDirectoryExists( cachePath );
    if ( !cachePathAvailable.ok() )
    {
        blog::error::stem( "Unable to create sub-directory in stem cache [{}], {}", cachePath.string(), cachePathAvailable.ToString() );
        m_state = State::Failed_CacheDirectory;
        return;
    }

    m_state = State::WorkEnqueued;

    spacetime::ScopedTimer stemTiming( "stem finalize" );

    // prepare download buffer
    RawAudioMemory audioMemory( m_data.fileLengthBytes );

    // check to see if we already have it downloaded
    auto cacheFile = cachePath / fmt::format( "stem.{}.ogg", m_data.couchID );
    if ( fs::exists( cacheFile ) )
    {
        blog::cache( "cached [{}]", m_data.couchID );

        auto fileSize = fs::file_size( cacheFile );

        if ( fileSize != audioMemory.m_rawLength )
        {
            // check if we should just accept discrepancies in the db/CDN size reports
            if ( fileSize > 0 && ncfg.api().hackAllowStemSizeMismatch )
            {
                blog::cache( "allowed cached file [{}] size mismatch; expected {}, got {}", m_data.couchID, audioMemory.m_rawLength, fileSize );
                audioMemory.allocate( fileSize );
            }
            else
            {
                blog::error::cache( "cached file [{}] size mismatch! expected {}, got {}", m_data.couchID, audioMemory.m_rawLength, fileSize );

                m_state = State::Failed_DataUnderflow;
                return;
            }
        }

        std::basic_ifstream<char> ifs( cacheFile, std::ios::in | std::ios::binary );
        ifs.read( (char*)audioMemory.m_rawAudio, fileSize );

        audioMemory.m_rawReceived = fileSize;
    }

    math::RNG32 lRng;

    if ( audioMemory.m_rawReceived == 0 )
    {
        bool stemFetchSuccess = false;

        blog::stem( "downloading [{}/{}] ...",
            m_data.fullEndpoint(),
            m_data.fileKey );

        // things can take a while to propogate to the CDN; wait longer each cycle and try repeatedly
        for ( auto remoteFetchAttempst = 0; remoteFetchAttempst < 3; remoteFetchAttempst++ )
        {
            const auto fetchDelayMs = lRng.genInt32( 250, 750 ) + ( remoteFetchAttempst * 1500 );

            // jitter our calls to the CDN
            std::this_thread::sleep_for( std::chrono::milliseconds( fetchDelayMs ) );

            if ( !attemptRemoteFetch( ncfg, lRng.genUInt32(), audioMemory ) )
            {
                blog::stem( "failed attempt {} for [{}], will retry", remoteFetchAttempst + 1, m_data.fileKey );
            }
            else
            {
                stemFetchSuccess = true;
                break;
            }
        }

        if ( !stemFetchSuccess )
        {
            blog::stem( "unable to acquire [{}]",  m_data.fileKey );
            return;
        }
    }

    // basic magic header check
    if ( audioMemory.m_rawAudio[0] != 'O' ||
         audioMemory.m_rawAudio[1] != 'g' ||
         audioMemory.m_rawAudio[2] != 'g' ||
         audioMemory.m_rawAudio[3] != 'S' )
    {
        blog::error::stem( "vorbis header broken [{}]", m_data.couchID );
        m_state = State::Failed_Vorbis;
        return;
    }

    // decode the vorbis stream into interleaved shorts
    short*  oggData = nullptr;
    int32_t oggChannels;
    int32_t oggSampleRate;

    m_sampleCount = stb_vorbis_decode_memory(
        audioMemory.m_rawAudio,
        (int32_t)audioMemory.m_rawLength,
        &oggChannels,
        &oggSampleRate,
        &oggData );

    if ( m_sampleCount <= 0 )
    {
        blog::error::stem( "vorbis decode fail [{} | {}", m_data.couchID, m_sampleCount );
        m_state = State::Failed_Vorbis;
        free( oggData );
        return;
    }

    if ( oggChannels != 2 )
    {
        blog::error::stem( "we only expect stereo [{} | ch:{}", m_data.couchID, oggChannels );
        m_state = State::Failed_Vorbis;
        free( oggData );
        return;
    }

    // emit a successful capture back to the cache
    {
        std::basic_ofstream<char> ofs( cacheFile, std::ios::out | std::ios::binary );
        ofs.write( (char*)audioMemory.m_rawAudio, audioMemory.m_rawReceived );
    }

    const double shortToDoubleNormalisedRcp = 1.0 / 32768.0;
    if ( oggSampleRate != m_sampleRate )
    {
        blog::stem( "resampling [{}] from {}", m_data.couchID, oggSampleRate );

        auto* resampleIn = mem::malloc16As<double>( m_sampleCount );

        r8b::CDSPResampler24 resampler24(
            (double)oggSampleRate,
            m_sampleRate,
            m_sampleCount );

        const auto outputSampleLength = resampler24.getMaxOutLen( 0 );
        double* resampleOut = mem::malloc16As<double>( outputSampleLength );

        for ( int channel = 0; channel < 2; channel++ )
        {
            for ( size_t s = 0, readIndex = channel; s < m_sampleCount; s++, readIndex += 2 )
            {
                resampleIn[s] = (double)oggData[readIndex] * shortToDoubleNormalisedRcp;
            }

            resampler24.oneshot( resampleIn, m_sampleCount, resampleOut, outputSampleLength );

            m_channel[channel] = mem::malloc16As<float>( outputSampleLength );
            for ( size_t s = 0; s < outputSampleLength; s++ )
            {
                m_channel[channel][s] = (float)resampleOut[s];
            }
        }

        mem::free16( resampleOut );
        mem::free16( resampleIn );

        m_sampleCount = outputSampleLength;
    }
    else
    {
        blog::stem( "stem [{}] already at {}", m_data.couchID, m_sampleRate );

        m_channel[0] = mem::malloc16As<float>( m_sampleCount );
        m_channel[1] = mem::malloc16As<float>( m_sampleCount );

        for ( size_t s = 0, readIndex = 0; s < m_sampleCount; s++ )
        {
            m_channel[0][s] = (float)((double)oggData[readIndex++] * shortToDoubleNormalisedRcp);
            m_channel[1][s] = (float)((double)oggData[readIndex++] * shortToDoubleNormalisedRcp);
        }
    }

    free( oggData );

    m_state = State::Complete;

    // report on our hard work
    {
        auto stemTime = stemTiming.stop();
        const auto humanisedMemoryUsage = base::humaniseByteSize( "using approx mem : ", computeMemoryUsage() );

        blog::stem( "finalizing took {}, {}",
            stemTime,
            humanisedMemoryUsage );
    }
}

// ---------------------------------------------------------------------------------------------------------------------
bool Stem::attemptRemoteFetch( const api::NetConfiguration& ncfg, const uint32_t attemptUID, RawAudioMemory& audioMemory )
{
    // create client to fetch audio stream from the CDN
    const auto& httpUrl = m_data.fullEndpoint();
    auto cdnClient      = std::make_unique< httplib::SSLClient >( httpUrl.c_str() );

    cdnClient->set_ca_cert_path( ncfg.api().certBundleRelative.c_str() );
    cdnClient->enable_server_certificate_verification( true );

    cdnClient->set_default_headers(
        {
            { "Host",            httpUrl },
            { "User-Agent",      ncfg.api().userAgentApp.c_str() },
            { "Accept",          "audio/ogg" },
            { "Accept-Encoding", "gzip, deflate, br" }
        } );

    auto slashedKey = fmt::format( "/{}", m_data.fileKey );

    
    auto precheckResult = cdnClient->Head( slashedKey.c_str() );
    auto precheckError = precheckResult.error();
    
    if ( precheckError != httplib::Error::Success )
    {
        blog::error::stem( "HEAD [{}] client failure with error : {}", slashedKey, endlesss::api::getHttpLibErrorString(precheckError) );
        m_state = State::Failed_Http;
        return false;
    }

    if ( precheckResult->status != 200 )
    {
        blog::error::stem( "HEAD [{}] response [{}]", slashedKey, precheckResult->status );
        m_state = State::Failed_Http;
        return false;
    }

    size_t precheckDataLength = 0;
    if ( precheckResult->has_header( "content-length" ) )
    {
        auto val = precheckResult->get_header_value( "content-length" );
        if ( !val.empty() )
            precheckDataLength = (size_t) std::atoll( val.c_str() );
    }

    if ( precheckDataLength != audioMemory.m_rawLength )
    {
        // check if we should just accept discrepancies in the db/CDN size reports
        if ( precheckDataLength > 0 && 
            ncfg.api().hackAllowStemSizeMismatch )
        {
            blog::stem( "HEAD [{}] allowing content-length mismatch; got [{}], DB expected [{}]", slashedKey, precheckDataLength, audioMemory.m_rawLength );

            // rebuild the audio memory block to cope
            audioMemory.allocate( precheckDataLength );
        }
        else
        {
            blog::error::stem( "HEAD [{}] content-length mismatch; got [{}], DB expected [{}]", slashedKey, precheckDataLength, audioMemory.m_rawLength );
            m_state = State::Failed_Http;
            return false;
        }
    }

    auto res = cdnClient->Get( slashedKey.c_str(), [&]( const char* data, size_t data_length )
        {
            if ( audioMemory.m_rawReceived + data_length > audioMemory.m_rawLength )
            {
                m_state = State::Failed_DataOverflow;
                return false;
            }

            memcpy( &audioMemory.m_rawAudio[audioMemory.m_rawReceived], data, data_length );
            audioMemory.m_rawReceived += data_length;

            return true;
        });

    if ( audioMemory.m_rawReceived != audioMemory.m_rawLength )
    {
        // auto result = cdnClient->get_openssl_verify_result();

        blog::error::stem( "ogg data size mismatch [{}{}] (expected {}, got {})", httpUrl, slashedKey, audioMemory.m_rawLength, audioMemory.m_rawReceived );
        m_state = State::Failed_DataUnderflow;
        return false;
    }

    if ( m_state == State::Failed_DataOverflow )
    {
        blog::error::stem( "fetch ogg overflow [{}{}]", httpUrl, slashedKey );
        return false;
    }

    if ( res == nullptr )
    {
        blog::error::stem( "fetch ogg failed [{}{}]", httpUrl, slashedKey );
        m_state = State::Failed_Http;
        return false;
    }

    if ( res->status != 200 )
    {
        blog::error::stem( "fetch ogg failed [{}{}] | {}", httpUrl, slashedKey, res->status );
        m_state = State::Failed_Http;
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
void Stem::fft()
{
    constexpr int32_t fftWindowSize     = 1024;
    constexpr int32_t fftBandBuckets    = 32;
    constexpr int32_t fftWindowToBucket = 5;    // >> value from window->bucketing
    constexpr int32_t fftEnergyHistory  = 42;
    constexpr float   fftWindowRcp      = 1.0f / (float)(fftWindowSize);
    constexpr float   fftEnergyRcp      = 1.0f / (float)(fftEnergyHistory);
    constexpr float   fftVarianceRcp    = 1.0f / (float)(fftWindowSize / fftBandBuckets);

    static_assert((fftWindowSize >> fftWindowToBucket) == fftBandBuckets, "incorrect bitshift value for window->bucket");

    // such a small stem that we can't really do much? shout out to Blackest Jammmmmmmmmmm
    if ( m_sampleCount <= fftWindowSize )
        return;

    const uint64_t    fftTimeSlices     = m_sampleCount / fftWindowSize;
    
    auto* fftDataIn       = mem::malloc16As<float>( fftWindowSize * 2 );
    auto* fftOutMagnitude = mem::malloc16As<float>( m_sampleCount );
    auto* fftOutLowBand   = mem::malloc16As<float>( fftTimeSlices );
    auto* fftOutAvgEnergy = mem::malloc16As<float>( fftTimeSlices );

    std::array< float, fftBandBuckets > subband;
    std::array< float, fftEnergyHistory > energy0;


    const double stemLengthInSeconds = (double)m_sampleCount / (double)m_sampleRate;
    const double sampleToPct = stemLengthInSeconds / (double)m_sampleCount;


    for ( auto i = 0; i < m_sampleCount; i++ )
        fftOutMagnitude[i] = 0.0f;

    uint64_t lowBandIndex = 0;

    for ( uint64_t xI = 0; xI < m_sampleCount - fftWindowSize; xI += fftWindowSize )
    {
        subband.fill( 0.0f );

        for ( int i = 0; i < fftWindowSize; i++ )
        {
            // fft<> expects data interleaved with real/imaginary
            const int32_t interleavedIndex = i << 1;
            fftDataIn[interleavedIndex] = m_channel[0][xI + i];
            fftDataIn[interleavedIndex + 1] = 0;
        }

        dsp::fft<fftWindowSize>( fftDataIn );

        for ( int i = 0; i < fftWindowSize; i++ )
        {
            const int32_t interleavedIndex = i << 1;

            const float outReal = fftDataIn[interleavedIndex];
            const float outImag = fftDataIn[interleavedIndex + 1];

            const float fftPower = ( outReal * outReal ) +
                                   ( outImag * outImag );

            const float fftMag = 2.0f * std::sqrt( fftPower ) * fftWindowRcp;

            fftOutMagnitude[xI + i] = fftMag;

            const auto subbandIdx = i >> fftWindowToBucket;
            subband[subbandIdx] += fftMag;
        }

        {
            const auto lowBand = std::max( subband[0], subband[fftBandBuckets - 1] );

            assert( lowBandIndex < fftTimeSlices );
            fftOutLowBand[lowBandIndex] = (lowBand > 0.0001f) ? lowBand : 0.0f;
            lowBandIndex++;
        }
    }

    const uint64_t lowBandRecords = lowBandIndex;

    int32_t energyIndex = 0;
    energy0.fill( 0.0f );

    // run the average energy twice to ensure we represent how energy flows in the loop
    for ( uint32_t avgPass = 0; avgPass < 2; avgPass++ )
        for ( uint64_t tsI = 0; tsI < lowBandRecords; tsI++ )
        {
            float averageEnergy = 0.0f;
            for ( int i = 0; i < fftEnergyHistory; i++ )
                averageEnergy += energy0[i];
            averageEnergy *= fftEnergyRcp;

            energy0[energyIndex] = fftOutLowBand[tsI];
            energyIndex = (energyIndex + 1) % fftEnergyHistory;

            assert( isfinite( averageEnergy ) );
            if ( avgPass == 0 )
                fftOutAvgEnergy[tsI] = averageEnergy;
            else
                fftOutAvgEnergy[tsI] = std::max( fftOutAvgEnergy[tsI], averageEnergy );
        }

    dsp::SchmittLatch<double> beatTrigger( 0.125 ); // #todo data drive this

    // controls the size of the band of values considered for variance
    constexpr int32_t varianceBandBitShift = 5;
    constexpr int32_t     varianceBandSize = fftWindowSize >> varianceBandBitShift;
    constexpr float        varianceBandRcp = 1.0f / (float)(varianceBandSize);

    m_sampleEnergy.resize( m_sampleCount );
    m_sampleBeat.resize( ( m_sampleCount >> 6 ) + 1 );

    lowBandIndex = 0;
    for ( uint64_t xI = 0; xI < m_sampleCount - fftWindowSize; xI += fftWindowSize )
    {
        const float lowBand       = fftOutLowBand[lowBandIndex];
        const float averageEnergy = fftOutAvgEnergy[lowBandIndex];


        float variance = 0.0f;
        for ( int i = 0; i < fftWindowSize >> varianceBandBitShift; i++ )
        {
            const float fftVar = fftOutMagnitude[xI + i] - lowBand;
            variance += (fftVar * fftVar);
        }
        variance *= varianceBandRcp;


        const float beatCoeff = (-0.3f * variance) + 1.125f; // #todo data drive this

        for ( uint64_t eI = 0; eI < fftWindowSize; eI ++ )
            m_sampleEnergy[xI + eI] = averageEnergy;

        if ( beatTrigger( lowBand, averageEnergy * beatCoeff ) )
        {
            // an arbitrary limit to avoid overloading the UI
            if ( m_detectedBeatTimes.size() < 100 )
                 m_detectedBeatTimes.emplace_back( (double)xI * sampleToPct );

            {
                const uint64_t bitBlock = xI >> 6;
                const uint64_t bitBit   = xI - (bitBlock << 6);
                m_sampleBeat[bitBlock] |= 1ULL << bitBit;
            }
        }

        lowBandIndex++;
    }

    mem::free16( fftOutAvgEnergy );
    mem::free16( fftOutLowBand );
    mem::free16( fftOutMagnitude );
    mem::free16( fftDataIn );

    m_hasValidAnalysis = true;
}

// ---------------------------------------------------------------------------------------------------------------------
size_t Stem::computeMemoryUsage() const
{
    size_t result = 0;

    result += sizeof( types::Stem );
    result += sizeof( int32_t ) * 4;
    result += m_sampleCount * sizeof( float ) * 2;

    if ( m_hasValidAnalysis )
    {
        result += m_detectedBeatTimes.size() * sizeof( double );
        result += m_sampleEnergy.size() * sizeof( float );
        result += m_sampleBeat.size() * sizeof( uint64_t );
    }

    return result;
}

// ---------------------------------------------------------------------------------------------------------------------
Stem::RawAudioMemory::RawAudioMemory( size_t size )
    : m_rawLength( size )
    , m_rawReceived( 0 )
    , m_rawAudio( nullptr )
{
    allocate( size );
}

// ---------------------------------------------------------------------------------------------------------------------
Stem::RawAudioMemory::~RawAudioMemory()
{
    mem::free16( m_rawAudio );
    m_rawAudio      = nullptr;
    m_rawReceived   = 0;
}

// ---------------------------------------------------------------------------------------------------------------------
void Stem::RawAudioMemory::allocate( size_t newSize )
{
    assert( m_rawReceived == 0 );

    if ( m_rawAudio == nullptr )
        mem::free16( m_rawAudio );

    m_rawLength = newSize;
    m_rawAudio = mem::malloc16AsSet< uint8_t >( m_rawLength + 4, 0 );   // +4 supports header read check in worst case of empty buf
}

} // namespace live
} // namespace endlesss
