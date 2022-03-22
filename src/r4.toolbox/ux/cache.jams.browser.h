//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "endlesss/core.types.h"

namespace endlesss { namespace cache { struct Jams; } }

namespace ux {

struct UniversalJamBrowserBehaviour
{
    std::function<bool( const endlesss::types::JamCouchID& )> fnIsDisabled;    // show jam as greyed out and unselectable
    std::function<void( const endlesss::types::JamCouchID& )> fnOnSelected;    // call when jam is selected
};

// 
void modalUniversalJamBrowser(
    const char* title,                                  // a imgui label to use with ImGui::OpenPopup
    const endlesss::cache::Jams& jamCache,
    const UniversalJamBrowserBehaviour& behaviour );


} // namespace ux