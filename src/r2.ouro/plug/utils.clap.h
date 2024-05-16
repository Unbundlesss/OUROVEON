//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#pragma once

#include "clap/clap.h"

namespace plug {
namespace utils {

constexpr const char* clapEventTypeToString( uint16_t eventType )
{
    switch( eventType )
    {
        // from clap/events.h
        case CLAP_EVENT_NOTE_ON:             return "NOTE_ON";
        case CLAP_EVENT_NOTE_OFF:            return "NOTE_OFF";
        case CLAP_EVENT_NOTE_CHOKE:          return "NOTE_CHOKE";
        case CLAP_EVENT_NOTE_END:            return "NOTE_END";
        case CLAP_EVENT_NOTE_EXPRESSION:     return "NOTE_EXPRESSION";
        case CLAP_EVENT_PARAM_VALUE:         return "PARAM_VALUE";
        case CLAP_EVENT_PARAM_MOD:           return "PARAM_MOD";
        case CLAP_EVENT_PARAM_GESTURE_BEGIN: return "PARAM_GESTURE_BEGIN";
        case CLAP_EVENT_PARAM_GESTURE_END:   return "PARAM_GESTURE_END";
        case CLAP_EVENT_TRANSPORT:           return "TRANSPORT";
        case CLAP_EVENT_MIDI:                return "MIDI";
        case CLAP_EVENT_MIDI_SYSEX:          return "MIDI_SYSEX";
        case CLAP_EVENT_MIDI2:               return "MIDI2";
        default:
            return "UNKNOWN";
    }
}

} // namespace utils
} // namespace plug
