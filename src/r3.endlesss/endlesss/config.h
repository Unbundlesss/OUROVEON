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

namespace config {
namespace endlesss {

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


    template<class Archive>
    void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( token )
               , CEREAL_NVP( password )
               , CEREAL_NVP( user_id )
               , CEREAL_NVP( expires )
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


    // BEHAVIOURAL HACKS
    // tweaks to how things should be when harsh reality shows up

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


    template<class Archive>
    void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( userAgentApp )
               , CEREAL_NVP( userAgentDb )
               , CEREAL_NVP( userAgentWeb )
               , CEREAL_NVP( certBundleRelative )
               , CEREAL_OPTIONAL_NVP( jamSentinelPollRateInSeconds )
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

} // namespace endlesss
} // namespace config
    

namespace cereal
{
    // specialisation for loading direct map of string:jamscan rather than cereal's default bloated way
    template <class Archive, class C, class A> inline
        void load( Archive& ar, absl::flat_hash_map<std::string, config::endlesss::PopulationPublics::JamScan, C, A>& map )
    {
        map.clear();

        auto hint = map.begin();
        while ( true )
        {
            const auto namePtr = ar.getNodeName();

            if ( !namePtr )
                break;

            std::string key = namePtr;
            config::endlesss::PopulationPublics::JamScan value; ar( value );
            hint = map.emplace_hint( hint, std::move( key ), std::move( value ) );
        }
    }

    // ditto user:count
    template <class Archive, class C, class A> inline
        void load( Archive& ar, absl::flat_hash_map<std::string, uint32_t, C, A>& map )
    {
        map.clear();

        auto hint = map.begin();
        while ( true )
        {
            const auto namePtr = ar.getNodeName();

            if ( !namePtr )
                break;

            std::string key = namePtr;
            uint32_t value; ar( value );
            hint = map.emplace_hint( hint, std::move( key ), value );
        }
    }
} // namespace cereal
