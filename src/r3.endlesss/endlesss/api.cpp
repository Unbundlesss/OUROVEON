//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"

#include "base/hashing.h"
#include "spacetime/chronicle.h"

#include "endlesss/api.h"


namespace endlesss {
namespace api {

static constexpr auto cEndlesssDataDomain       = "data.endlesss.fm";
static constexpr auto cEndlesssAPIDomain        = "api.endlesss.fm";
static constexpr auto cMimeApplicationJson      = "application/json";

static constexpr auto cRegexLengthTypeMismatch  = "\"length\":\"([0-9]+)\"";

// ---------------------------------------------------------------------------------------------------------------------
NetConfiguration::NetConfiguration( const config::endlesss::rAPI& api, const fs::path& tempDir )
    : m_api( api )
    , m_tempDir( tempDir )
    , m_dataFixRegex_lengthTypeMismatch( cRegexLengthTypeMismatch )
{
}

// ---------------------------------------------------------------------------------------------------------------------
NetConfiguration::NetConfiguration( const config::endlesss::rAPI& api, const config::endlesss::Auth& auth, const fs::path& tempDir )
    : m_api( api )
    , m_auth( auth )
    , m_hasValidEndlesssAuth( true )
    , m_tempDir( tempDir )
    , m_dataFixRegex_lengthTypeMismatch( cRegexLengthTypeMismatch )
{
}

// ---------------------------------------------------------------------------------------------------------------------
std::string NetConfiguration::generateRandomLoadBalancerCookie() const
{
    constexpr uint64_t loadIndexMin = 1;
    constexpr uint64_t loadIndexMax = 7;

    // not a terribly glamorous RNG but it doesn't really need to be; we just want something stateless
    uint32_t rng = base::randomU64() & std::numeric_limits<uint32_t>::max();

    // boil the value down to the acceptable LB range, inclusive
    const uint64_t loadRange = ( loadIndexMax - ( loadIndexMin - 1 ) );
                         rng = ( rng * loadRange ) >> 32;

    return fmt::format( "LB=live{:02d}", loadIndexMin + (int32_t)rng );
}

// ---------------------------------------------------------------------------------------------------------------------
std::string NetConfiguration::getVerboseCaptureFilename( const char* context ) const
{
    const auto newIndex     = m_writeIndex++;
    const auto timestamp    = spacetime::createPrefixTimestampForFile();

    const auto resultPath   = m_tempDir / fmt::format( "{}.{}.{}.json", timestamp, newIndex, context );

    return resultPath.string();
}

// ---------------------------------------------------------------------------------------------------------------------
std::unique_ptr<httplib::SSLClient> createEndlesssHttpClient( const NetConfiguration& ncfg, const UserAgent ua )
{
    enum class AuthType
    {
        None,
        Basic,
        Bearer
    };

    using namespace std::literals::chrono_literals;

    const std::string loadBalance = ncfg.generateRandomLoadBalancerCookie();

    std::string requestDomain = cEndlesssDataDomain;
    AuthType addAuthentication = AuthType::Basic;
    const char* userAgent = "";
    switch ( ua )
    {
        case UserAgent::ClientService:  userAgent = ncfg.api().userAgentApp.c_str();
            break;
        default:
        case UserAgent::Couchbase:      userAgent = ncfg.api().userAgentDb.c_str();
            break;

        case UserAgent::WebWithoutAuth:
            addAuthentication = AuthType::None;
            userAgent = ncfg.api().userAgentWeb.c_str();
            requestDomain = cEndlesssAPIDomain;
            break;

        case UserAgent::WebWithAuth:
            addAuthentication = AuthType::Bearer;
            userAgent = ncfg.api().userAgentWeb.c_str();
            requestDomain = cEndlesssAPIDomain;
            break;
    }

    auto dataClient = std::make_unique< httplib::SSLClient >( requestDomain );

    dataClient->set_ca_cert_path( ncfg.api().certBundleRelative.c_str() );
    dataClient->enable_server_certificate_verification( true );

    // some of endlesss' servers are slow to respond
    dataClient->set_connection_timeout( 8s );
    dataClient->set_read_timeout( 8s );

    // most of the API calls expect Basic auth credentials
    if ( addAuthentication == AuthType::Basic )
    {
        dataClient->set_basic_auth( ncfg.auth().token.c_str(), ncfg.auth().password.c_str() );
    }
    // some of the web APIs can accept Bearer to access per-user private data (eg. private shared riffs)
    else if ( addAuthentication == AuthType::Bearer )
    {
        dataClient->set_bearer_token_auth( fmt::format( FMTX("{}:{}"), ncfg.auth().token.c_str(), ncfg.auth().password.c_str() ) );
    }

    dataClient->set_compress( true );

    if ( ncfg.api().debugVerboseNetLog )
    {
        dataClient->set_logger( []( const httplib::Request& req, const httplib::Response& rsp ) 
        {
            blog::api( "VERBOSE | REQ | {} {}", req.method, req.path );
            blog::api( "VERBOSE | RSP | {} {}", rsp.status, rsp.reason );
        });
    }
    dataClient->set_default_headers(
    {
        { "Host",               requestDomain          },
        { "User-Agent",         userAgent              },
        { "Cookie",             loadBalance            },
        { "Accept",             cMimeApplicationJson   },
        { "Accept-Encoding",    "gzip, deflate, br"    },
        { "Accept-Language",    "en-gb"                },
    });

    return dataClient;
}


// ---------------------------------------------------------------------------------------------------------------------
const char* getHttpLibErrorString( const httplib::Error err )
{
    switch ( err )
    {
        case httplib::Error::Success:                           return "Success";
        default:
        case httplib::Error::Unknown:                           return "Unknown";
        case httplib::Error::Connection:                        return "Connection";
        case httplib::Error::BindIPAddress:                     return "BindIPAddress";
        case httplib::Error::Read:                              return "Read";
        case httplib::Error::Write:                             return "Write";
        case httplib::Error::ExceedRedirectCount:               return "ExceedRedirectCount";
        case httplib::Error::Canceled:                          return "Canceled";
        case httplib::Error::SSLConnection:                     return "SSLConnection";
        case httplib::Error::SSLLoadingCerts:                   return "SSLLoadingCerts";
        case httplib::Error::SSLServerVerification:             return "SSLServerVerification";
        case httplib::Error::UnsupportedMultipartBoundaryChars: return "UnsupportedMultipartBoundaryChars";
        case httplib::Error::Compression:                       return "Compression";
    }
    return "";
}


// ---------------------------------------------------------------------------------------------------------------------
bool JamProfile::fetch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID )
{
    auto res = createEndlesssHttpClient( ncfg, UserAgent::Couchbase )->Get(
        fmt::format( "/user_appdata${}/Profile", jamDatabaseID ).c_str() );

    return deserializeJson< JamProfile >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, jamDatabaseID ) );
}

// ---------------------------------------------------------------------------------------------------------------------
bool JamChanges::fetch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID )
{
    auto res = createEndlesssHttpClient( ncfg, UserAgent::Couchbase )->Post(
        fmt::format( "/user_appdata${}/_changes?descending=true&limit=1", jamDatabaseID ).c_str(),
        keyBody,
        cMimeApplicationJson );

    return deserializeJson< JamChanges >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, jamDatabaseID ) );
}

// ---------------------------------------------------------------------------------------------------------------------
bool JamChanges::fetchSince( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID, const std::string& seqSince )
{
    auto res = createEndlesssHttpClient( ncfg, UserAgent::Couchbase )->Post(
        fmt::format( "/user_appdata${}/_changes?since={}", jamDatabaseID, seqSince ).c_str(),
        keyBody,
        cMimeApplicationJson );

    return deserializeJson< JamChanges >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, jamDatabaseID ) );
}

// ---------------------------------------------------------------------------------------------------------------------
bool RiffDetails::fetch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID, const endlesss::types::RiffCouchID& riffDocumentID )
{
    // manually form a json body with the single document filter
    auto keyBody = fmt::format( R"({{ "keys" : [ "{}" ]}})", riffDocumentID );

    // post the query, we expect a single document stream back
    auto res = createEndlesssHttpClient( ncfg, UserAgent::Couchbase )->Post(
        fmt::format( "/user_appdata${}/_all_docs?include_docs=true", jamDatabaseID ).c_str(),
        keyBody,
        cMimeApplicationJson );

    // #HDD TODO replace with new NetConfiguration stuff
#if 0
    FILE* f;
    fopen_s(&f, "E:\\Audio\\Ouroveon\\riff.json", "wt");
    fprintf(f, "%s\n", res->body.c_str());
    fclose(f);
#endif

    return deserializeJson< RiffDetails >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, jamDatabaseID ) );
}

// ---------------------------------------------------------------------------------------------------------------------
bool RiffDetails::fetchBatch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID, const endlesss::types::RiffCouchIDs& riffDocumentIDs )
{
    // expand all riff ids into a json array
    auto keyBody = fmt::format( R"({{ "keys" : [ "{}" ]}})", fmt::join( riffDocumentIDs, R"(", ")" ) );

    // post the query, we expect multiple rows of documents in return
    auto res = createEndlesssHttpClient( ncfg, UserAgent::Couchbase )->Post(
        fmt::format( "/user_appdata${}/_all_docs?include_docs=true", jamDatabaseID ).c_str(),
        keyBody,
        cMimeApplicationJson );

    return deserializeJson< RiffDetails >( ncfg, res, *this, fmt::format("{}( {} )", __FUNCTION__, jamDatabaseID ) );
}

// ---------------------------------------------------------------------------------------------------------------------
bool StemTypeCheck::fetchBatch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID, const endlesss::types::StemCouchIDs& stemDocumentIDs )
{
    // expand all stem ids into a json array
    auto keyBody = fmt::format( R"({{ "keys" : [ "{}" ]}})", fmt::join( stemDocumentIDs, R"(", ")" ) );

    // fetch all the stem data docs but only do a very minimal parse for 'type' fields
    auto res = createEndlesssHttpClient( ncfg, UserAgent::Couchbase )->Post(
        fmt::format( "/user_appdata${}/_all_docs?include_docs=true", jamDatabaseID ).c_str(),
        keyBody,
        cMimeApplicationJson );

    return deserializeJson< StemTypeCheck >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, jamDatabaseID ) );
}

// ---------------------------------------------------------------------------------------------------------------------
bool StemDetails::fetchBatch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID, const endlesss::types::StemCouchIDs& stemDocumentIDs )
{
    // expand all stem ids into a json array
    auto keyBody = fmt::format( R"({{ "keys" : [ "{}" ]}})", fmt::join( stemDocumentIDs, R"(", ")" ) );

    // fetch data about all the stems in bulk
    auto res = createEndlesssHttpClient( ncfg, UserAgent::Couchbase )->Post(
        fmt::format( "/user_appdata${}/_all_docs?include_docs=true", jamDatabaseID ).c_str(),
        keyBody,
        cMimeApplicationJson );

    // #HDD TODO replace with new NetConfiguration stuff
#if 0
    FILE* f;
    fopen_s(&f, "F:\\stems.json", "wt");
    fprintf(f, "%s\n", res->body.c_str());
    fclose(f);
#endif

    return deserializeJson< StemDetails >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, jamDatabaseID ) );
}

// ---------------------------------------------------------------------------------------------------------------------
bool CurrentJoinInJams::fetch( const NetConfiguration& ncfg )
{
    auto res = createEndlesssHttpClient( ncfg, UserAgent::ClientService )->Get( "/app_client_config/bands:joinable" );

    return deserializeJson< CurrentJoinInJams >( ncfg, res, *this, __FUNCTION__ );
}

// ---------------------------------------------------------------------------------------------------------------------
bool CurrentCollectibleJams::fetch( const NetConfiguration& ncfg, int32_t pageNo )
{
    // pageSize 4 matches what the web site currently uses
    const auto requestUrl = fmt::format( FMTX( "/marketplace/collectible-jams?pageSize=4&pageNo={}" ), pageNo );

    auto client = createEndlesssHttpClient( ncfg, UserAgent::WebWithoutAuth );

    // this endpoint is slow and prone to failure. give it a second try if it fails immediately.
    auto res = client->Get( requestUrl);
    if ( res.error() != httplib::Error::Success )
    {
        res = client->Get( requestUrl );
    }

    return deserializeJson< CurrentCollectibleJams >( ncfg, res, *this, __FUNCTION__ );
}


// ---------------------------------------------------------------------------------------------------------------------
// NB: could change this to data.endlesss.fm/user_appdata${user}/_find
//
bool SubscribedJams::fetch( const NetConfiguration& ncfg, const std::string& userName )
{
    auto res = createEndlesssHttpClient( ncfg, UserAgent::ClientService )->Get(
        fmt::format( "/user_appdata${}/_design/membership/_view/getMembership", userName ).c_str() );

    return deserializeJson< SubscribedJams >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, userName ) );
}

// ---------------------------------------------------------------------------------------------------------------------
bool SharedRiffsByUser::fetch( const NetConfiguration& ncfg, const std::string& userName, int32_t count, int32_t offset )
{
    const auto requestUrl = fmt::format( FMTX( "/api/v3/feed/shared_by/{}?size={}&from={}" ), userName, count, offset );

    auto res = createEndlesssHttpClient( ncfg, UserAgent::WebWithAuth )->Get( requestUrl );

    return deserializeJson< SharedRiffsByUser >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, userName ) );
}

} // namespace remote
} // namespace endlesss
