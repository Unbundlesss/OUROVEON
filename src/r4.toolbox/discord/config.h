//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#pragma once

#include "config/base.h"

namespace config {
namespace discord {

OURO_CONFIG( Connection )
{
    // data routing
    static constexpr auto StoragePath       = IPathProvider::PathFor::SharedConfig;
    static constexpr auto StorageFilename   = "discord.json";

    std::string     botToken;       // access token for bot work
    std::string     guildSID;       // snowflake ID for the discord guild to deal with

    template<class Archive>
    void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( botToken ),
                 CEREAL_NVP( guildSID )
        );
    }
};
using ConnectionOptional = std::optional< Connection >;

} // namespace discord
} // namespace config

