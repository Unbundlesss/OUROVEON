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
const char* getHttpLibErrorString( const httplib::Error err )
{
    switch ( err )
    {
    case httplib::Error::Success:                           return "Success";
    default:
    case httplib::Error::Unknown:                           return "Unknown";
    case httplib::Error::Connection:                        return "Connection";
    case httplib::Error::BindIPAddress:                     return "Bind IPAddress";
    case httplib::Error::Read:                              return "Read";
    case httplib::Error::Write:                             return "Write";
    case httplib::Error::ExceedRedirectCount:               return "Exceed Redirect Count";
    case httplib::Error::Canceled:                          return "Canceled";
    case httplib::Error::SSLConnection:                     return "SSL Connection";
    case httplib::Error::SSLLoadingCerts:                   return "SSL Loading Certs";
    case httplib::Error::SSLServerVerification:             return "SSL Server Verification";
    case httplib::Error::UnsupportedMultipartBoundaryChars: return "Unsupported Multipart Boundary Chars";
    case httplib::Error::Compression:                       return "Compression";
    }
    return "";
}

// ---------------------------------------------------------------------------------------------------------------------
void NetConfiguration::initWithoutAuthentication(
    base::EventBusClient eventBusClient,
    const config::endlesss::rAPI& api )
{
    m_eventBusClient = std::move( eventBusClient );

    // downgrade would be unusual
    if ( m_access == Access::Authenticated )
    {
        blog::error::api( FMTX("downgrading network configuration from authenticated -> public") );    // technically just a warning
        m_auth = {};
    }

    m_access = Access::Public;
    m_api = api;

    postInit();
}

// ---------------------------------------------------------------------------------------------------------------------
void NetConfiguration::initWithAuthentication(
    base::EventBusClient eventBusClient,
    const config::endlesss::rAPI& api,
    const config::endlesss::Auth& auth )
{
    m_eventBusClient = std::move( eventBusClient );

    m_access = Access::Authenticated;
    m_api = api;
    m_auth = auth;

    ABSL_ASSERT( !m_auth.token.empty() );

    postInit();
}

// ---------------------------------------------------------------------------------------------------------------------
void NetConfiguration::postInit()
{
    // preflights
    ABSL_ASSERT( !m_api.certBundleRelative.empty() );

    const auto nameForAccess = [this]()
        {
            switch ( m_access )
            {
            default:
            case Access::None:          return "None";
            case Access::Public:        return "Public";
            case Access::Authenticated: return "Authenticated";
            }
        };

    blog::api( FMTX( "NetConfiguration::postInit() with Access::{} user:{}" ), nameForAccess(), m_auth.user_id );

    // log out httplib features we've compiled in, for our own references' sake
    blog::api( FMTX( "[httplib] compression {}, engines compiled : {}{}" ),
        m_api.connectionCompressionSupport ? "enabled" : "disabled",
#ifdef CPPHTTPLIB_BROTLI_SUPPORT
        "[brotli] ",
#else
        "",
#endif
#ifdef CPPHTTPLIB_ZLIB_SUPPORT
        "[gzip / deflate] "
#else
        ""
#endif
    );
}


// ---------------------------------------------------------------------------------------------------------------------
void NetConfiguration::enableFullNetworkDiagnostics()
{
    blog::app( FMTX( "NetConfiguration::enableFullNetworkDiagnostics()" ) );

    m_api.debugVerboseNetDataCapture = true;
    m_api.debugVerboseNetLog = true;
}

// ---------------------------------------------------------------------------------------------------------------------
void NetConfiguration::enableStemVersionBypass()
{
    blog::app( FMTX( "NetConfiguration::enableStemVersionBypass()" ) );

    m_api.allowStemsWithoutVersionData = true;
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
// there was an
httplib::Result NetConfiguration::attempt( const std::function<httplib::Result()>& operation ) const
{
    int32_t retries = getRequestRetries();

    httplib::Result opResult = operation();

    // on failure, circle until we run out of tries or the call succeeds
    uint32_t delayInMs = 250;
    while ( (opResult == nullptr || opResult.error() != httplib::Error::Success) && retries > 0 )
    {
        {
            metricsActivityFailure();

            std::this_thread::sleep_for( std::chrono::milliseconds( delayInMs ) );
            opResult = operation();
        }
        retries--;
        delayInMs = std::min( delayInMs + 150, 1000u );   // arbitrary; push up delay up to 1s each. just break up the re-send cycle a bit
    }

    // make attempt to log out a failure code if we bungled it
    if ( opResult != nullptr && opResult.error() != httplib::Error::Success )
    {
        blog::error::api( FMTX( "network request failed with error state '{}'" ), getHttpLibErrorString( opResult.error() ) );
    }

    return opResult;
}


// ---------------------------------------------------------------------------------------------------------------------
// used by all API calls to create a primed http client instance; seeded with the correct headers, authentication, SSL etc
// 
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

    // blanket the timeouts all the same
    {
        const auto timeoutSec = ncfg.getRequestTimeout();
        dataClient->set_connection_timeout( timeoutSec );
        dataClient->set_read_timeout( timeoutSec );
        dataClient->set_write_timeout( timeoutSec );
    }

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

    dataClient->set_compress( ncfg.api().connectionCompressionSupport );
    dataClient->set_decompress( ncfg.api().connectionCompressionSupport );

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

    // log network traffic
    ncfg.metricsActivitySend();

    return dataClient;
}



// ---------------------------------------------------------------------------------------------------------------------
bool JamProfile::fetch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID )
{
    auto client = createEndlesssHttpClient( ncfg, UserAgent::Couchbase );

    auto res = ncfg.attempt( [&]() -> httplib::Result {
        return client->Get( fmt::format( "/user_appdata${}/Profile", jamDatabaseID ).c_str() );
        });

    return deserializeJson< JamProfile >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, jamDatabaseID ), "jam_profile" );
}

// ---------------------------------------------------------------------------------------------------------------------
bool JamChanges::fetch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID )
{
    auto client = createEndlesssHttpClient( ncfg, UserAgent::Couchbase );

    auto res = ncfg.attempt( [&]() -> httplib::Result {
        return client->Post(
            fmt::format( "/user_appdata${}/_changes?descending=true&limit=1", jamDatabaseID ).c_str(),
            keyBody,
            cMimeApplicationJson );
        });

    return deserializeJson< JamChanges >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, jamDatabaseID ), "jam_changes" );
}

// ---------------------------------------------------------------------------------------------------------------------
bool JamChanges::fetchSince( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID, const std::string& seqSince )
{
    auto client = createEndlesssHttpClient( ncfg, UserAgent::Couchbase );

    auto res = ncfg.attempt( [&]() -> httplib::Result {
        return client->Post(
            fmt::format( "/user_appdata${}/_changes?since={}", jamDatabaseID, seqSince ).c_str(),
            keyBody,
            cMimeApplicationJson );
        });

    return deserializeJson< JamChanges >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, jamDatabaseID ), "jam_changes_since" );
}

// ---------------------------------------------------------------------------------------------------------------------
bool JamLatestState::fetch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID )
{
    auto client = createEndlesssHttpClient( ncfg, UserAgent::Couchbase );

    auto res = ncfg.attempt( [&]() -> httplib::Result {
        return client->Get(
            fmt::format( "/user_appdata${}/_design/types/_view/rifffLoopsByCreateTime?descending=true&limit=1", jamDatabaseID ).c_str() );
        });

    return deserializeJson< JamLatestState >( ncfg, res, *this, __FUNCTION__, "jam_latest_state" );
}

// ---------------------------------------------------------------------------------------------------------------------
bool JamFullSnapshot::fetch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID )
{
    auto client = createEndlesssHttpClient( ncfg, UserAgent::Couchbase );

    auto res = ncfg.attempt( [&]() -> httplib::Result {
        return client->Get(
            fmt::format( "/user_appdata${}/_design/types/_view/rifffLoopsByCreateTime?descending=true", jamDatabaseID ).c_str() );
        });

    return deserializeJson< JamFullSnapshot >( ncfg, res, *this, __FUNCTION__, "jam_full_snapshot" );
}

// ---------------------------------------------------------------------------------------------------------------------
bool JamRiffCount::fetch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID )
{
    auto client = createEndlesssHttpClient( ncfg, UserAgent::Couchbase );

    auto res = ncfg.attempt( [&]() -> httplib::Result {
        return client->Get( fmt::format( "/user_appdata${}/_design/types/_view/rifffsByCreateTime", jamDatabaseID ).c_str() );
        } );

    return deserializeJson< JamRiffCount >( ncfg, res, *this, __FUNCTION__, "jam_riff_count" );
}

// ---------------------------------------------------------------------------------------------------------------------
bool RiffDetails::fetch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID, const endlesss::types::RiffCouchID& riffDocumentID )
{
    // manually form a json body with the single document filter
    auto keyBody = fmt::format( R"({{ "keys" : [ "{}" ]}})", riffDocumentID );

    auto client = createEndlesssHttpClient( ncfg, UserAgent::Couchbase );

    // post the query, we expect a single document stream back
    auto res = ncfg.attempt( [&]() -> httplib::Result {
        return client->Post(
            fmt::format( "/user_appdata${}/_all_docs?include_docs=true", jamDatabaseID ).c_str(),
            keyBody,
            cMimeApplicationJson );
        });

    return deserializeJson< RiffDetails >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, jamDatabaseID ), "riff_details" );
}

// ---------------------------------------------------------------------------------------------------------------------
bool RiffDetails::fetchBatch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID, const endlesss::types::RiffCouchIDs& riffDocumentIDs )
{
    // expand all riff ids into a json array
    auto keyBody = fmt::format( R"({{ "keys" : [ "{}" ]}})", fmt::join( riffDocumentIDs, R"(", ")" ) );

    auto client = createEndlesssHttpClient( ncfg, UserAgent::Couchbase );

    // post the query, we expect multiple rows of documents in return
    auto res = ncfg.attempt( [&]() -> httplib::Result {
        return client->Post(
            fmt::format( "/user_appdata${}/_all_docs?include_docs=true", jamDatabaseID ).c_str(),
            keyBody,
            cMimeApplicationJson );
        });

    return deserializeJson< RiffDetails >( ncfg, res, *this, fmt::format("{}( {} )", __FUNCTION__, jamDatabaseID ), "riff_details_batch" );
}

// ---------------------------------------------------------------------------------------------------------------------
bool StemTypeCheck::fetchBatch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID, const endlesss::types::StemCouchIDs& stemDocumentIDs )
{
    // expand all stem ids into a json array
    auto keyBody = fmt::format( R"({{ "keys" : [ "{}" ]}})", fmt::join( stemDocumentIDs, R"(", ")" ) );

    auto client = createEndlesssHttpClient( ncfg, UserAgent::Couchbase );

    // fetch all the stem data docs but only do a very minimal parse for 'type' fields
    auto res = ncfg.attempt( [&]() -> httplib::Result {
        return client->Post(
            fmt::format( "/user_appdata${}/_all_docs?include_docs=true", jamDatabaseID ).c_str(),
            keyBody,
            cMimeApplicationJson );
        });

    return deserializeJson< StemTypeCheck >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, jamDatabaseID ), "stem_type_check_batch" );
}

// ---------------------------------------------------------------------------------------------------------------------
bool StemDetails::fetchBatch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID, const endlesss::types::StemCouchIDs& stemDocumentIDs )
{
    // expand all stem ids into a json array
    auto keyBody = fmt::format( R"({{ "keys" : [ "{}" ]}})", fmt::join( stemDocumentIDs, R"(", ")" ) );

    auto client = createEndlesssHttpClient( ncfg, UserAgent::Couchbase );

    // fetch data about all the stems in bulk
    auto res = ncfg.attempt( [&]() -> httplib::Result {
        return client->Post(
            fmt::format( "/user_appdata${}/_all_docs?include_docs=true", jamDatabaseID ).c_str(),
            keyBody,
            cMimeApplicationJson );
        });

    return deserializeJson< StemDetails >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, jamDatabaseID ), "stem_details_batch" );
}

// ---------------------------------------------------------------------------------------------------------------------
bool CurrentJoinInJams::fetch( const NetConfiguration& ncfg )
{
    auto client = createEndlesssHttpClient( ncfg, UserAgent::ClientService );

    auto res = ncfg.attempt( [&]() -> httplib::Result {
        return client->Get( "/app_client_config/bands:joinable" );
        });

    return deserializeJson< CurrentJoinInJams >( ncfg, res, *this, __FUNCTION__, "current_join_in_jams" );
}

// ---------------------------------------------------------------------------------------------------------------------
bool CurrentCollectibleJams::fetch( const NetConfiguration& ncfg, int32_t pageNo )
{
    // pageSize 4 matches what the web site currently uses; this endpoint is notoriously slow and unreasonable at anything higher
    // .. which is pretty annoying, given there are like 40+ pages of these now at this pagination size
    const auto requestUrl = fmt::format( FMTX( "/marketplace/collectible-jams?pageSize=4&pageNo={}" ), pageNo );

    auto client = createEndlesssHttpClient( ncfg, UserAgent::WebWithoutAuth );

    auto res = ncfg.attempt( [&]() -> httplib::Result {
            return client->Get( requestUrl );
        });

    return deserializeJson< CurrentCollectibleJams >( ncfg, res, *this, __FUNCTION__, "current_collectible_jams" );
}

// ---------------------------------------------------------------------------------------------------------------------
// NB: could change this to data.endlesss.fm/user_appdata${user}/_find
//
bool SubscribedJams::fetch( const NetConfiguration& ncfg, const std::string& userName )
{
    auto client = createEndlesssHttpClient( ncfg, UserAgent::ClientService );

    auto res = ncfg.attempt( [&]() -> httplib::Result {
        return client->Get( fmt::format( "/user_appdata${}/_design/membership/_view/getMembership", userName ).c_str() );
        } );

    return deserializeJson< SubscribedJams >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, userName ), "subscribed_jams" );
}

// ---------------------------------------------------------------------------------------------------------------------
bool BandPermalinkMeta::fetch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID )
{
    auto client = createEndlesssHttpClient( ncfg, UserAgent::WebWithoutAuth );

    auto res = ncfg.attempt( [&]() -> httplib::Result {
        return client->Get( fmt::format( "/api/band/{}/permalink", jamDatabaseID ).c_str() );
        });

    return deserializeJson< BandPermalinkMeta >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, jamDatabaseID ), "band_permalink_meta" );
}

// ---------------------------------------------------------------------------------------------------------------------
bool BandNameFromExtendedID::fetch( const NetConfiguration& ncfg, const std::string& jamLongID )
{
    auto client = createEndlesssHttpClient( ncfg, UserAgent::WebWithoutAuth );

    auto res = ncfg.attempt( [&]() -> httplib::Result {
        return client->Get( fmt::format( "/jam/{}/rifffs?pageNo=0&pageSize=1", jamLongID ).c_str() );
        });

    return deserializeJson< BandNameFromExtendedID >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, jamLongID ), "band_name_from_extid" );
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
    // can use either auth or not, depending on what we have in the configuration; no-auth just means you won't see your own private stuff
    // as everything else is available via public calls
    const UserAgent srUA = ncfg.hasAccess( NetConfiguration::Access::Authenticated ) ? UserAgent::WebWithAuth : UserAgent::WebWithoutAuth;

    auto client = createEndlesssHttpClient( ncfg, srUA );

    auto res = ncfg.attempt( [&]() -> httplib::Result {
        return client->Get( requestUrl );
        });

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
