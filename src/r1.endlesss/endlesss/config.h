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

} // namespace endlesss
} // namespace config
    