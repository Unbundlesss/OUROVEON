//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "config/base.h"
#include "endlesss/ids.h"
#include "spacetime/chronicle.h"

namespace config {
namespace endlesss {

// ---------------------------------------------------------------------------------------------------------------------
struct SyncOptions
{
    bool            sync_collectibles = false;  // go fetch the collectible jam data from (buggy) web endpoints?
    bool            sync_state        = true;   // fetch riff counts for all private jams (may take a while)

    template<class Archive>
    void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( sync_collectibles )
               , CEREAL_NVP( sync_state )
        );
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// extraction of the default web login response, to gather login tokens
OURO_CONFIG( Auth )
{
    // data routing
    static constexpr auto StoragePath       = IPathProvider::PathFor::SharedConfig;
    static constexpr auto StorageFilename   = "endlesss.auth.json";

    std::string     token;
    std::string     password;
    std::string     user_id;

    uint64_t        expires = 0;

    SyncOptions     sync_options;

    template<class Archive>
    void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( token )
               , CEREAL_NVP( password )
               , CEREAL_NVP( user_id )
               , CEREAL_NVP( expires )
               , CEREAL_OPTIONAL_NVP( sync_options )
        );
    }
};

// response when auth bails on us
struct AuthFailure
{
    std::string     message;

    template<class Archive>
    void serialize( Archive & archive )
    {
        archive( CEREAL_NVP( message )
        );
    }
};


// ---------------------------------------------------------------------------------------------------------------------
// config required for all the remote calls to pull data from endlesss backend
OURO_CONFIG( rAPI )
{
    // data routing
    static constexpr auto StoragePath       = IPathProvider::PathFor::SharedData;       // api config ships with app
    static constexpr auto StorageFilename   = "endlesss.api.json";

    // identity for the network connection User-Agent string
    std::string             userAgentApp;   // .. when connecting to 'app services' as if we are the client / website
    std::string             userAgentDb;    // .. when talking to Couch
    std::string             userAgentWeb;   // .. when talking to web api

    // path from the app shared data directory to a valid CA Root Certificates file
    std::string             certBundleRelative;

    // seconds between polls when using a sentinel to track jam changes
    int32_t                 jamSentinelPollRateInSeconds = 5;


    // 
    // NB. default vs unstable below is selected via the saved Performance configuration
    // 

    // connection and read timeouts for all network requests
    int32_t                 networkTimeoutInSecondsDefault = 2;         // for LAN broadband / stable connections
    int32_t                 networkTimeoutInSecondsUnstable = 6;        // for 4G / less reliable connections

    // how many times we'll retry a request until abandoning the idea and failing out
    int32_t                 networkRequestRetryLimitDefault = 2;        // for LAN broadband / stable connections
    int32_t                 networkRequestRetryLimitUnstable = 5;       // for 4G / less reliable connections



    // BEHAVIOURAL HACKS
    // tweaks to how things should be when harsh reality shows up

    // enable http network compression
    bool                    connectionCompressionSupport = true;

    // set to relax requirements on the database size having to match the CDN actual size for stem data
    // this is basically required for deep diving back more than about 6 months, there is often some strange
    // data lurking in the older archives
    bool                    hackAllowStemSizeMismatch = true;

    // .. similarly this can happen when actually streaming the data in, it just terminates early every time. 
    // activate this to ignore and clamp the received audio buffer to whatever we get
    bool                    hackAllowStemUnderflow = false;


    // DEBUG OPTIONS
    // general users shouldn't need to enable these, they are for debugging Endlesss traffic, malformed data capture etc

    // if true, install a logger when making network requests to print out all http requests
    bool                    debugVerboseNetLog = false;

    // if true, stream all http response body text out to files before they are deserialised. will fill up your drive.
    bool                    debugVerboseNetDataCapture = false;

    // by default we identify 'broken' stems from old jams as they lack application version tags - however, even older stems
    // (ie. mid/early 2019) are valid but also lack this metadata. turning this on will let you sync very old jams by ignoring
    // this change in data layout
    bool                    allowStemsWithoutVersionData = false;


    template<class Archive>
    void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( userAgentApp )
               , CEREAL_NVP( userAgentDb )
               , CEREAL_NVP( userAgentWeb )
               , CEREAL_NVP( certBundleRelative )
               , CEREAL_OPTIONAL_NVP( jamSentinelPollRateInSeconds )
               , CEREAL_OPTIONAL_NVP( networkTimeoutInSecondsDefault )
               , CEREAL_OPTIONAL_NVP( networkTimeoutInSecondsUnstable )
               , CEREAL_OPTIONAL_NVP( networkRequestRetryLimitDefault )
               , CEREAL_OPTIONAL_NVP( networkRequestRetryLimitUnstable )
               , CEREAL_OPTIONAL_NVP( hackAllowStemSizeMismatch )
               , CEREAL_OPTIONAL_NVP( debugVerboseNetLog )
               , CEREAL_OPTIONAL_NVP( debugVerboseNetDataCapture )
        );
    }
};


// ---------------------------------------------------------------------------------------------------------------------
// a captured bank of public jam metadata we siphon via a lengthy automated offline process
OURO_CONFIG( PublicJamManifest )
{
    // data routing
    static constexpr auto StoragePath       = IPathProvider::PathFor::SharedData;       // public snapshot ships with build
    static constexpr auto StorageFilename   = "endlesss.publics.json";

    struct Jam
    {
        std::string             band_id;
        std::string             invite_id;
        std::string             listen_id;
        std::string             jam_name;
        std::string             earliest_user;
        std::string             latest_user;
        uint32_t                earliest_unixtime;
        uint32_t                latest_unixtime;
        uint32_t                estimated_days_of_activity;
        uint32_t                total_riffs;
        uint32_t                subscribed_member_count;

        template<class Archive>
        void serialize( Archive& archive )
        {
            archive( CEREAL_NVP( band_id )
                   , CEREAL_NVP( invite_id )
                   , CEREAL_NVP( listen_id )
                   , CEREAL_NVP( jam_name )
                   , CEREAL_NVP( earliest_user )
                   , CEREAL_NVP( latest_user )
                   , CEREAL_NVP( earliest_unixtime )
                   , CEREAL_NVP( latest_unixtime )
                   , CEREAL_NVP( estimated_days_of_activity )
                   , CEREAL_NVP( total_riffs )
                   , CEREAL_NVP( subscribed_member_count )
            );
        }
    };

    std::vector< Jam >  jams;

    template<class Archive>
    void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( jams )
        );
    }
};


// ---------------------------------------------------------------------------------------------------------------------
// snapshot of collectible/nft jam metadata (boo, hiss)
OURO_CONFIG( CollectibleJamManifest )
{
    // data routing
    static constexpr auto StoragePath       = IPathProvider::PathFor::SharedConfig;
    static constexpr auto StorageFilename   = "endlesss.collectibles.json";

    struct Jam
    {
        std::string             jamId;
        std::string             name;
        std::string             bio;
        std::string             bandId;
        std::string             owner;
        std::vector< std::string > members;
        uint64_t                rifftime = 0;
        uint32_t                riffCount = 0;

        template<class Archive>
        void serialize( Archive& archive )
        {
            archive( CEREAL_NVP( jamId )
                   , CEREAL_NVP( name )
                   , CEREAL_OPTIONAL_NVP( bio )
                   , CEREAL_NVP( bandId )
                   , CEREAL_NVP( owner )
                   , CEREAL_NVP( members )
                   , CEREAL_NVP( rifftime )
                   , CEREAL_OPTIONAL_NVP( riffCount )
            );
        }
    };

    std::vector< Jam >  jams;

    template<class Archive>
    void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( jams )
        );
    }
};

// a version of the collectible manifest that ships with the build
struct CollectibleJamManifestSnapshot
{
    // data routing
    static constexpr auto StoragePath       = IPathProvider::PathFor::SharedData;
    static constexpr auto StorageFilename   = "endlesss.collectibles-snapshot.json";

    std::vector< CollectibleJamManifest::Jam >  jams;

    template<class Archive>
    void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( jams )
        );
    }
};


// ---------------------------------------------------------------------------------------------------------------------
// detailed data capture from public jams regarding user involvement
OURO_CONFIG( PopulationPublics )
{
    // data routing
    static constexpr auto StoragePath       = IPathProvider::PathFor::SharedData;       // snapshot ships with build
    static constexpr auto StorageFilename   = "endlesss.population-publics.json";

    struct JamScan
    {
        using UserContributions = absl::flat_hash_map< std::string, uint32_t >;

        std::string                 jam_name;
        uint32_t                    riff_scanned = 0;
        uint32_t                    unique_users = 0;
        std::vector< std::string >  subscribed_users;
        UserContributions           user_and_riff_count;

        template<class Archive>
        void serialize( Archive& archive )
        {
            archive( CEREAL_NVP( jam_name )
                   , CEREAL_NVP( riff_scanned )
                   , CEREAL_NVP( unique_users )
                   , CEREAL_NVP( subscribed_users )
                   , CEREAL_NVP( user_and_riff_count )
            );
        }
    };
    using PerJamScan = absl::flat_hash_map< std::string, JamScan >;

    PerJamScan  jampop;

    template<class Archive>
    void serialize( Archive& archive )
    {
        archive( CEREAL_NVP(jampop) );
    }
};


// ---------------------------------------------------------------------------------------------------------------------
// list of all the users we know about
OURO_CONFIG( PopulationGlobalUsers )
{
    // data routing
    static constexpr auto StoragePath       = IPathProvider::PathFor::SharedData;       // snapshot ships with build
    static constexpr auto StorageFilename   = "endlesss.population-global.json";

    std::vector<std::string> users;

    template<class Archive>
    void serialize( Archive & archive )
    {
        archive( CEREAL_NVP(users) );
    }
};


// ---------------------------------------------------------------------------------------------------------------------
OURO_CONFIG( SharedRiffsCache )
{
    // data routing
    static constexpr auto StoragePath       = IPathProvider::PathFor::SharedConfig;
    static constexpr auto StorageFilename   = "endlesss.shared-riffs.json";

    using SharedRiffID  = ::endlesss::types::SharedRiffCouchID;
    using RiffID        = ::endlesss::types::RiffCouchID;
    using JamID         = ::endlesss::types::JamCouchID;

    uint32_t                        m_version = 1;
    std::string                     m_username;
    std::size_t                     m_count = 0;        // number of retrieved shares
    uint64_t                        m_lastSyncTime = 0;

    // flat storage of m_count shared riff details
    std::vector< std::string >      m_names;
    std::vector< std::string >      m_images;
    std::vector< SharedRiffID >     m_sharedRiffIDs;    // unique ID for the shared-riff object, distinct from the riff couch ID
    std::vector< RiffID >           m_riffIDs;
    std::vector< JamID >            m_jamIDs;
    std::vector< uint64_t >         m_timestamps;
    std::vector< bool >             m_private;          // will only turn up when browsing your own shares, true if this was something not shared to the public feed
    std::vector< bool >             m_personal;         // riff shared from personal (non-band#####) jam space

    template<class Archive>
    inline void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( m_version )
               , CEREAL_NVP( m_username )
               , CEREAL_NVP( m_count )
               , CEREAL_NVP( m_lastSyncTime )
               , CEREAL_NVP( m_names )
               , CEREAL_NVP( m_images )
               , CEREAL_NVP( m_sharedRiffIDs )
               , CEREAL_NVP( m_riffIDs )
               , CEREAL_NVP( m_jamIDs )
               , CEREAL_NVP( m_timestamps )
               , CEREAL_NVP( m_private )
               , CEREAL_NVP( m_personal )
        );
    }
};


// ---------------------------------------------------------------------------------------------------------------------
// BNS, just a long list of band IDs we've dug up and pre-resolved to some kind of name to save some traffic to 
// the endlesss APIs when we solve lists of shared riff jam names (for example)
OURO_CONFIG( BandNameService )
{
    // data routing
    static constexpr auto StoragePath       = IPathProvider::PathFor::SharedData;
    static constexpr auto StorageFilename   = "endlesss.bns.json";

    struct Entry
    {
        std::string                 link_name;      // from original permalink, contains the initial name set
        std::string                 sync_name;      // from riff page 0 request, tracks any subsequent renames
                                                    // if this is empty, the link/sync names were the same (or we couldn't get a sync name due to permission failure)

        template<class Archive>
        void serialize( Archive& archive )
        {
            archive( CEREAL_NVP( link_name )
                   , CEREAL_NVP( sync_name )
            );
        }
    };
    using NameLookup = absl::flat_hash_map< ::endlesss::types::JamCouchID, Entry >;

    NameLookup  entries;

    template<class Archive>
    void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( entries ) );
    }
};


} // namespace config
} // namespace endlesss


namespace cereal
{
    // specialisation for loading direct map of string:jamscan rather than cereal's default bloated way
    #define SPECIALISE_FLAT_HASH_MAP_LOAD( _keyType, _valueType )                               \
        template <class Archive, class C, class A> inline                                       \
            void load( Archive& ar, absl::flat_hash_map<_keyType, _valueType, C, A>& map )      \
        {                                                                                       \
            map.clear();                                                                        \
                                                                                                \
            auto hint = map.begin();                                                            \
            while ( true )                                                                      \
            {                                                                                   \
                const auto namePtr = ar.getNodeName();                                          \
                                                                                                \
                if ( !namePtr )                                                                 \
                    break;                                                                      \
                                                                                                \
                _keyType key = _keyType( namePtr );                                             \
                _valueType value; ar( value );                                                  \
                hint = map.emplace_hint( hint, std::move( key ), std::move( value ) );          \
            }                                                                                   \
        }

    SPECIALISE_FLAT_HASH_MAP_LOAD( std::string, config::endlesss::PopulationPublics::JamScan );
    SPECIALISE_FLAT_HASH_MAP_LOAD( ::endlesss::types::JamCouchID, config::endlesss::BandNameService::Entry );
    SPECIALISE_FLAT_HASH_MAP_LOAD( std::string, uint32_t );
} // namespace cereal
