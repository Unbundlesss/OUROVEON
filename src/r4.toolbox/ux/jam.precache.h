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
#include "endlesss/core.services.h"

#include "spacetime/moment.h"

namespace endlesss { namespace toolkit { struct Warehouse; } }

namespace ux {

    struct JamPrecacheState;
    std::shared_ptr< JamPrecacheState > createJamPrecacheState( const endlesss::types::JamCouchID& jamID );

    // 
    void modalJamPrecache(
        const char* title,                                      // a imgui label to use with ImGui::OpenPopup
        JamPrecacheState& jamPrecacheState,                     // UI state 
        const endlesss::toolkit::Warehouse& warehouse,          // warehouse access to pull stem data
        endlesss::services::RiffFetchProvider& fetchProvider,   // network fetch services
        tf::Executor& taskExecutor );                           // async support via tf

} // namespace ux
