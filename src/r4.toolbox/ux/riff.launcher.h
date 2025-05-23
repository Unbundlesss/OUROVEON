//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#pragma once

#include "app/core.h"
#include "base/utils.h"
#include "endlesss/core.types.h"
#include "endlesss/core.services.h"

namespace ux {

struct RiffLauncher
{
    RiffLauncher( base::EventBusClient eventBus );
    ~RiffLauncher();

    void imgui( app::CoreGUI& coreGUI, const endlesss::live::RiffAndPermutation& currentRiff );

private:

    struct State;
    std::unique_ptr< State >    m_state;

};

} // namespace ux
