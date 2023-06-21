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
#include "discord/config.h"

namespace discord {

struct Bot;

// ---------------------------------------------------------------------------------------------------------------------
// little wrapper around running the discord bot interface via an imgui panel
struct BotWithUI
{
    BotWithUI( app::ICoreServices& coreServices );
    ~BotWithUI();

    void imgui( app::CoreGUI& coreGUI );


private:

    std::unique_ptr< Bot >                  m_discordBot;

    app::ICoreServices&                     m_services;
    config::discord::ConnectionOptional     m_config;

    base::RollingAverage< 10 >              m_avgPacketSize;

    uint64_t                                m_trafficOutBytes;
    app::CoreGUI::UIInjectionHandleOptional m_trafficOutBytesStatusHandle;
};

} // namespace discord
