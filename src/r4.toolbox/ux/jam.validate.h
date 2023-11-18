//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#pragma once

#include "endlesss/core.types.h"
#include "endlesss/api.h"

namespace endlesss { namespace toolkit { struct Warehouse; } }

namespace ux {

    struct JamValidateState;
    std::shared_ptr< JamValidateState > createJamValidateState( const endlesss::types::JamCouchID& jamID );

    // 
    void modalJamValidate(
        const char* title,                                      // a imgui label to use with ImGui::OpenPopup
        JamValidateState& jamValidateState,                     // UI state 
        endlesss::toolkit::Warehouse& warehouse,                // warehouse access to pull stem data / modify riff records
        const endlesss::api::NetConfiguration::Shared& netCfg,  // network services
        tf::Executor& taskExecutor );                           // async support via tf

} // namespace ux
