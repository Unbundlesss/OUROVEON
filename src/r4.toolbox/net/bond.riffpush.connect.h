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

namespace net {
namespace bond {

// 
void modalRiffPushClientConnection(
    const char* title,                                  // a imgui label to use with ImGui::OpenPopup
    const app::FrontendModule& frontend,
    net::bond::RiffPushClient& client );

} // namespace bond
} // namespace net

