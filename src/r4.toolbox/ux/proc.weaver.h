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

#include "config/base.h"

namespace config {

struct IPathProvider;

OURO_CONFIG( Weaver )
{
    // data routing
    static constexpr auto StoragePath       = IPathProvider::PathFor::SharedData;
    static constexpr auto StorageFilename   = "endlesss.weaver.json";

    std::vector< std::string >  presetsWeHate;

    template<class Archive>
    void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( presetsWeHate )
        );
    }
};

} // namespace config


namespace ux {

struct Weaver
{
    Weaver( const config::IPathProvider& pathProvider, base::EventBusClient eventBus );
    ~Weaver();

    void imgui(
        app::CoreGUI& coreGUI,
        endlesss::live::RiffPtr& currentRiffPtr,      // currently playing riff, may be null
        net::bond::RiffPushClient& bondClient,
        endlesss::toolkit::Warehouse& warehouse );

private:

    struct State;
    std::unique_ptr< State >    m_state;

};

} // namespace ux
