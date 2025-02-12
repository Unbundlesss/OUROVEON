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

    
    // modify loaded config::endlesss::rAPI data with advanced option toggles
    // call after init()
    void enableFullNetworkDiagnostics();
    void enableLastMinuteQuirkFixes();


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


    endlesss::types::JamCouchID checkAndSanitizeJamCouchID( const endlesss::types::JamCouchID& jamID ) const;

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
    endlesss::types::RiffCouchID                 id;        // this will be the document ID of the riff metadata
    uint64_t                                     key = 0;   // unix nano time
    std::vector< endlesss::types::StemCouchID >  value;     // document IDs of the stems in play

    template<class Archive>
    inline void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( id )
               , CEREAL_NVP( key )
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


    // for OLLLD riffs - we're talking mid-to-late 2019 here - the stem format was slightly different
    // and lacked one of the signifiers we use to identify a workable data packet - the app data version
    // in this case we also optionally decode this extra _attachments block and use that to see if we're
    // working with old-school data
    // 
    // "_attachments": {
    //     "oggAudio": {
    //         "content_type": "audio/ogg",
    //         "revpos" : 1,
    //         "digest" : "md5-WZH7j+Na2MzTchCuQlfodA==",
    //         "length" : 37747,
    //         "stub" : true
    //     }
    // }
    struct OldAttachments
    {
        struct OldOggAudio
        {
            std::string     content_type;
            std::string     digest;

            template<class Archive>
            inline void serialize( Archive& archive )
            {
                archive( CEREAL_OPTIONAL_NVP( content_type )
                    , CEREAL_OPTIONAL_NVP( digest )
                );
            }
        } oggAudio;

        template<class Archive>
        inline void serialize( Archive& archive )
        {
            archive( CEREAL_OPTIONAL_NVP( oggAudio )
            );
        }
    } _attachments;

    std::string                 type;
    int32_t                     app_version = 0;

    template<class Archive>
    inline void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( _id )
               , CEREAL_NVP( type )
               , CEREAL_OPTIONAL_NVP( cdn_attachments )
               , CEREAL_OPTIONAL_NVP( app_version )
               , CEREAL_OPTIONAL_NVP( _attachments )
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
struct JamLatestState final : public ResultRowHeader<ResultRiffAndStemIDs>
{
    bool fetch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID );
};

// ---------------------------------------------------------------------------------------------------------------------
struct JamFullSnapshot final : public ResultRowHeader<ResultRiffAndStemIDs>
{
    bool fetch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID );
};

// ---------------------------------------------------------------------------------------------------------------------
struct JamRiffCount final : public TotalRowsOnly
{
    bool fetch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID );
};

// ---------------------------------------------------------------------------------------------------------------------
struct RiffDetails final : public ResultRowHeader<ResultDocsHeader<ResultRiffDocument, endlesss::types::RiffCouchID>>
{
    bool fetch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID, const endlesss::types::RiffCouchID& riffDocumentID );
    bool fetchBatch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID, const std::vector< endlesss::types::RiffCouchID >& riffDocumentIDs );
};

// ---------------------------------------------------------------------------------------------------------------------
// 'safe' handling for wonky stem data - parses minimal data set and all keys are basically optional
struct StemTypeCheck final : public ResultRowHeader<ResultDocsSafeHeader<TypeCheckDocument, endlesss::types::StemCouchID>>
{
    bool fetchBatch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID, const std::vector< endlesss::types::StemCouchID >& stemDocumentIDs );
};

// ---------------------------------------------------------------------------------------------------------------------
struct StemDetails final : public ResultRowHeader<ResultDocsHeader<ResultStemDocument, endlesss::types::StemCouchID>>
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

        // run regex to pull the full id from "/jam/<full id>/join" in the path
        // returns true if this succeeded
        bool extractLongJamIDFromPath( std::string& outResult );
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
                   , cereal::make_optional_nvp( "private", is_private )
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


// ---------------------------------------------------------------------------------------------------------------------
// given the long-form jam ID (eg. "487dd4be90b25fe7ea018e10d87ce251aeb41a35f781199cc0820fdab568b59c") this will use the 
// public api to fetch a page of the riff data down to the point of being able to validate stem IDs and higher structure
// (as opposed to going through the couchbase API route)
//
struct RiffStructureValidation
{
    struct Rifff
    {
        // borrow our structure from the couchbase side
        using RifffState = ResultRiffDocument::State;

        std::string             _id;            // riff couch ID
        RifffState              state;
        std::vector< ResultStemDocument >
                                loops;

        template<class Archive>
        inline void serialize( Archive& archive )
        {
            archive( CEREAL_NVP( _id )
                   , CEREAL_NVP( state )
                   , CEREAL_NVP( loops )
            );
        }
    };

    struct Data
    {
        std::string             legacy_id;     // band#### name, just for debugging/checking
        std::string             name;          // most up-to-date public name
        std::vector< Rifff >    rifffs;

        template<class Archive>
        inline void serialize( Archive& archive )
        {
            archive( CEREAL_NVP( legacy_id )
                   , CEREAL_NVP( name )
                   , CEREAL_NVP( rifffs )
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
               , CEREAL_OPTIONAL_NVP( message ) // valid if ok==false
        );
    }

    bool fetch( const NetConfiguration& ncfg, const std::string& jamLongID, int32_t pageNumber, int32_t pageSize );
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

namespace push {

// ---------------------------------------------------------------------------------------------------------------------
struct ShareRiffOnFeed
{
    endlesss::types::JamCouchID         m_jamCouchID;
    endlesss::types::RiffCouchID        m_riffCouchID;
    std::string                         m_shareName;
    bool                                m_private = true;

    // on absl::ok, resultUUID holds the shared-rifff id that should be available on the web immediately
    ouro_nodiscard absl::Status action( const NetConfiguration& ncfg, std::string& resultUUID );
};

// ---------------------------------------------------------------------------------------------------------------------
// used to 'share' to Clubs by riff copying
struct RiffCopy
{
    std::string                         m_jamFullID_CopyTo;     // eg 3549a4b5387bcb96c6fc8c5a9d2eb797c09cd46e5c42862fbb8912c977a0fa59

    // riff to copy from
    endlesss::types::JamCouchID         m_jamCouchID;
    endlesss::types::RiffCouchID        m_riffCouchID;

    // on absl::ok, resultUUID holds the copied riff ID. i guess
    ouro_nodiscard absl::Status action( const NetConfiguration& ncfg, std::string& resultUUID );
};


} // namespace push

} // namespace api
} // namespace endlesss

