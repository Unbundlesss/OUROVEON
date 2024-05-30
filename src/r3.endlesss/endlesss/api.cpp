//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#include "pch.h"

#include "base/text.h"
#include "base/hashing.h"
#include "data/uuid.h"
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
void NetConfiguration::enableLastMinuteQuirkFixes()
{
    blog::app( FMTX( "NetConfiguration::enableLastMinuteQuirkFixes()" ) );

    m_api.debugLastMinuteQuirkFixes = true;
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
// there was an ...
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
endlesss::types::JamCouchID NetConfiguration::checkAndSanitizeJamCouchID( const endlesss::types::JamCouchID& jamID ) const
{
    // this shouldn't be possible, so check on it JIC
    ABSL_ASSERT( !jamID.empty() );
    if ( jamID.empty() )
        return jamID;

    // check if this is a personal jam request; if so, we need to escape the username for couchbase
    std::string jamNameSanitised = jamID.value();
    if ( jamNameSanitised.rfind( "band", 0 ) != 0 ) // is this couch ID beginning with band#### .. if so, we don't have to worry about it
    {
        // but otherwise this is a personal jam, so - for usernames with hyphens, we have to escape them for use with couchbase
        // there are other symbols we should probably escape but I don't know of any users using them or if they are even allowed during user sign-up
        const std::string jamNameSanitisedCb = std::regex_replace( jamNameSanitised, std::regex( "-" ), "(2d)" );

        blog::debug::api( FMTX( "checkAndSanitizeJamCouchID() username '{}' -> '{}'" ), jamNameSanitised, jamNameSanitisedCb );

        return endlesss::types::JamCouchID( jamNameSanitisedCb );
    }
    else
    {
        return jamID;   // no changes
    }
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
    const endlesss::types::JamCouchID& jamDatabaseID_Sanitised = ncfg.checkAndSanitizeJamCouchID( jamDatabaseID );

    auto client = createEndlesssHttpClient( ncfg, UserAgent::Couchbase );

    auto res = ncfg.attempt( [&]() -> httplib::Result {
        return client->Get( fmt::format( "/user_appdata${}/Profile", jamDatabaseID_Sanitised ).c_str() );
        });

    return deserializeJson< JamProfile >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, jamDatabaseID_Sanitised ), "jam_profile" );
}

// ---------------------------------------------------------------------------------------------------------------------
bool JamChanges::fetch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID )
{
    const endlesss::types::JamCouchID& jamDatabaseID_Sanitised = ncfg.checkAndSanitizeJamCouchID( jamDatabaseID );

    auto client = createEndlesssHttpClient( ncfg, UserAgent::Couchbase );

    auto res = ncfg.attempt( [&]() -> httplib::Result {
        return client->Post(
            fmt::format( "/user_appdata${}/_changes?descending=true&limit=1", jamDatabaseID_Sanitised ).c_str(),
            keyBody,
            cMimeApplicationJson );
        });

    return deserializeJson< JamChanges >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, jamDatabaseID_Sanitised ), "jam_changes" );
}

// ---------------------------------------------------------------------------------------------------------------------
bool JamChanges::fetchSince( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID, const std::string& seqSince )
{
    const endlesss::types::JamCouchID& jamDatabaseID_Sanitised = ncfg.checkAndSanitizeJamCouchID( jamDatabaseID );

    auto client = createEndlesssHttpClient( ncfg, UserAgent::Couchbase );

    auto res = ncfg.attempt( [&]() -> httplib::Result {
        return client->Post(
            fmt::format( "/user_appdata${}/_changes?since={}", jamDatabaseID_Sanitised, seqSince ).c_str(),
            keyBody,
            cMimeApplicationJson );
        });

    return deserializeJson< JamChanges >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, jamDatabaseID_Sanitised ), "jam_changes_since" );
}

// ---------------------------------------------------------------------------------------------------------------------
bool JamLatestState::fetch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID )
{
    const endlesss::types::JamCouchID& jamDatabaseID_Sanitised = ncfg.checkAndSanitizeJamCouchID( jamDatabaseID );

    auto client = createEndlesssHttpClient( ncfg, UserAgent::Couchbase );

    auto res = ncfg.attempt( [&]() -> httplib::Result {
        return client->Get(
            fmt::format( "/user_appdata${}/_design/types/_view/rifffLoopsByCreateTime?descending=true&limit=1", jamDatabaseID_Sanitised ).c_str() );
        });

    return deserializeJson< JamLatestState >( ncfg, res, *this, __FUNCTION__, "jam_latest_state" );
}

// ---------------------------------------------------------------------------------------------------------------------
bool JamFullSnapshot::fetch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID )
{
    auto client = createEndlesssHttpClient( ncfg, UserAgent::Couchbase );

    const endlesss::types::JamCouchID& jamDatabaseID_Sanitised = ncfg.checkAndSanitizeJamCouchID( jamDatabaseID );

    auto res = ncfg.attempt( [&]() -> httplib::Result {
        return client->Get(
            fmt::format( "/user_appdata${}/_design/types/_view/rifffLoopsByCreateTime?descending=true", jamDatabaseID_Sanitised ).c_str() );
        });

    return deserializeJson< JamFullSnapshot >( ncfg, res, *this, __FUNCTION__, "jam_full_snapshot" );
}

// ---------------------------------------------------------------------------------------------------------------------
bool JamRiffCount::fetch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID )
{
    auto client = createEndlesssHttpClient( ncfg, UserAgent::Couchbase );

    const endlesss::types::JamCouchID& jamDatabaseID_Sanitised = ncfg.checkAndSanitizeJamCouchID( jamDatabaseID );

    auto res = ncfg.attempt( [&]() -> httplib::Result {
        return client->Get( fmt::format( "/user_appdata${}/_design/types/_view/rifffsByCreateTime", jamDatabaseID_Sanitised ).c_str() );
        } );

    return deserializeJson< JamRiffCount >( ncfg, res, *this, __FUNCTION__, "jam_riff_count" );
}

// ---------------------------------------------------------------------------------------------------------------------
bool RiffDetails::fetch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID, const endlesss::types::RiffCouchID& riffDocumentID )
{
    const endlesss::types::JamCouchID& jamDatabaseID_Sanitised = ncfg.checkAndSanitizeJamCouchID( jamDatabaseID );

    // manually form a json body with the single document filter
    auto keyBody = fmt::format( R"({{ "keys" : [ "{}" ]}})", riffDocumentID );

    auto client = createEndlesssHttpClient( ncfg, UserAgent::Couchbase );

    // post the query, we expect a single document stream back
    auto res = ncfg.attempt( [&]() -> httplib::Result {
        return client->Post(
            fmt::format( "/user_appdata${}/_all_docs?include_docs=true", jamDatabaseID_Sanitised ).c_str(),
            keyBody,
            cMimeApplicationJson );
        });

    return deserializeJson< RiffDetails >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, jamDatabaseID_Sanitised ), "riff_details" );
}

// ---------------------------------------------------------------------------------------------------------------------
bool RiffDetails::fetchBatch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID, const endlesss::types::RiffCouchIDs& riffDocumentIDs )
{
    const endlesss::types::JamCouchID& jamDatabaseID_Sanitised = ncfg.checkAndSanitizeJamCouchID( jamDatabaseID );

    // expand all riff ids into a json array
    auto keyBody = fmt::format( R"({{ "keys" : [ "{}" ]}})", fmt::join( riffDocumentIDs, R"(", ")" ) );

    auto client = createEndlesssHttpClient( ncfg, UserAgent::Couchbase );

    // post the query, we expect multiple rows of documents in return
    auto res = ncfg.attempt( [&]() -> httplib::Result {
        return client->Post(
            fmt::format( "/user_appdata${}/_all_docs?include_docs=true", jamDatabaseID_Sanitised ).c_str(),
            keyBody,
            cMimeApplicationJson );
        });

    if ( ncfg.api().debugLastMinuteQuirkFixes )
    {
        // last minute shit found in Ash's solo jam - the stem playback value "on" would - for ONE RIFF - turn up as a 0 or 1 rather than a bool value like
        // literally everything else aaaaaaaaaaaaaaaaa
        return deserializeJson< RiffDetails >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, jamDatabaseID_Sanitised ), "riff_details_batch", []( std::string& bodyText )
            {
                bodyText = std::regex_replace( bodyText, std::regex( "\"on\":0," ), "\"on\":false," );
                bodyText = std::regex_replace( bodyText, std::regex( "\"on\":1," ), "\"on\":true," );
            });
    }
    else
    {
        return deserializeJson< RiffDetails >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, jamDatabaseID_Sanitised ), "riff_details_batch" );
    }
}

// ---------------------------------------------------------------------------------------------------------------------
bool StemTypeCheck::fetchBatch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID, const endlesss::types::StemCouchIDs& stemDocumentIDs )
{
    const endlesss::types::JamCouchID& jamDatabaseID_Sanitised = ncfg.checkAndSanitizeJamCouchID( jamDatabaseID );

    // expand all stem ids into a json array
    auto keyBody = fmt::format( R"({{ "keys" : [ "{}" ]}})", fmt::join( stemDocumentIDs, R"(", ")" ) );

    auto client = createEndlesssHttpClient( ncfg, UserAgent::Couchbase );

    // fetch all the stem data docs but only do a very minimal parse for 'type' fields
    auto res = ncfg.attempt( [&]() -> httplib::Result {
        return client->Post(
            fmt::format( "/user_appdata${}/_all_docs?include_docs=true", jamDatabaseID_Sanitised ).c_str(),
            keyBody,
            cMimeApplicationJson );
        });

    return deserializeJson< StemTypeCheck >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, jamDatabaseID_Sanitised ), "stem_type_check_batch" );
}

// ---------------------------------------------------------------------------------------------------------------------
bool StemDetails::fetchBatch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID, const endlesss::types::StemCouchIDs& stemDocumentIDs )
{
    const endlesss::types::JamCouchID& jamDatabaseID_Sanitised = ncfg.checkAndSanitizeJamCouchID( jamDatabaseID );

    // expand all stem ids into a json array
    auto keyBody = fmt::format( R"({{ "keys" : [ "{}" ]}})", fmt::join( stemDocumentIDs, R"(", ")" ) );

    auto client = createEndlesssHttpClient( ncfg, UserAgent::Couchbase );

    // fetch data about all the stems in bulk
    auto res = ncfg.attempt( [&]() -> httplib::Result {
        return client->Post(
            fmt::format( "/user_appdata${}/_all_docs?include_docs=true", jamDatabaseID_Sanitised ).c_str(),
            keyBody,
            cMimeApplicationJson );
        });

    return deserializeJson< StemDetails >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, jamDatabaseID_Sanitised ), "stem_details_batch" );
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

    // for usernames with hyphens, we have to escape them for use with couchbase
    // there are other symbols we should probably escape but I don't know of any users using them or if they are even allowed during user sign-up
    std::string sanitisedUserName = std::regex_replace( userName, std::regex( "-" ), "(2d)" );

    auto res = ncfg.attempt( [&]() -> httplib::Result {
        return client->Get( fmt::format( "/user_appdata${}/_design/membership/_view/getMembership", sanitisedUserName ).c_str() );
        });

    return deserializeJson< SubscribedJams >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, sanitisedUserName ), "subscribed_jams" );
}

// ---------------------------------------------------------------------------------------------------------------------
bool BandPermalinkMeta::fetch( const NetConfiguration& ncfg, const endlesss::types::JamCouchID& jamDatabaseID )
{
    const endlesss::types::JamCouchID& jamDatabaseID_Sanitised = ncfg.checkAndSanitizeJamCouchID( jamDatabaseID );

    auto client = createEndlesssHttpClient( ncfg, UserAgent::WebWithoutAuth );

    auto res = ncfg.attempt( [&]() -> httplib::Result {
        return client->Get( fmt::format( "/api/band/{}/permalink", jamDatabaseID_Sanitised ).c_str() );
        });

    return deserializeJson< BandPermalinkMeta >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, jamDatabaseID_Sanitised ), "band_permalink_meta" );
}

bool BandPermalinkMeta::Data::extractLongJamIDFromPath( std::string& outResult )
{
    // "/jam/296a74e8a64d254c0df007dda8a205d08e63915959ac5e912bdb8dde7077c638/join"
    //       ^->                                                          <-^
    //
    static constexpr auto cRegexTakeLongFormJamID = "jam/([^/]+)/join";

    std::regex regexLongForm( cRegexTakeLongFormJamID );

    const auto& pathExtraction = this->path;
    std::smatch m;
    if ( !pathExtraction.empty() && std::regex_search( pathExtraction, m, regexLongForm ) )
    {
        // cut the big ID oot
        outResult = m[1].str();
        return true;
    }
    return false;
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

#ifdef INSANE_LAST_MINUTE_FIXING_THE_SHARED_RIFF_JSON_GLITCHES
    static uint32_t exportIndexDbg = 0;
#endif // INSANE_LAST_MINUTE_FIXING_THE_SHARED_RIFF_JSON_GLITCHES

    return deserializeJson< SharedRiffsByUser >( ncfg, res, *this, requestContext, "shared_riffs_by_user", [&requestContext]( std::string& bodyText )
        {
#ifdef INSANE_LAST_MINUTE_FIXING_THE_SHARED_RIFF_JSON_GLITCHES
            exportIndexDbg++;

            {
                std::string outputFileName;
                base::sanitiseNameForPath( requestContext, outputFileName, '_', false );
                FILE* fExport = fopen( fmt::format( FMTX( "E:\\_DA_DEBUGS\\{}={}.before.json" ), exportIndexDbg, outputFileName ).c_str(), "wt");
                fprintf( fExport, "%s\n", bodyText.c_str() );
                fclose( fExport );
            }
            std::string pretest = bodyText;
#endif // INSANE_LAST_MINUTE_FIXING_THE_SHARED_RIFF_JSON_GLITCHES

            // first pass regex removes stuff like
            //      {                              <- we inject a 'dead' current object to pass parsing without wrecking the contents of lists
            //         "current": null                  in the process of trying to snip them out
            //      },
            //
            //      "isNote":false,
            //      "$type":null,                  <- this weird null entry for things
            //      "primaryColour":"ff4d9de0",
            //      "creationDate":null            <- ditto
            //      ...
            //
            // because cereal just hates nulls. as do i, now. we do this to avoid the hardwired null 
            // snipper below hitting them out of sequence and leaving orphaned keys
            //
            // first - swap out null 'current' entries in playback chunks with *some* kind of valid 'current' object to parse
            bodyText = std::regex_replace( bodyText, std::regex( R"("current":null)" ), R"("current":{"on":false,"gain":0.0})" );
            // then go hammer remaining weird "key":null stuff, we get a few random ones like "$type":null turning up completely randomly
            bodyText = std::regex_replace( bodyText, std::regex( R"("[$a-zA-Z]+":null,)" ), " " );

            // okay and also lets get rid of "loops":[null, too because that simplifies our later null neutering
            // we do (null,)+ to strip out any early sequences of nulls - the code later in this function takes care of mid-array and end-of-array instances
            bodyText = std::regex_replace( bodyText, std::regex( R"("loops":\[(null,)+)" ), R"("loops":[)" );

#ifdef INSANE_LAST_MINUTE_FIXING_THE_SHARED_RIFF_JSON_GLITCHES
            if ( pretest != bodyText )
            {
                std::string outputFileName;
                base::sanitiseNameForPath( requestContext, outputFileName, '_', false );
                FILE* fExport = fopen( fmt::format( FMTX( "E:\\_DA_DEBUGS\\{}={}.after.json" ), exportIndexDbg, outputFileName ).c_str(), "wt" );
                fprintf( fExport, "%s\n", bodyText.c_str() );
                fclose( fExport );
            }
#endif // INSANE_LAST_MINUTE_FIXING_THE_SHARED_RIFF_JSON_GLITCHES

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

                // remove ",null" if we're inside "loops" [ ]
                if ( bReplacementMode )
                {
                    // im so tired
lol_jesus_christ_a_goto:

                    CHECK_CHAR( 0, ',' )
                    CHECK_CHAR( 1, 'n' )
                    CHECK_CHAR( 2, 'u' )
                    CHECK_CHAR( 3, 'l' )
                    CHECK_CHAR( 4, 'l' )
                    {
                        bodyStream[i + 0] = ' ';
                        bodyStream[i + 1] = ' ';
                        bodyStream[i + 2] = ' ';
                        bodyStream[i + 3] = ' ';
                        bodyStream[i + 4] = ' ';
                        i += 4;

                        goto lol_jesus_christ_a_goto;
                    }
                }
            }

#ifdef INSANE_LAST_MINUTE_FIXING_THE_SHARED_RIFF_JSON_GLITCHES
            if ( pretest != bodyText )
            {
                std::string outputFileName;
                base::sanitiseNameForPath( requestContext, outputFileName, '_', false );
                FILE* fExport = fopen( fmt::format( FMTX( "E:\\_DA_DEBUGS\\{}={}.final.json" ), exportIndexDbg, outputFileName ).c_str(), "wt" );
                fprintf( fExport, "%s\n", bodyText.c_str() );
                fclose( fExport );
            }
#endif  // INSANE_LAST_MINUTE_FIXING_THE_SHARED_RIFF_JSON_GLITCHES
        });

    #undef CHECK_CHAR
}

// ---------------------------------------------------------------------------------------------------------------------
bool RiffStructureValidation::fetch( const NetConfiguration& ncfg, const std::string& jamLongID, int32_t pageNumber, int32_t pageSize )
{
    auto client = createEndlesssHttpClient( ncfg, UserAgent::WebWithoutAuth );

    auto res = ncfg.attempt( [&]() -> httplib::Result {
        return client->Get( fmt::format( "/jam/{}/rifffs?pageNo={}&pageSize={}", jamLongID, pageNumber, pageSize ).c_str() );
        } );

    return deserializeJson< RiffStructureValidation >( ncfg, res, *this, fmt::format( "{}( {} )", __FUNCTION__, jamLongID ), "public_riff_structure" );
}

// ---------------------------------------------------------------------------------------------------------------------
bool MyClubs::fetch( const NetConfiguration& ncfg )
{
    auto client = createEndlesssHttpClient( ncfg, UserAgent::WebWithAuth );

    auto res = ncfg.attempt( [&]() -> httplib::Result {
        return client->Get( "/clubs/my-clubs" );
        } );

    return deserializeJson< MyClubs >( ncfg, res, *this, fmt::format( "{}", __FUNCTION__ ), "my_clubs" );
}


// ---------------------------------------------------------------------------------------------------------------------
namespace push {

struct ShareRequestBody
{
    std::string             jamId;              // extended ID
    bool                    is_private = true;
    std::string             rifffId;            // couch ID of riff to share
    std::string             shareId;            // UUID for new share object
    std::string             title;              // name of shared object

    template<class Archive>
    inline void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( jamId )
               , cereal::make_nvp( "private", is_private )
               , CEREAL_NVP( rifffId )
               , CEREAL_NVP( shareId )
               , CEREAL_NVP( title )
        );
    }
};

// ---------------------------------------------------------------------------------------------------------------------
struct ShareRequestResponse
{
    struct Data
    {
        std::string             id;             // should match the input shareId UUID

        template<class Archive>
        inline void serialize( Archive& archive )
        {
            archive( CEREAL_NVP( id )
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
};

} // namespace push

// ---------------------------------------------------------------------------------------------------------------------
absl::Status push::ShareRiffOnFeed::action( const NetConfiguration& ncfg, std::string& resultUUID )
{
    ABSL_ASSERT( !m_jamCouchID.empty() );
    ABSL_ASSERT( !m_riffCouchID.empty() );
    ABSL_ASSERT( !m_shareName.empty() );

    std::string jamExtendedID;

    // riff share call needs the full extended band ID, so go find that
    endlesss::api::BandPermalinkMeta bandPermalink;
    if ( bandPermalink.fetch( ncfg, m_jamCouchID ) )
    {
        if ( bandPermalink.errors.empty() )
        {
            if ( bandPermalink.data.extractLongJamIDFromPath( jamExtendedID ) )
            {
                blog::api( FMTX( "ShareRiff : resolved extended ID for [{}] : [{}]" ), m_jamCouchID, jamExtendedID );
            }
            else
            {
                return absl::UnavailableError( fmt::format( FMTX( "Could not find extended ID; {}" ), bandPermalink.data.path ) );
            }
        }
        else
        {
            return absl::UnavailableError( fmt::format( FMTX( "Failed to fetch link data; {}" ), bandPermalink.errors[0] ) );
        }
    }
    else
    {
        return absl::UnknownError( "BandPermalinkMeta network request failure" );
    }

    ShareRequestBody requestBodyData;
    requestBodyData.jamId       = jamExtendedID;
    requestBodyData.is_private  = m_private;
    requestBodyData.rifffId     = m_riffCouchID.value();
    requestBodyData.shareId     = data::generateUUID_V1( false );
    requestBodyData.title       = m_shareName;

    blog::api( FMTX( "ShareRiff : generated share UUID [{}] for [{}]" ), requestBodyData.shareId, requestBodyData.title );

    // encode request to JSON to post
    std::string requestBodyJson;
    try
    {
        std::ostringstream iss;
        {
            cereal::JSONOutputArchive archive( iss );
            requestBodyData.serialize( archive );
        }
        requestBodyJson = iss.str();
    }
    catch ( cereal::Exception& cEx )
    {
        return absl::InternalError( cEx.what() );
    }

    auto client = createEndlesssHttpClient( ncfg, UserAgent::WebWithAuth );

    auto res = ncfg.attempt( [&]() -> httplib::Result {
        return client->Post(
            "/rifff-feed/share",
            requestBodyJson,
            cMimeApplicationJson );
        });

    ShareRequestResponse response;
    const bool isOk = deserializeJson< ShareRequestResponse >( ncfg, res, response, fmt::format( "{}( {} )", __FUNCTION__, m_riffCouchID ), "share_riff_on_feed" );
    if ( isOk )
    {
        ABSL_ASSERT( response.data.id == requestBodyData.shareId );
        resultUUID = response.data.id;
        return absl::OkStatus();
    }
    
    return absl::UnknownError( fmt::format( FMTX( "Network request failed; Status {}, `{}`" ), res->status, res->body ) );
}

// ---------------------------------------------------------------------------------------------------------------------
struct RiffCopyBody
{
    std::string             jamId;              // extended ID
    std::string             message;            // message to post with the riff
    std::string             rifffId;            // riff couch ID from jamId to share

    template<class Archive>
    inline void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( jamId )
               , CEREAL_NVP( message )
               , CEREAL_NVP( rifffId )
        );
    }
};

// ---------------------------------------------------------------------------------------------------------------------
absl::Status push::RiffCopy::action( const NetConfiguration& ncfg, std::string& resultUUID )
{
    ABSL_ASSERT( !m_jamFullID_CopyTo.empty() );
    ABSL_ASSERT( !m_jamCouchID.empty() );
    ABSL_ASSERT( !m_riffCouchID.empty() );

    std::string jamExtendedID;

    // riff share call needs the full extended band ID, so go find that
    endlesss::api::BandPermalinkMeta bandPermalink;
    if ( bandPermalink.fetch( ncfg, m_jamCouchID ) )
    {
        if ( bandPermalink.errors.empty() )
        {
            if ( bandPermalink.data.extractLongJamIDFromPath( jamExtendedID ) )
            {
                blog::api( FMTX( "ShareRiff : resolved extended ID for [{}] : [{}]" ), m_jamCouchID, jamExtendedID );
            }
            else
            {
                return absl::UnavailableError( fmt::format( FMTX( "Could not find extended ID; {}" ), bandPermalink.data.path ) );
            }
        }
        else
        {
            return absl::UnavailableError( fmt::format( FMTX( "Failed to fetch link data; {}" ), bandPermalink.errors[0] ) );
        }
    }
    else
    {
        return absl::UnknownError( "BandPermalinkMeta network request failure" );
    }


    RiffCopyBody requestBodyData;
    requestBodyData.jamId = jamExtendedID;
    requestBodyData.rifffId = m_riffCouchID.value();

    // encode request to JSON to post
    std::string requestBodyJson;
    try
    {
        std::ostringstream iss;
        {
            cereal::JSONOutputArchive archive( iss );
            requestBodyData.serialize( archive );
        }
        requestBodyJson = iss.str();
    }
    catch ( cereal::Exception& cEx )
    {
        return absl::InternalError( cEx.what() );
    }

    // https://api.endlesss.fm/jam/3549a4b5387bcb96c6fc8c5a9d2eb797c09cd46e5c42862fbb8912c977a0fa59/rifffs/import

    auto client = createEndlesssHttpClient( ncfg, UserAgent::WebWithAuth );

    auto res = ncfg.attempt( [&]() -> httplib::Result {
        return client->Post(
            fmt::format( FMTX("/jam/{}/rifffs/import"), m_jamFullID_CopyTo ),
            requestBodyJson,
            cMimeApplicationJson );
        });

    ShareRequestResponse response;
    const bool isOk = deserializeJson< ShareRequestResponse >( ncfg, res, response, fmt::format( "{}( {} )", __FUNCTION__, m_riffCouchID ), "riff_copy_import" );
    if ( isOk )
    {
        ABSL_ASSERT( response.data.id == requestBodyData.rifffId );
        resultUUID = response.data.id;
        return absl::OkStatus();
    }
    
    return absl::UnknownError( fmt::format( FMTX( "Network request failed; Status {}, `{}`" ), res->status, res->body ) );

}

} // namespace remote
} // namespace endlesss
