//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "net/uriparse.h"

#include "endlesss/config.h"
#include "endlesss/core.types.h"

namespace endlesss {
namespace api {

// binding of config data into the state required to make API calls to various bits of Endlesss backend
struct NetConfiguration
{
    NetConfiguration() = delete;

    // initialise without Endlesss auth for a network layer that can't talk to Couch, etc (but can grab stuff from CDN)
    NetConfiguration(
        const config::endlesss::rAPI& api,
        const fs::path& tempDir );                  // writable temp location for verbose/debug logging if enabled

    // initialise with validated Endlesss auth for full access
    NetConfiguration(
        const config::endlesss::rAPI& api,
        const config::endlesss::Auth& auth,
        const fs::path& tempDir );                  // writable temp location for verbose/debug logging if enabled

    ouro_nodiscard constexpr const config::endlesss::rAPI& api()  const { return m_api; }
    ouro_nodiscard constexpr const config::endlesss::Auth& auth() const { return m_auth; }
    ouro_nodiscard constexpr const bool hasValidEndlesssAuth()    const { return m_hasValidEndlesssAuth; }

    // spin an RNG to produce a new LB=live## cookie value
    ouro_nodiscard std::string generateRandomLoadBalancerCookie() const;

    // when doing comprehensive traffic capture, ask for a full path to a file to write to; this will include
    // some kind of timestamp or order differentiator
    ouro_nodiscard std::string getVerboseCaptureFilename( const char* context ) const;


    ouro_nodiscard constexpr const std::regex& getDataFixRegex_lengthTypeMismatch() const { return m_dataFixRegex_lengthTypeMismatch; }

private:

    inline static std::atomic_uint32_t  m_writeIndex = 0;

    config::endlesss::rAPI      m_api;
    config::endlesss::Auth      m_auth;

    bool                        m_hasValidEndlesssAuth = false;

    // for capture/debug output
    fs::path                    m_tempDir;

    // used to precondition incoming data against the version of endless that decided to start writing "Length" values
    // as strings instead of numbers - this regex patches those back to numbers
    std::regex                  m_dataFixRegex_lengthTypeMismatch;
};

enum class UserAgent
{
    ClientService,
    Couchbase,
    WebAPI
};


// ---------------------------------------------------------------------------------------------------------------------
// used by all API calls to create a primed http client instance; seeded with the correct headers, authentication, SSL etc
// 
std::unique_ptr<httplib::SSLClient> createEndlesssHttpClient( const NetConfiguration& ncfg, const UserAgent ua );

// ---------------------------------------------------------------------------------------------------------------------
const char* getHttpLibErrorString( const httplib::Error err );

// ---------------------------------------------------------------------------------------------------------------------
// general boilerplate that takes a httplib response and tries to deserialize it from JSON to
// the given type, returning false and logging the error if parsing bails
template< typename _Type >
inline static bool deserializeJson( const NetConfiguration& ncfg, const httplib::Result& res, _Type& instance, const std::string& functionContext )
{
    if ( res == nullptr )
    {
        blog::error::api( "HTTP | connection failure\n" );
        return false;
    }
    if ( res->status != 200 )
    {
        blog::error::api( "HTTP | request status {} | {}\n", res->status, res->body );
        return false;
    }

    auto bodyText = res->body;

    //
    // apply horribly inefficient kludge to work around one particular version of Endlesss that decided to start
    // writing out "length" keys as strings rather than numbers :O *shakes fist*
    //
    bodyText = std::regex_replace( bodyText, ncfg.getDataFixRegex_lengthTypeMismatch(), "\"length\":$1" );

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

        const auto exportFilename = ncfg.getVerboseCaptureFilename( "json_parse_error" );
        {
            FILE* fExport = fopen( exportFilename.c_str(), "wt" );
            fprintf( fExport, "%s\n\n", cEx.what() );
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
    uint32_t                    total_rows;

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
    uint32_t                    total_rows;
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

        template<class Archive>
        inline void serialize( Archive& archive )
        {
            archive( CEREAL_OPTIONAL_NVP( oggAudio ) );
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

                    bool            on;
                    std::string     currentLoop;    // stem document ID
                    float           gain;

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

        float                   bps;
        float                   barLength;
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
struct ResultStemDocument
{
    struct CDNAttachments
    {
        struct OGGAudio
        {
            std::string     bucket;                 // in old jams, this could be set (eg. would be "ndls-att0")
            std::string     endpoint;               // eg. "ndls-att0.fra1.digitaloceanspaces.com", 
            std::string     key;                    // eg. "attachments/oggAudio/####/################"
            std::string     url;                    // full URL to the audio data
            std::string     mime = "audio/ogg";     // leave as default; very old jams (/stems) lack MIME data
            int32_t         length = 0;

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
                    std::size_t found = endpoint.find_last_of( "/" );
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

        template<class Archive>
        inline void serialize( Archive& archive )
        {
            archive( CEREAL_NVP( oggAudio ) );
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
    bool fetch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID )
    {
        auto res = createEndlesssHttpClient( ncfg, UserAgent::Couchbase )->Get(
            fmt::format( "/user_appdata${}/_design/types/_view/rifffLoopsByCreateTime?descending=true&limit=1", jamDatabaseID ).c_str() );

        return deserializeJson< JamLatestState >( ncfg, res, *this, __FUNCTION__ );
    }
};

// ---------------------------------------------------------------------------------------------------------------------
struct JamFullSnapshot : public ResultRowHeader<ResultRiffAndStemIDs>
{
    bool fetch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID )
    {
        auto res = createEndlesssHttpClient( ncfg, UserAgent::Couchbase )->Get(
            fmt::format( "/user_appdata${}/_design/types/_view/rifffLoopsByCreateTime?descending=true", jamDatabaseID ).c_str() );

        return deserializeJson< JamFullSnapshot >( ncfg, res, *this, __FUNCTION__ );
    }
};

// ---------------------------------------------------------------------------------------------------------------------
struct JamRiffCount : public TotalRowsOnly
{
    bool fetch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID )
    {
        auto res = createEndlesssHttpClient( ncfg, UserAgent::Couchbase )->Get(
            fmt::format( "/user_appdata${}/_design/types/_view/rifffsByCreateTime", jamDatabaseID ).c_str() );

        return deserializeJson< JamRiffCount >( ncfg, res, *this, __FUNCTION__ );
    }
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
//
struct SharedRiffsByUser
{
    struct Riff
    {
        std::string             jamId;
        std::string             image;

        template<class Archive>
        inline void serialize( Archive& archive )
        {
            archive( CEREAL_OPTIONAL_NVP( jamId )   // these don't always seem to be present
                   , CEREAL_OPTIONAL_NVP( image )   //  ^ ^
            );
        }
    };

    struct Data
    {
        std::string             doc_id;
        uint64_t                action_timestamp;
        std::string             title;
        Riff                    rifff;
        std::vector< ResultStemDocument >
                                loops;
        std::string             image_url;
        bool                    image;

        template<class Archive>
        inline void serialize( Archive& archive )
        {
            archive( CEREAL_NVP( doc_id )
                   , CEREAL_NVP( action_timestamp )
                   , CEREAL_NVP( title )
                   , CEREAL_NVP( rifff )
                   , CEREAL_NVP( loops )
                   , CEREAL_NVP( image_url )
                   , CEREAL_NVP( image )
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
