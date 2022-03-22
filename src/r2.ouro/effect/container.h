//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  generic host interface for things that can use fx to process audio
//

#pragma once

#include "base/id.simple.h"

namespace vst { class Instance; }

namespace effect {

// an interface spec for something that can hold instances of effects (currently VSTs until we refactor that)
// commands may be async, hence they return a counter that can be used as part of a blockUntil() mechanism if required
struct IContainer
{
    // audio buffer configurations for the container
    virtual int32_t getEffectSampleRate() const = 0;
    virtual int32_t getEffectMaximumBufferSize() const = 0;

    // option to pause/resume all effects
    virtual AsyncCommandCounter toggleEffectBypass() = 0;
    virtual bool isEffectBypassEnabled() const = 0;

    // add / remove effect instances
    // TODO refactor from vst:: specific
    virtual AsyncCommandCounter effectAppend( vst::Instance* vst ) = 0;
    virtual AsyncCommandCounter effectClearAll() = 0;
};

} // namespace effect

