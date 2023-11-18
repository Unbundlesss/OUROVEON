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

    struct CacheMigrationState;
    std::shared_ptr< CacheMigrationState > createCacheMigrationState( const fs::path& cacheCommonRootPath );

    // 
    void modalCacheMigration(
        const char* title,                                      // a imgui label to use with ImGui::OpenPopup
        endlesss::toolkit::Warehouse& warehouse,                // warehouse access to pull stem data / modify riff records
        CacheMigrationState& jamValidateState                   // UI state 
        );

} // namespace ux
