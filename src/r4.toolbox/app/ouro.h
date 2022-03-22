//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "app/core.h"

#include "discord/config.h"

namespace rec { struct IRecordable; }
namespace app {

// ---------------------------------------------------------------------------------------------------------------------
// OuroApp is a small shim for UI apps to build upon that adds some basic Endlesss features out the gate; like a standard
// "login" screen that offers audio/services configuration and validating a connection to the Endlesss backend
//
struct OuroApp : public CoreGUI
{
    OuroApp()
        : CoreGUI()
    {}

protected:

    // from CoreGUI
    // this inserts the generic ouro app configuration preflight UI; when that closes, we pass to EntrypointOuro
    virtual int EntrypointGUI() override;

    // inheritants implement this as app entrypoint
    virtual int EntrypointOuro() = 0;



    // validated storage locations for the app
    // <optional> because this is expected to be configured once the boot sequence has checked / changed
    //              the root storage path; once the app starts for real, this can be assumed to be valid
    std::optional< StoragePaths >           m_storagePaths = std::nullopt;
    
    // the global live-instance stem cache, used to populate riffs when preparing for playback
    endlesss::cache::Stems                  m_stemCache;

    // discord config done on preflight screen
    config::discord::Connection             m_configDiscord;
};

} // namespace app
