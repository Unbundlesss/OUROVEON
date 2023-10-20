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

namespace app { struct ICoreServices; }
namespace endlesss { namespace cache { struct Jams; } }

namespace ux {

struct UniversalJamBrowserState;

struct UniversalJamBrowserBehaviour
{
    UniversalJamBrowserBehaviour();
    virtual ~UniversalJamBrowserBehaviour();

    std::function<bool( const endlesss::types::JamCouchID& )> fnIsDisabled;    // show jam as greyed out and unselectable
    std::function<void( const endlesss::types::JamCouchID& )> fnOnSelected;    // call when jam is selected

    std::unique_ptr< UniversalJamBrowserState >     m_state;
};

// 
void modalUniversalJamBrowser(
    const char* title,                                  // a imgui label to use with ImGui::OpenPopup
    const endlesss::cache::Jams& jamCache,
    const UniversalJamBrowserBehaviour& behaviour,
    app::ICoreServices& coreServices );

} // namespace ux
