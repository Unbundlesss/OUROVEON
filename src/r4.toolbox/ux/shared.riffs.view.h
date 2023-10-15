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


namespace ux {

struct SharedRiffView
{
    SharedRiffView( endlesss::api::NetConfiguration::Shared& networkConfig, base::EventBusClient eventBus );
    ~SharedRiffView();

    void imgui( app::CoreGUI& coreGUI, endlesss::services::IJamNameCacheServices& jamNameCacheServices );

private:

    struct State;
    std::unique_ptr< State >    m_state;

};

} // namespace ux
