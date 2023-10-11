//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "endlesss/core.types.h"
#include "endlesss/api.h"

#include "spacetime/moment.h"

namespace endlesss { namespace toolkit { struct Warehouse; } }

namespace ux {

    struct JamValidateState
    {
        using Instance = std::shared_ptr<JamValidateState>;

        JamValidateState() = delete;
        JamValidateState( endlesss::types::JamCouchID& jamID )
            : m_jamCouchID( jamID )
        {
        }

        void imgui(
            const endlesss::toolkit::Warehouse& warehouse,
            const endlesss::api::NetConfiguration::Shared& netCfg,
            tf::Executor& taskExecutor );

        endlesss::types::JamCouchID     m_jamCouchID;
    };

    // 
    void modalJamValidate(
        const char* title,                                      // a imgui label to use with ImGui::OpenPopup
        JamValidateState& jamValidateState,                     // UI state 
        const endlesss::toolkit::Warehouse& warehouse,          // warehouse access to pull stem data
        const endlesss::api::NetConfiguration::Shared& netCfg,  // network services
        tf::Executor& taskExecutor );                           // async support via tf

} // namespace ux
