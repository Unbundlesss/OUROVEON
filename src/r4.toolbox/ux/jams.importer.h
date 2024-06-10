//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#pragma once
#include "base/eventbus.h"

namespace app { struct OuroApp; }

namespace ux {

    struct JamImporterState;
    std::shared_ptr< JamImporterState > createJamImporterState( const fs::path& importPath, base::EventBusClient eventBus );

    // 
    void modalJamImporterState(
        const char* title,                      // a imgui label to use with ImGui::OpenPopup
        JamImporterState& jamImporterState,     // UI state 
        app::OuroApp& ouroApplication );        // access to various app services we'll need

} // namespace ux
