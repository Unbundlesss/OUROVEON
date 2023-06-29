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


// ---------------------------------------------------------------------------------------------------------------------
void NetConfiguration::initWithoutAuthentication( const config::endlesss::rAPI& api )
{
    // downgrade would be unusual
    if ( m_access == Access::Authenticated )
    {
        blog::error::api( "downgrading network configuration from authenticated -> public" );    // technically just a warning
        m_auth = {};
    }

    m_access = Access::Public;
    m_api = api;

    ABSL_ASSERT( !m_api.certBundleRelative.empty() );
}

// ---------------------------------------------------------------------------------------------------------------------
void NetConfiguration::initWithAuthentication( const config::endlesss::rAPI& api, const config::endlesss::Auth& auth )
{
    m_access = Access::Authenticated;
    m_api = api;
    m_auth = auth;

    ABSL_ASSERT( !m_api.certBundleRelative.empty() );
    ABSL_ASSERT( !m_auth.token.empty() );
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
std::string NetConfiguration::getVerboseCaptureFilename( std::string_view context ) const
{
    // shouldn't be called before setting up a debug output path
    const bool bNoOutputDirSet = m_verboseOutputDir.empty();
    ABSL_ASSERT( bNoOutputDirSet == false );
    if ( bNoOutputDirSet )
        return "";

    const auto newIndex     = m_writeIndex++;
    const auto timestamp    = spacetime::createPrefixTimestampForFile();

    const auto resultPath   = m_verboseOutputDir / fmt::format( "debug.{}.{}.{}.json", timestamp, newIndex, context );

    return resultPath.string();
}

// ---------------------------------------------------------------------------------------------------------------------
std::unique_ptr<httplib::SSLClient> createEndlesssHttpClient( const NetConfiguration& ncfg, const UserAgent ua )
{
    using namespace std::literals::chrono_literals;

    enum class AuthHeaders
    {
        None,
        Basic,
        Bearer
    };

    const std::string loadBalance = ncfg.generateRandomLoadBalancerCookie();

    std::string requestDomain = cEndlesssDataDomain;
    AuthHeaders authHeaders = AuthHeaders::Basic;
    const char* userAgent = "";
    switch ( ua )
    {
        case UserAgent::ClientService:
            userAgent = ncfg.api().userAgentApp.c_str();
            break;

        default:
        case UserAgent::Couchbase:
            userAgent = ncfg.api().userAgentDb.c_str();
            break;

        case UserAgent::WebWithoutAuth:
            authHeaders = AuthHeaders::None;
            userAgent = ncfg.api().userAgentWeb.c_str();
            requestDomain = cEndlesssAPIDomain;
            break;

        case UserAgent::WebWithAuth:
            authHeaders = AuthHeaders::Bearer;
            userAgent = ncfg.api().userAgentWeb.c_str();
            requestDomain = cEndlesssAPIDomain;
            break;
    }

    auto dataClient = std::make_unique< httplib::SSLClient >( requestDomain );

    dataClient->set_ca_cert_path( ncfg.api().certBundleRelative.c_str() );
    dataClient->enable_server_certificate_verification( true );

    // some of endlesss' servers are slow to respond
    const auto timeoutSec = 1s * ncfg.api().networkTimeoutInSeconds;
    dataClient->set_connection_timeout( timeoutSec );
    dataClient->set_read_timeout( timeoutSec );

    // most of the API calls expect Basic auth credentials
    if ( authHeaders == AuthHeaders::Basic )
    {
        dataClient->set_basic_auth( ncfg.auth().token.c_str(), ncfg.auth().password.c_str() );
    }
    // some of the web APIs can accept Bearer to access per-user private data (eg. private shared riffs), formed out of token:password
    else if ( authHeaders == AuthHeaders::Bearer )
    {
        dataClient->set_bearer_token_auth( fmt::format( FMTX("{}:{}"), ncfg.auth().token, ncfg.auth().password ) );
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

    return deserializeJson< JamProfile >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, jamDatabaseID ), "jam_profile" );
}

// ---------------------------------------------------------------------------------------------------------------------
bool JamChanges::fetch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID )
{
    auto res = createEndlesssHttpClient( ncfg, UserAgent::Couchbase )->Post(
        fmt::format( "/user_appdata${}/_changes?descending=true&limit=1", jamDatabaseID ).c_str(),
        keyBody,
        cMimeApplicationJson );

    return deserializeJson< JamChanges >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, jamDatabaseID ), "jam_changes" );
}

// ---------------------------------------------------------------------------------------------------------------------
bool JamChanges::fetchSince( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID, const std::string& seqSince )
{
    auto res = createEndlesssHttpClient( ncfg, UserAgent::Couchbase )->Post(
        fmt::format( "/user_appdata${}/_changes?since={}", jamDatabaseID, seqSince ).c_str(),
        keyBody,
        cMimeApplicationJson );

    return deserializeJson< JamChanges >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, jamDatabaseID ), "jam_changes_since" );
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

    return deserializeJson< RiffDetails >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, jamDatabaseID ), "riff_details" );
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

    return deserializeJson< RiffDetails >( ncfg, res, *this, fmt::format("{}( {} )", __FUNCTION__, jamDatabaseID ), "riff_details_batch" );
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

    return deserializeJson< StemTypeCheck >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, jamDatabaseID ), "stem_type_check_batch" );
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

    return deserializeJson< StemDetails >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, jamDatabaseID ), "stem_details_batch" );
}

// ---------------------------------------------------------------------------------------------------------------------
bool CurrentJoinInJams::fetch( const NetConfiguration& ncfg )
{
    auto res = createEndlesssHttpClient( ncfg, UserAgent::ClientService )->Get( "/app_client_config/bands:joinable" );

    return deserializeJson< CurrentJoinInJams >( ncfg, res, *this, __FUNCTION__, "current_join_in_jams" );
}

// ---------------------------------------------------------------------------------------------------------------------
bool CurrentCollectibleJams::fetch( const NetConfiguration& ncfg, int32_t pageNo )
{
    // pageSize 4 matches what the web site currently uses; this endpoint is notoriously slow and unreasonable at anything higher
    // .. which is pretty annoying, given there are like 40+ pages of these now at this pagination size
    const auto requestUrl = fmt::format( FMTX( "/marketplace/collectible-jams?pageSize=4&pageNo={}" ), pageNo );

    auto client = createEndlesssHttpClient( ncfg, UserAgent::WebWithoutAuth );

    // this endpoint is slow and prone to failure. give it a second try if it fails immediately.
    auto res = client->Get( requestUrl);
    if ( res.error() != httplib::Error::Success )
    {
        res = client->Get( requestUrl );
    }

    return deserializeJson< CurrentCollectibleJams >( ncfg, res, *this, __FUNCTION__, "current_collectible_jams" );
}

// ---------------------------------------------------------------------------------------------------------------------
// NB: could change this to data.endlesss.fm/user_appdata${user}/_find
//
bool SubscribedJams::fetch( const NetConfiguration& ncfg, const std::string& userName )
{
    auto res = createEndlesssHttpClient( ncfg, UserAgent::ClientService )->Get(
        fmt::format( "/user_appdata${}/_design/membership/_view/getMembership", userName ).c_str() );

    return deserializeJson< SubscribedJams >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, userName ), "subscribed_jams" );
}

// ---------------------------------------------------------------------------------------------------------------------
bool BandPermalinkMeta::fetch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID )
{
    auto res = createEndlesssHttpClient( ncfg, UserAgent::WebWithoutAuth )->Get(
        fmt::format( "/api/band/{}/permalink", jamDatabaseID ).c_str() );

    return deserializeJson< BandPermalinkMeta >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, jamDatabaseID ), "band_permalink_meta" );
}

// ---------------------------------------------------------------------------------------------------------------------
bool SharedRiffsByUser::fetch( const NetConfiguration& ncfg, const std::string& userName, int32_t count, int32_t offset )
{
    return commonRequest( ncfg, fmt::format( FMTX( "/api/v3/feed/shared_by/{}?size={}&from={}" ), userName, count, offset ), fmt::format( "{}( {} )", __FUNCTION__, userName ) );
}

// ---------------------------------------------------------------------------------------------------------------------
bool SharedRiffsByUser::fetchSpecific( const NetConfiguration& ncfg, const endlesss::types::SharedRiffCouchID& sharedRiffID )
{
    return commonRequest( ncfg, fmt::format( FMTX( "/api/v3/feed/shared_rifff/{}" ), sharedRiffID ), fmt::format( "{}( {} )", __FUNCTION__, sharedRiffID ) );
}

// ---------------------------------------------------------------------------------------------------------------------
bool SharedRiffsByUser::commonRequest( const NetConfiguration& ncfg, const std::string& requestUrl, const std::string& requestContext )
{
    auto res = createEndlesssHttpClient( ncfg, UserAgent::WebWithAuth )->Get( requestUrl );

    #define CHECK_CHAR( _idx, _chr ) if ( bodyStream[i + _idx] == _chr )

    /* this body processor is a grotty but quick hack to deal with outlier results from Endlesss where we get
       a loops array with null elements in it, eg

       "loops":
            [
                null,
                null,
                null,
                null,
                null,
                null,
                {
                    "_id": "1641d000ef9911ed9000d1062b20bdd7",
                    "_rev": "1-d61e44c90da6ca1ef97bdc242c589f0a",
                    "cdn_attachments":
                    {

        .. which Cereal does not like. this processor looks for the start of "loops", switches to a replacement mode that should
        cancel when it hits the loops array's terminating ']' - this replacement mode looks for the phrase "null," and replaces it with whitespace
        meaning that the parser can just get to the actual meat of the array and ignore the gaps
     */

    return deserializeJson< SharedRiffsByUser >( ncfg, res, *this, requestContext, "shared_riffs_by_user", []( std::string& bodyText )
        {
            const std::size_t bodySize = bodyText.size();
            if ( bodySize <= 16 )
                return;

            bool bReplacementMode = false;
            int32_t replacementScope = 0;

            char* bodyStream = &bodyText[0];
            for ( auto i = 0; i < bodySize - 16; i++ )
            {
                CHECK_CHAR( 0, '\"')
                CHECK_CHAR( 1, 'l')
                CHECK_CHAR( 2, 'o')
                CHECK_CHAR( 3, 'o')
                CHECK_CHAR( 4, 'p')
                CHECK_CHAR( 5, 's')
                CHECK_CHAR( 6, '\"' )
                {
                    bReplacementMode = true;
                    replacementScope = 0;
                    
                    // eat until the opening '[', treat this as scope level 0
                    while ( bodyStream[i] != '[' )
                    {
                        i++;
                    }

                    continue;
                }

                if ( bReplacementMode )
                {
                    // support things like nested
                    // "colourHistory":
                    // [
                    //     "ff1de3c0",
                    //     "fff07b39"
                    // ] ,
                    // by tracking in/out of [ ] scopes
                    CHECK_CHAR( 0, '[' )
                    {
                        replacementScope++;
                    }
                    CHECK_CHAR( 0, ']' )
                    {
                        replacementScope--;
                        // if we go under 0 that means we're leaving the "loops" [ scope
                        if ( replacementScope < 0 )
                        {
                            bReplacementMode = false;
                        }
                    }
                }

                // remove "null," if we're inside "loops" [ ]
                if ( bReplacementMode )
                {
                    CHECK_CHAR( 0, 'n' )
                    CHECK_CHAR( 1, 'u' )
                    CHECK_CHAR( 2, 'l' )
                    CHECK_CHAR( 3, 'l' )
                    CHECK_CHAR( 4, ',' )
                    {
                        bodyStream[i + 0] = ' ';
                        bodyStream[i + 1] = ' ';
                        bodyStream[i + 2] = ' ';
                        bodyStream[i + 3] = ' ';
                        bodyStream[i + 4] = ' ';
                        i += 4;
                        continue;
                    }
                }
            }
        });

    #undef CHECK_CHAR
}

} // namespace remote
} // namespace endlesss
