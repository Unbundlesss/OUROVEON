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
struct Auth : public Base
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
struct API : public Base
{
    // data routing
    static constexpr auto StoragePath       = IPathProvider::PathFor::SharedData;       // api config ships with app
    static constexpr auto StorageFilename   = "endlesss.api.json";

    // identity for the network connection User-Agent string
    std::string             userAgentApp;   // .. when connecting to 'app services' as if we are the client / website
    std::string             userAgentDb;    // .. when talking to Couch

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
               , CEREAL_NVP( certBundleRelative )
               , CEREAL_OPTIONAL_NVP( jamSentinelPollRateInSeconds )
               , CEREAL_OPTIONAL_NVP( hackAllowStemSizeMismatch )
               , CEREAL_OPTIONAL_NVP( debugVerboseNetLog )
               , CEREAL_OPTIONAL_NVP( debugVerboseNetDataCapture )
        );
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// a captured bank of public jam metadata we siphon via a tedious automated process
struct PublicJamManifest : public Base
{
    // data routing
    static constexpr auto StoragePath       = IPathProvider::PathFor::SharedData;       // public snapshot ships with build
    static constexpr auto StorageFilename   = "endlesss.publics.json";

    struct Jam
    {
        std::string             band_id;
        std::string             invite_id;
        std::string             jam_name;
        std::string             earliest_user;
        std::string             latest_user;
        uint32_t                earliest_unixtime;
        uint32_t                latest_unixtime;
        uint32_t                estimated_days_of_activity;
        uint32_t                total_riffs;

        template<class Archive>
        void serialize( Archive& archive )
        {
            archive( CEREAL_NVP( band_id )
                   , CEREAL_NVP( invite_id )
                   , CEREAL_NVP( jam_name )
                   , CEREAL_NVP( earliest_user )
                   , CEREAL_NVP( latest_user )
                   , CEREAL_NVP( earliest_unixtime )
                   , CEREAL_NVP( latest_unixtime )
                   , CEREAL_NVP( estimated_days_of_activity )
                   , CEREAL_NVP( total_riffs )
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


} // namespace endlesss
} // namespace config
    