//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "endlesss/live.riff.h"

namespace base { struct EventBusClient; }

namespace ImGui {
namespace ux {

// ---------------------------------------------------------------------------------------------------------------------
// provide a compact rendering of the riff details - key, author, bpm, submission date, etc
// also provides a [Export This] button, serviced via the event bus
//
void RiffDetails(
    endlesss::live::RiffPtr& riffPtr,
    base::EventBusClient& eventBusClient,
    const endlesss::toolkit::xp::RiffExportAdjustments* adjustments = nullptr );

} // namespace ux
} // namespace ImGui