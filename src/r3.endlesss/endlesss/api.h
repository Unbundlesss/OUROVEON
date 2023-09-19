//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "base/construction.h"

#include "net/uriparse.h"

#include "endlesss/config.h"
#include "endlesss/core.types.h"

namespace endlesss {
namespace api {

// ---------------------------------------------------------------------------------------------------------------------
// turn httplib error into printable string
const char* getHttpLibErrorString( const httplib::Error err );

// ---------------------------------------------------------------------------------------------------------------------
// binding of config data into the state required to make API calls to various bits of Endlesss backend
struct NetConfiguration
{
    using Shared = std::shared_ptr< NetConfiguration >;
    using Weak = std::weak_ptr< NetConfiguration >;

private:
    
    // regex capture for "length":"13", used to replace with "length":13 (see comments below)
    static constexpr auto cRegexLengthTypeMismatch = "\"length\":\"([0-9]+)\"";

public:

    enum class NetworkQuality
    {
        Stable,             // eg. LAN, broadband, stable
        Unstable            // eg. 4G dongle, mobile, more unreliable
    };

    enum class Access
    {
        None,               // no network configuration has been set
        Public,             // configuration set for public / web requests
        Authenticated       // full login provided for access to restricted user data
    };

    DECLARE_NO_COPY( NetConfiguration );

    NetConfiguration()
        : m_access( Access::None )
        , m_dataFixRegex_lengthTypeMismatch( cRegexLengthTypeMismatch )
    {}

    // initialise without Endlesss auth for a network layer that can't talk to Couch, etc (but can grab stuff from CDN)
    void initWithoutAuthentication(
        base::EventBusClient eventBusClient,
        const config::endlesss::rAPI& api );

    // initialise with validated Endlesss auth for full access
    void initWithAuthentication(
        base::EventBusClient eventBusClient,
        const config::endlesss::rAPI& api,
        const config::endlesss::Auth& auth );

    // writable temp location for verbose/debug logging if enabled
    void setVerboseCaptureOutputPath( const fs::path& captureDir ) { m_verboseOutputDir = captureDir; }

    ouro_nodiscard constexpr const config::endlesss::rAPI& api()  const { ABSL_ASSERT( hasAccess( Access::Public ) );        return m_api;  }
    ouro_nodiscard constexpr const config::endlesss::Auth& auth() const { ABSL_ASSERT( hasAccess( Access::Authenticated ) ); return m_auth; }

    // check what level of network access we have configured at this point
    ouro_nodiscard constexpr const bool hasAccess( Access access ) const
    { 
        switch ( access )
        {
            case Access::None:          return m_access == Access::None;
            case Access::Public:        return m_access == Access::Public ||
                                               m_access == Access::Authenticated;
            case Access::Authenticated: return m_access == Access::Authenticated;
        }
        ABSL_ASSERT( false );
        return false;
    }

    // shorthand for not having been configured with anything yet
    ouro_nodiscard constexpr const bool hasNoAccessSet() const
    {
        return m_access == Access::None;
    }

    constexpr void setQuality( NetworkQuality quality )
    {
        if ( m_quality != quality )
        {
            m_quality = quality;
            switch ( m_quality )
            {
            default:
            case NetworkQuality::Stable:    blog::api( FMTX( "network quality set to Stable" ) ); break;
            case NetworkQuality::Unstable:  blog::api( FMTX( "network quality set to Unstable" ) ); break;
            }
        }
    }
    ouro_nodiscard constexpr NetworkQuality getQuality() const
    {
        return m_quality;
    }

    // based on set network quality, how long to wait for endlessss servers to pick up / write back
    ouro_nodiscard constexpr std::chrono::seconds getRequestTimeout() const
    {
        using namespace std::chrono_literals;

        switch ( m_quality )
        {
        default:
        case NetworkQuality::Stable:    return 1s * m_api.networkTimeoutInSecondsDefault;
        case NetworkQuality::Unstable:  return 1s * m_api.networkTimeoutInSecondsUnstable;
        }
    }

    // based on set network quality, how many retries should we give API calls
    ouro_nodiscard constexpr int32_t getRequestRetries() const
    {
        switch ( m_quality )
        {
        default:
        case NetworkQuality::Stable:    return m_api.networkRequestRetryLimitDefault;
        case NetworkQuality::Unstable:  return m_api.networkRequestRetryLimitUnstable;
        }
    }

    // spin an RNG to produce a new LB=live## cookie value
    ouro_nodiscard std::string generateRandomLoadBalancerCookie() const;

    // when doing comprehensive traffic capture, ask for a full path to a file to write to; this will include
    // some kind of timestamp or order differentiator. returns empty string if the output directory hasn't been set.
    ouro_nodiscard std::string getVerboseCaptureFilename( std::string_view context ) const;


    ouro_nodiscard constexpr const std::regex& getDataFixRegex_lengthTypeMismatch() const { return m_dataFixRegex_lengthTypeMismatch; }


    // utility function used by API calls to get their call attempted an getRequestRetries() number of times, returning
    // on success (or whatever the final failure is otherwise)
    httplib::Result attempt( const std::function<httplib::Result()>& operation ) const;


    // metrics functions that dispatch network activity events via mutable event bus
    // used to tell the rest of the app that network stuff is happening
    void metricsActivitySend() const
    {
        m_eventBusClient->Send< ::events::NetworkActivity >( 0 );
    }
    void metricsActivityRecv( std::size_t bytesIn ) const
    {
        m_eventBusClient->Send< ::events::NetworkActivity >( bytesIn );
    }
    void metricsActivityFailure() const
    {
        m_eventBusClient->Send< ::events::NetworkActivity >( ::events::NetworkActivity::failure() );
    }

private:

    using EventBusOpt = std::optional< base::EventBusClient >;

    // run after either init configuration call
    void postInit();

    // counter used for debug verbose captures to avoid duplication
    inline static std::atomic_uint32_t  m_writeIndex = 0;

    NetworkQuality              m_quality = NetworkQuality::Stable;
    Access                      m_access = Access::None;

    mutable EventBusOpt         m_eventBusClient = std::nullopt;

    config::endlesss::rAPI      m_api;
    config::endlesss::Auth      m_auth;

    // for capture/debug output
    fs::path                    m_verboseOutputDir;

    // used to precondition incoming data against the version of endless that decided to start writing "Length" values
    // as strings instead of numbers - this regex patches those back to numbers
    std::regex                  m_dataFixRegex_lengthTypeMismatch;
};

enum class UserAgent
{
    ClientService,
    Couchbase,
    WebWithoutAuth,         // plain external web APIs without any credentials
    WebWithAuth,            // as above but with the user authentication included
};

// ---------------------------------------------------------------------------------------------------------------------
// general boilerplate that takes a httplib response and tries to deserialize it from JSON to
// the given type, returning false and logging the error if parsing bails
template< typename _Type >
inline static bool deserializeJson(
    const NetConfiguration& netConfig,
    const httplib::Result& res,
    _Type& instance,
    const std::string& functionContext,
    std::string_view traceContext,
    const std::function< void( std::string& ) >& bodyTextProcessor = nullptr )
{
    if ( res == nullptr )
    {
        blog::error::api( "HTTP | connection failure | <context> {}\n", functionContext.c_str() );
        return false;
    }
    if ( res->status != 200 )
    {
        blog::error::api( "HTTP | request status {} | {} | <context> {}\n", res->status, res->body, functionContext.c_str() );
        return false;
    }

    //
    // apply horribly inefficient kludge to work around one particular version of Endlesss that decided to start
    // writing out "length" keys as strings rather than numbers :O *shakes fist*
    //
    std::string bodyText = std::regex_replace( res->body, netConfig.getDataFixRegex_lengthTypeMismatch(), "\"length\":$1" );

    // allow custom body modifications pre-parse in case there are any other hilarious json tripmines to work around
    if ( bodyTextProcessor )
    {
        bodyTextProcessor( bodyText );
    }

    // optional heavy debug verbose output option
    if ( netConfig.api().debugVerboseNetDataCapture )
    {
        const auto verboseFilename = netConfig.getVerboseCaptureFilename( traceContext );
        if ( !verboseFilename.empty() )
        {
            FILE* fExport = fopen( verboseFilename.c_str(), "wt" );
            fprintf( fExport, "%s\n\n", functionContext.c_str() );
            fprintf( fExport, "%s\n", bodyText.c_str() );
            fclose( fExport );
        }
    }

    netConfig.metricsActivityRecv( bodyText.size() );

    // attempt the parse
    try
    {
        std::istringstream is( bodyText );
        cereal::JSONInputArchive archive( is );

        instance.serialize( archive );
    }
    catch ( cereal::Exception& cEx )
    {
        // cereal is not very good for actually backtracking to where/what failed in JSON parsing
        // in the case we hit more malformed data, burn it out to disk so someone can send it to me for analysis

        const auto exportFilename = netConfig.getVerboseCaptureFilename( "json_parse_error" );
        if ( !exportFilename.empty() )
        {
            FILE* fExport = fopen( exportFilename.c_str(), "wt" );
            fprintf( fExport, "%s\n", cEx.what() );
            fprintf( fExport, "%s\n\n", functionContext.c_str() );
            fprintf( fExport, "%s\n", bodyText.c_str() );
            fclose( fExport );
        }

        blog::error::api( "JSON | {} | {}", functionContext, cEx.what() );
        blog::error::api( "JSON | problematic JSON saved to [{}]", exportFilename );
        blog::error::api( "JSON | please send it to ishani" );
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// leanest parse possible, just the number of results that would have been returned
struct TotalRowsOnly
{
    uint32_t                    total_rows = 0;

    template<class Archive>
    inline void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( total_rows )
        );
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// most queries return a pile of 1-or-more results with this structure
// as the top of the json; total_rows is not the size of rows[], it holds the size
// of the unfiltered data on the server
template< typename _rowType >
struct ResultRowHeader
{
    uint32_t                    total_rows = 0;
    std::vector< _rowType >     rows;

    template<class Archive>
    inline void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( total_rows )
               , CEREAL_NVP( rows )
        );
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// a document query has a generic header too, the id of the document and then a sub-object
// holding the actual data (as well as some other things we ignore)
template< typename _docType, typename _keyType >
struct ResultDocsHeader
{
    _keyType        id;
    _docType        doc;


    template<class Archive>
    inline void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( id )
               , CEREAL_NVP( doc )
        );
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// a document query has a generic header too, the id of the document and then a sub-object
// holding the actual data (as well as some other things we ignore)
template< typename _docType, typename _keyType >
struct ResultDocsSafeHeader
{
    _keyType        key;
    std::string     error;
    _docType        doc;

    // track cases where we have a null [doc] and a mark of deletion in [value]
    // {
    //     "id": "afa5e840694f11eaa8405fee66bfbe0f",
    //     "key" : "afa5e840694f11eaa8405fee66bfbe0f",
    //     "value" :
    //     {
    //         "rev": "2-17dc387f79bfcd9b709a9ba3614d27c3",
    //         "deleted" : true
    //     },
    //     "doc" : null
    // },
    //
    struct InnerValue
    {
        bool deleted = false;
        template<class Archive>
        inline void serialize( Archive& archive )
        {
            archive( CEREAL_OPTIONAL_NVP( deleted )
            );
        }
    };

    InnerValue value;

    // cope with broken documents, eg
    // "{"key":"d49d1cf0b0ce11eb9cf036dbe4428707","error":"not_found"}," (ie i think if a stem got removed for moderation purposes)
    // 
    template<class Archive>
    inline void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( key )
               , CEREAL_OPTIONAL_NVP( value )
               , CEREAL_OPTIONAL_NVP( error )
               , CEREAL_OPTIONAL_NVP( doc )
        );
    }
};


// ---------------------------------------------------------------------------------------------------------------------
// return of rifffLoopsByCreateTime() query function, representing a stored riff as ids
struct ResultRiffAndStemIDs
{
    endlesss::types::RiffCouchID                 id;     // this will be the document ID of the riff metadata
    std::vector< endlesss::types::StemCouchID >  value;  // document IDs of the stems in play

    template<class Archive>
    inline void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( id )
               , CEREAL_NVP( value )
        );
    }
};


// ---------------------------------------------------------------------------------------------------------------------
// very minimal document that allows us to check the 'type' value; ie "Loop" or "ChatMessage" and the basic required
// bits of stem details -- can parse other types of documents with optional NVP entries
struct TypeCheckDocument
{
    endlesss::types::StemCouchID    _id;    // couch uid

    // basic structure of CDN stuff; some beta versions managed to not save oggAudio, resulting in broken stem data
    struct CDNAttachments
    {
        struct OGGAudio
        {
            std::string     endpoint;   // eg. "ndls-att0.fra1.digitaloceanspaces.com", 

            template<class Archive>
            inline void serialize( Archive& archive )
            {
                archive( CEREAL_OPTIONAL_NVP( endpoint ) );
            }
        } oggAudio;

        struct FLACAudio
        {
            std::string     endpoint;   // eg. "ndls-att0.fra1.digitaloceanspaces.com", 

            template<class Archive>
            inline void serialize( Archive& archive )
            {
                archive( CEREAL_OPTIONAL_NVP( endpoint ) );
            }
        } flacAudio;

        template<class Archive>
        inline void serialize( Archive& archive )
        {
            archive( CEREAL_OPTIONAL_NVP( oggAudio )
                    ,CEREAL_OPTIONAL_NVP( flacAudio )
            );
        }
    } cdn_attachments;

    std::string                 type;
    int32_t                     app_version = 0;

    template<class Archive>
    inline void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( _id )
               , CEREAL_NVP( type )
               , CEREAL_OPTIONAL_NVP( cdn_attachments )
               , CEREAL_OPTIONAL_NVP( app_version )
        );
    }
};


// ---------------------------------------------------------------------------------------------------------------------
// the extremely nested full metadata for a riff
struct ResultRiffDocument
{
    struct State
    {
        struct Playback
        {
            struct Slot
            {
                struct Current
                {
                    Current()
                        : on( false )
                        , gain( 0 )
                    {}

                    bool            on = false;
                    std::string     currentLoop;    // stem document ID
                    float           gain = 0;

                    template<class Archive>
                    inline void serialize( Archive& archive )
                    {
                        archive( CEREAL_NVP( on )
                               , CEREAL_OPTIONAL_NVP( currentLoop )
                               , CEREAL_NVP( gain )
                        );

                        // fix for some data where currentLoop got nulled even though 'on' is true
                        if ( currentLoop.empty() )
                            on = false;
                    }
                } current;

                template<class Archive>
                inline void serialize( Archive& archive )
                {
                    archive( CEREAL_OPTIONAL_NVP( current )
                    );
                }
            } slot;

            template<class Archive>
            inline void serialize( Archive& archive )
            {
                archive( CEREAL_NVP( slot )
                );
            }
        };

        float                   bps = 0;
        float                   barLength = 0;
        std::vector< Playback > playback;

        template<class Archive>
        inline void serialize( Archive& archive )
        {
            archive( CEREAL_NVP( bps )
                   , CEREAL_NVP( barLength )
                   , CEREAL_NVP( playback )
            );
        }
    };

    endlesss::types::RiffCouchID
                                _id;    // couch uid

    State                       state;
    std::string                 userName;
    uint64_t                    created     = 0;
    int32_t                     root        = 0;
    int32_t                     scale       = 0;
    int32_t                     app_version = 0;
    float                       magnitude   = 0;

    template<class Archive>
    inline void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( _id )
               , CEREAL_NVP( state )
               , CEREAL_NVP( userName )
               , CEREAL_NVP( created )
               , CEREAL_NVP( root )
               , CEREAL_NVP( scale )
               , CEREAL_OPTIONAL_NVP( app_version )
               , CEREAL_OPTIONAL_NVP( magnitude )
        );
    }
};

// ---------------------------------------------------------------------------------------------------------------------
struct IStemAudioFormat
{
    enum class CompressionFormat
    {
        OGG,
        FLAC
    };

    virtual ~IStemAudioFormat() = default;

    virtual std::string_view getEndpoint() const = 0;
    virtual std::string_view getBucket() const { return ""; }   // optional
    virtual std::string_view getKey() const = 0;
    virtual std::string_view getMIME() const = 0;
    virtual int32_t getLength() const = 0;
    virtual CompressionFormat getFormat() const = 0;

#define IStemAudioFormat_DefaultImpl                                            \
    std::string_view getEndpoint() const override { return endpoint; }          \
    std::string_view getKey() const override { return key; }                    \
    std::string_view getMIME() const override { return mime; }                  \
    int32_t getLength() const override { return length; }

};

struct ResultStemDocument
{
    struct CDNAttachments
    {
        struct OGGAudio : public IStemAudioFormat
        {
            std::string     bucket;                 // in old jams, this could be set (eg. would be "ndls-att0")
            std::string     endpoint;               // eg. "ndls-att0.fra1.digitaloceanspaces.com", 
            std::string     key;                    // eg. "attachments/oggAudio/####/################"
            std::string     url;                    // full URL to the audio data
            std::string     mime = "audio/ogg";     // leave as default; very old jams (/stems) lack MIME data
            int32_t         length = 0;

            // implement IStemAudioFormat
            IStemAudioFormat_DefaultImpl;
            std::string_view getBucket() const override { return bucket; }
            CompressionFormat getFormat() const override { return IStemAudioFormat::CompressionFormat::OGG; }

            // OGGAudio blocks have changed a bit over the lifetime of the app and require the most manual 
            // fix-up and error-correction
            template<class Archive>
            inline void serialize( Archive& archive )
            {
                archive( CEREAL_OPTIONAL_NVP( bucket )
                       , CEREAL_NVP( endpoint )
                       , CEREAL_OPTIONAL_NVP( key )     // in some old jams, key is missing!
                       , CEREAL_NVP( url )
                       , CEREAL_NVP( length )
                );

                // -- old jams can contain weird, badly formatted data : fix it as we go --

                // damage / old data missing "key"; re-derive it from URL
                if ( key.empty() )
                {
                    const base::UriParse parser( url );
                    if ( !parser.isValid() )
                    {
                        blog::error::api( "URL Parse fail : {}", url );
                        throw cereal::Exception( "failed to parse missing key from existing URL" );
                    }

                    key = parser.path().substr(1);   // skip the leading "/"
                    if ( key.empty() )
                    {
                        blog::error::api( "URL Parse fail : {}", url );
                        throw cereal::Exception( "failed to parse missing key from existing URL" );
                    }

                    blog::api( "Fixed missing [key] in ogg data" );
                }

                // some weird batch of data encoded https://<bucket> as a prefix into the endpoint; remove it
                if ( endpoint.rfind( "http", 0 ) == 0 )
                {
                    const std::size_t found = endpoint.find_last_of( '/' );
                    if ( found == 0 || found == std::string::npos )
                    {
                        blog::error::api( "Endpoint fix : {}", endpoint );
                        throw cereal::Exception( "failed to fix invalid endpoint data" );
                    }

                    endpoint = endpoint.substr( found + 1 ); // +1 to skip the /

                    blog::api( "Fixed invalid [endpoint] in ogg data" );
                }
                // check if the bucket is already prepended to the endpoint spec
                if ( !bucket.empty() && endpoint.rfind( bucket, 0 ) == 0 )
                {
                    // remove bucket, the endpoint is already valid
                    bucket = "";

                    blog::api( "Fixed invalid [bucket] in ogg data" );
                }
            }
        } oggAudio;

        struct FLACAudio : public IStemAudioFormat
        {
            std::string     endpoint;               // eg. "endlesss-dev.fra1.digitaloceanspaces.com", 
            std::string     hash;                   // no idea 
            std::string     key;                    // eg. "attachments/flacAudio/band####/################"
            int32_t         length = 0;
            std::string     mime = "audio/flac";    // as one might expect
            std::string     url;                    // full URL to the audio data

            // implement IStemAudioFormat
            IStemAudioFormat_DefaultImpl;
            CompressionFormat getFormat() const override { return IStemAudioFormat::CompressionFormat::FLAC; }

            template<class Archive>
            inline void serialize( Archive& archive )
            {
                archive( CEREAL_NVP( endpoint )
                       , CEREAL_NVP( key )
                       , CEREAL_NVP( length )
                       , CEREAL_NVP( url )
                );
            }

        } flacAudio;

        template<class Archive>
        inline void serialize( Archive& archive )
        {
            // oggAudiomarked as OPTIONAL as there are very rare times where there seems to just be no ogg data at all
            // .. and in those times, we can leave the ogg structure above barren, the riff resolover can deal with that, 
            // it's braver than cereal's very picky json parsing
            archive( CEREAL_OPTIONAL_NVP( oggAudio )
            // and of course flacAudio is a recent addition for the lossless platform. hopefully at least one of these
            // data blocks is here otherwise we're in trouble
                   , CEREAL_OPTIONAL_NVP( flacAudio )
            );
        }

        // choose a prevailing audio format for this stem, given the deserialised data
        // one of oggData or flacData should have something for us to use...
        const IStemAudioFormat& getAudioFormat() const
        {
            const bool bHasFLAC = (flacAudio.getLength() > 0);
            const endlesss::api::IStemAudioFormat& audioFormat = bHasFLAC ?
                static_cast<const endlesss::api::IStemAudioFormat&>(flacAudio) :
                static_cast<const endlesss::api::IStemAudioFormat&>(oggAudio);

            return audioFormat;
        }

    } cdn_attachments;

    endlesss::types::StemCouchID
                            _id;    // couch uid

    float                   bps;
    float                   length16ths;
    float                   originalPitch;
    float                   barLength;
    std::string             presetName;
    std::string             creatorUserName;
    std::string             primaryColour;
    float                   sampleRate;         // has to be float now as Studio seems to save with decimal precision
    uint64_t                created;
    bool                    isDrum;
    bool                    isNote;
    bool                    isBass;
    bool                    isMic;

    template<class Archive>
    inline void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( _id )
               , CEREAL_NVP( cdn_attachments )
               , CEREAL_NVP( bps )
               , CEREAL_NVP( length16ths )
               , CEREAL_NVP( originalPitch )
               , CEREAL_NVP( barLength )
               , CEREAL_NVP( presetName )
               , CEREAL_NVP( creatorUserName )
               , CEREAL_NVP( primaryColour )
               , CEREAL_NVP( sampleRate )
               , CEREAL_NVP( created )
               , CEREAL_OPTIONAL_NVP( isDrum )
               , CEREAL_OPTIONAL_NVP( isNote )
               , CEREAL_OPTIONAL_NVP( isBass )
               , CEREAL_OPTIONAL_NVP( isMic )
        );
    }
};

// ---------------------------------------------------------------------------------------------------------------------
struct JamProfile
{
    std::string     displayName;
    int32_t         app_version = 0;                    // default value for old jams without versioning
    std::string     bio;

    template<class Archive>
    inline void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( displayName )
               , CEREAL_OPTIONAL_NVP( app_version )     // sometimes absent on older jams
               , CEREAL_OPTIONAL_NVP( bio )             //
        );
    }

    bool fetch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID );
};

// ---------------------------------------------------------------------------------------------------------------------
struct JamChanges
{
    struct Entry
    {
        std::string id;
        std::string seq;

        template<class Archive>
        inline void serialize( Archive& archive )
        {
            archive( CEREAL_NVP( id )
                   , CEREAL_NVP( seq )
            );
        }
    };

    std::string             last_seq;
    int32_t                 pending;
    std::vector< Entry >    results;

    template<class Archive>
    inline void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( last_seq )
               , CEREAL_NVP( pending )
               , CEREAL_NVP( results )
        );
    }

    static constexpr auto keyBody = R"({ "feed" : "normal", "style" : "all_docs", "active_only" : true })";

    bool fetch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID );

    bool fetchSince( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID, const std::string& seqSince );
};

// ---------------------------------------------------------------------------------------------------------------------
struct JamLatestState : public ResultRowHeader<ResultRiffAndStemIDs>
{
    bool fetch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID );
};

// ---------------------------------------------------------------------------------------------------------------------
struct JamFullSnapshot : public ResultRowHeader<ResultRiffAndStemIDs>
{
    bool fetch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID );
};

// ---------------------------------------------------------------------------------------------------------------------
struct JamRiffCount : public TotalRowsOnly
{
    bool fetch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID );
};

// ---------------------------------------------------------------------------------------------------------------------
struct RiffDetails : public ResultRowHeader<ResultDocsHeader<ResultRiffDocument, endlesss::types::RiffCouchID>>
{
    bool fetch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID, const endlesss::types::RiffCouchID& riffDocumentID );
    bool fetchBatch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID, const std::vector< endlesss::types::RiffCouchID >& riffDocumentIDs );
};

// ---------------------------------------------------------------------------------------------------------------------
// 'safe' handling for wonky stem data - parses minimal data set and all keys are basically optional
struct StemTypeCheck : public ResultRowHeader<ResultDocsSafeHeader<TypeCheckDocument, endlesss::types::StemCouchID>>
{
    bool fetchBatch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID, const std::vector< endlesss::types::StemCouchID >& stemDocumentIDs );
};

// ---------------------------------------------------------------------------------------------------------------------
struct StemDetails : public ResultRowHeader<ResultDocsHeader<ResultStemDocument, endlesss::types::StemCouchID>>
{
    bool fetchBatch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID, const std::vector< endlesss::types::StemCouchID >& stemDocumentIDs );
};

// ---------------------------------------------------------------------------------------------------------------------
// return of app_client_config/bands:joinable - the current batch of public "Join In" front-page jams listed on the app
//
struct CurrentJoinInJams
{
    std::vector< std::string >  band_ids;

    template<class Archive>
    inline void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( band_ids )
        );
    }

    bool fetch( const NetConfiguration& ncfg );
};

// ---------------------------------------------------------------------------------------------------------------------
struct CurrentCollectibleJams
{
    struct LatestRiff
    {
        uint64_t                    created = 0;

        template<class Archive>
        inline void serialize( Archive& archive )
        {
            archive( CEREAL_OPTIONAL_NVP( created )
            );
        }
    };
    struct Data
    {
        std::string                 jamId;
        std::string                 name;
        std::string                 bio;
        std::string                 owner;
        std::vector< std::string >  members;
        std::string                 legacy_id;
        LatestRiff                  rifff;

        template<class Archive>
        inline void serialize( Archive& archive )
        {
            archive( CEREAL_NVP( jamId )
                   , CEREAL_NVP( name )
                   , CEREAL_OPTIONAL_NVP( bio )     // weirdly these can all turn up "null"
                   , CEREAL_OPTIONAL_NVP( owner )
                   , CEREAL_OPTIONAL_NVP( members )
                   , CEREAL_NVP( legacy_id )
                   , CEREAL_OPTIONAL_NVP( rifff )
            );
        }
    };

    bool ok;
    std::vector< Data > data;

    template<class Archive>
    inline void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( ok )
               , CEREAL_OPTIONAL_NVP( data )
        );
    }

    bool fetch( const NetConfiguration& ncfg, int32_t pageNo );
};


// ---------------------------------------------------------------------------------------------------------------------
struct ResultJamMembership
{
    std::string                 id;     // database ID of jam
    std::string                 key;    // timestamp when joined, eg "2020-09-23T13:04:02.375Z"

    template<class Archive>
    inline void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( id )
               , CEREAL_NVP( key )
        );
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// return of getting jams that the user has subscribed to / are private to them
//
struct SubscribedJams : public ResultRowHeader<ResultJamMembership>
{
    bool fetch( const NetConfiguration& ncfg, const std::string& userName );
};

// ---------------------------------------------------------------------------------------------------------------------
// fetching some basic jam data via the couch database band ID by 'borrowing' the permalink/join-url generator which seems
// to largely not need authentication to retrieve stuff (unlike listenlink)
//
struct BandPermalinkMeta
{
    struct Data
    {
        std::string                 url;        // full spec /join URL for this jam
        std::string                 path;       // just the "/jam/<full id>/join" component of the URL
        endlesss::types::JamCouchID band_id;    // repeated couch ID
        std::string                 band_name;  // public display name

        template<class Archive>
        inline void serialize( Archive& archive )
        {
            archive( CEREAL_NVP( url )
                   , CEREAL_NVP( path )
                   , CEREAL_NVP( band_id )
                   , CEREAL_NVP( band_name )
            );
        }
    };

    std::string result;
    Data        data;

    std::vector< std::string > errors;

    template<class Archive>
    inline void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( result )
               , CEREAL_NVP( data )
               , CEREAL_NVP( errors )
        );
    }

    bool fetch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID );
};

// ---------------------------------------------------------------------------------------------------------------------
// given the long-form jam ID (eg. "487dd4be90b25fe7ea018e10d87ce251aeb41a35f781199cc0820fdab568b59c") this will use the 
// public riffs api to fetch its current name, as opposed to the original name set at creation time that BandPermalinkMeta returns
//
struct BandNameFromExtendedID
{
    struct Data
    {
        std::string      legacy_id;     // band#### name, just for debugging/checking
        std::string      name;          // most up-to-date public name

        template<class Archive>
        inline void serialize( Archive& archive )
        {
            archive( CEREAL_NVP( legacy_id )
                   , CEREAL_NVP( name )
            );
        }
    };

    bool        ok;
    Data        data;
    std::string message;

    template<class Archive>
    inline void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( ok )
               , CEREAL_NVP( data )
               , CEREAL_OPTIONAL_NVP( message ) // valid if ok==false (afaik)
        );
    }

    bool fetch( const NetConfiguration& ncfg, const std::string& jamLongID );
};


// ---------------------------------------------------------------------------------------------------------------------
//
struct SharedRiffsByUser
{
    struct Data
    {
        std::string             _id;        // shared-riff ID, not the same as the riff ID
        std::string             doc_id;

        endlesss::types::JamCouchID
                                band;
        uint64_t                action_timestamp;
        std::string             title;
        std::vector< std::string >  
                                creators;
        ResultRiffDocument      rifff;
        std::vector< ResultStemDocument >
                                loops;
        std::string             image_url;
        bool                    image;
        bool                    is_private;

        template<class Archive>
        inline void serialize( Archive& archive )
        {
            archive( CEREAL_NVP( _id )
                   , CEREAL_NVP( doc_id )
                   , CEREAL_OPTIONAL_NVP( band )
                   , CEREAL_NVP( action_timestamp )
                   , CEREAL_NVP( title )
                   , CEREAL_OPTIONAL_NVP( creators )
                   , CEREAL_NVP( rifff )
                   , CEREAL_NVP( loops )
                   , CEREAL_OPTIONAL_NVP( image_url )
                   , CEREAL_NVP( image )
                   , cereal::make_nvp( "private", is_private )
            );
        }
    };

    std::vector< Data >   data;

    template<class Archive>
    inline void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( data )
        );
    }

    bool fetch( const NetConfiguration& ncfg, const std::string& userName, int32_t count, int32_t offset );

    // load a specific shared-riff by specific ID rather than a batch
    bool fetchSpecific( const NetConfiguration& ncfg, const endlesss::types::SharedRiffCouchID& sharedRiffID );

private:

    // both fetch/fetchSpecific use the same basic processing, just a different initial call
    bool commonRequest( const NetConfiguration& ncfg, const std::string& requestUrl, const std::string& requestContext );
};

namespace pull {

// ---------------------------------------------------------------------------------------------------------------------
// synchronously fetch the latest state of a jam (use this in a task or a thread if you want it to not block)
struct LatestRiffInJam
{
    LatestRiffInJam(
        const endlesss::types::JamCouchID&  jamCouchID,
        const std::string&                  jamDisplayName )
        : m_jamCouchID( jamCouchID )
        , m_jamDisplayName( jamDisplayName )
        , m_dataIsValid( false )
    {
    }

    ouro_nodiscard inline bool trySynchronousLoad( const NetConfiguration& ncfg )
    {
        m_dataIsValid = true;
        m_dataIsValid &= m_dataJamLatestState.fetch( ncfg, m_jamCouchID );
        if ( m_dataIsValid )
        {
            m_dataIsValid &= m_dataRiffDetails.fetch( ncfg, m_jamCouchID, m_dataJamLatestState.rows[0].id );
            m_dataIsValid &= m_dataStemDetails.fetchBatch( ncfg, m_jamCouchID, m_dataJamLatestState.rows[0].value );
        }

        return m_dataIsValid;
    }

    ouro_nodiscard constexpr bool isDataValid() const { return m_dataIsValid; }

    ouro_nodiscard constexpr const endlesss::api::JamLatestState& getJamState() const { ABSL_ASSERT( isDataValid() ); return m_dataJamLatestState; }
    ouro_nodiscard constexpr const endlesss::api::RiffDetails& getRiffDetails() const { ABSL_ASSERT( isDataValid() ); return m_dataRiffDetails; }
    ouro_nodiscard constexpr const endlesss::api::StemDetails& getStemDetails() const { ABSL_ASSERT( isDataValid() ); return m_dataStemDetails; }

    const endlesss::types::JamCouchID   m_jamCouchID;
    const std::string                   m_jamDisplayName;

private:
    endlesss::api::JamLatestState       m_dataJamLatestState;
    endlesss::api::RiffDetails          m_dataRiffDetails;
    endlesss::api::StemDetails          m_dataStemDetails;
    bool                                m_dataIsValid;
};

} // namespace pull

} // namespace api
} // namespace endlesss
