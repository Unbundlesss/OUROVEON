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

    struct CacheTrimState;
    std::shared_ptr< CacheTrimState > createCacheTrimState( const fs::path& cacheCommonRootPath );

    // 
    void modalCacheTrim(
        const char* title,                                      // a imgui label to use with ImGui::OpenPopup
        CacheTrimState& jamValidateState                        // UI state 
        );

} // namespace ux
