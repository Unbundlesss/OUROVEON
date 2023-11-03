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

namespace ux {

    struct RiffFeedShareState;
    std::shared_ptr< RiffFeedShareState > createModelRiffFeedShareState( const endlesss::types::RiffIdentity& identity );

    // 
    void modalRiffFeedShare(
        const char* title,                                      // a imgui label to use with ImGui::OpenPopup
        RiffFeedShareState& riffFeedShareState,                 // UI state 
        const endlesss::api::NetConfiguration::Shared& netCfg,  // network services
        tf::Executor& taskExecutor );                           // async support via tf

} // namespace ux
