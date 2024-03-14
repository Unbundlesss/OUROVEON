//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  procedural riff weaver; using Virtual Riffs system, create wild new riffs by combining similar-but-disparate stems
//

#pragma once

#include "app/core.h"
#include "base/utils.h"
#include "net/bond.riffpush.h"
#include "endlesss/core.types.h"
#include "endlesss/core.services.h"


namespace ux {

struct Weaver
{
    Weaver( base::EventBusClient eventBus );
    ~Weaver();

    void imgui( app::CoreGUI& coreGUI, net::bond::RiffPushClient& bondClient, endlesss::toolkit::Warehouse& warehouse );

private:

    struct State;
    std::unique_ptr< State >    m_state;

};

} // namespace ux
