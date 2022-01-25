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

#include "endlesss/core.types.h"
#include "endlesss/cache.jams.h"

namespace app { namespace module { struct MixerInterface; } }
namespace rec { struct IRecordable; }

namespace app {

// ---------------------------------------------------------------------------------------------------------------------
// OuroApp is a small shim for UI apps to build upon that adds some basic Endlesss features out the gate; like a standard
// "login" screen that offers audio configuration and validating a connection to the Endlesss backend. 
//
struct OuroApp : public CoreGUI
{
    OuroApp()
        : CoreGUI()
    {
    }


protected:

    // from CoreGUI
    virtual int EntrypointGUI() override;

    virtual int EntrypointOuro() = 0;


    // run the default OuroApp boot screen with audio config options, endlesss connection stuff, all the basics
    void runBootConfigUI();


    // a modal ImGui dialogue that presents the contents of the jam cache, with filtering and such
    static void modalJamBrowser( 
        const char* title,                                                                      // a imgui label to use with ImGui::OpenPopup
        const endlesss::cache::Jams& jamLibrary,                                                // the jam cache to use
        const std::function<const endlesss::types::JamCouchID()>& getCurrentSelection,          // (optional) return a 'current selection' to have it highlit
        const std::function<void( const endlesss::types::JamCouchID& )>& changeSelection );     // (optional) callback when a jam is selected

    void ImGui_AppHeader();
    void ImGui_DiskRecorder( rec::IRecordable& recordable );


    // validated storage locations for the app
    // <optional> because this is expected to be configured once the boot sequence has checked / changed
    //              the root storage path; once the app starts for real, this should be valid
    std::optional< StoragePaths >           m_storagePaths = std::nullopt;


    config::discord::Connection             m_configDiscord;
};

} // namespace app
