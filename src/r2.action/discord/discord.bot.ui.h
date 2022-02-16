//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "base/utils.h"
#include "app/imgui.ext.h"

namespace app { struct ICoreServices; namespace module { struct Frontend; } }
namespace config { namespace discord { struct Connection; } }

namespace discord {

struct Bot;

// little wrapper around running the discord bot interface via an imgui panel
struct BotWithUI
{
    BotWithUI( app::ICoreServices& coreServices, const config::discord::Connection& configConnection );
    ~BotWithUI();

    void imgui( const app::module::Frontend& frontend );


private:

    std::unique_ptr< discord::Bot >             m_discordBot;

    app::ICoreServices&                         m_services;
    const config::discord::Connection&          m_config;

    base::RollingAverage< 20 >                  m_avg;
    ImGui::RingBufferedGraph< int32_t, 100 >    m_bandwidth;
};

} // namespace discord
