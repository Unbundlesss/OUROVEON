//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "app/imgui.ext.h"

namespace app { struct ICoreServices; namespace module { struct Frontend; } }
namespace config { namespace discord { struct Connection; } }

namespace discord {


template <double _windowSize>
struct RollingAverage
{
    static constexpr double cNewValueWeight = 1.0 / _windowSize;
    static constexpr double cOldValueWeight = 1.0 - cNewValueWeight;

    double  m_average       = 0.0;
    bool    m_initialSample = true;

    inline void update( double v )
    {
        if ( m_initialSample )
        {
            m_average = v;
            m_initialSample = false;
        }
        else
            m_average = ( v * cNewValueWeight) + ( m_average * cOldValueWeight);
    }
};

struct Bot;

// little wrapper around running the discord bot interface via an imgui panel
struct BotWithUI
{
    BotWithUI( app::ICoreServices& coreServices, const config::discord::Connection& configConnection );
    ~BotWithUI();

    void imgui( const app::module::Frontend& frontend );


private:

    std::unique_ptr< discord::Bot >     m_discordBot;

    app::ICoreServices&                 m_services;
    const config::discord::Connection&  m_config;

    RollingAverage< 20.0 >                      m_avg;
    ImGui::RingBufferedGraph< int32_t, 100 >    m_bandwidth;
};

} // namespace discord
