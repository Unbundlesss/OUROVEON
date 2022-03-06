//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "app/core.h"
#include "base/utils.h"

namespace config { namespace discord { struct Connection; } }

namespace discord {

struct Bot;

// little wrapper around running the discord bot interface via an imgui panel
struct BotWithUI
{
    BotWithUI( app::ICoreServices& coreServices, const config::discord::Connection& configConnection );
    ~BotWithUI();

    void imgui( app::CoreGUI& coreGUI );


private:

    std::unique_ptr< discord::Bot >             m_discordBot;

    app::ICoreServices&                         m_services;
    const config::discord::Connection&          m_config;

    base::RollingAverage< 10 >                  m_avgPacketSize;
    uint64_t                                    m_trafficOutBytes;
    std::optional< app::CoreGUI::UIInjectionHandle >                   m_statusBarHandle;
};

} // namespace discord
