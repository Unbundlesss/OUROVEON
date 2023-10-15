//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#pragma once

#include "base/construction.h"

// using a default prefix to bump our threads to sort at the top of the view
#define OURO_THREAD_PREFIX      "$:OURO::"

namespace base {
namespace instr {

// ---------------------------------------------------------------------------------------------------------------------
// set profiler-friendly name for currently executing thread
void setThreadName( const char* name );

// ---------------------------------------------------------------------------------------------------------------------
// generic profiler event bookends
void eventBegin( const char* name, const char* context = nullptr, uint8_t colorR = 255, uint8_t colorG = 220, uint8_t colorB = 170 );
void eventEnd();

// ---------------------------------------------------------------------------------------------------------------------
enum class PresetColour
{
    Red,
    Orange,
    Amber,
    Lime,
    Emerald,
    Cyan,
    Indigo,
    Violet,
    Pink
};

// automatically start/stop an event block inside a scope
struct ScopedEvent
{
    ScopedEvent() = delete;
    DECLARE_NO_COPY_NO_MOVE( ScopedEvent );

    ScopedEvent( const char* name )
    {
        eventBegin( name );
    }
    ScopedEvent( const char* name, const char* context )
    {
        eventBegin( name, context );
    }
    ScopedEvent( const char* name, const char* context, const PresetColour col )
    {
        init( name, context, col );
    }
    ScopedEvent( const char* name, const PresetColour col )
    {
        init( name, nullptr, col );
    }
    ~ScopedEvent()
    {
        eventEnd();
    }

private:
    static void init( const char* name, const char* context, const PresetColour col )
    {
        switch ( col )
        {
        default:
        case PresetColour::Red:      eventBegin( name, context, 252, 165, 165 ); break;
        case PresetColour::Orange:   eventBegin( name, context, 253, 186, 116 ); break;
        case PresetColour::Amber:    eventBegin( name, context, 252, 211,  77 ); break;
        case PresetColour::Lime:     eventBegin( name, context, 190, 242, 100 ); break;
        case PresetColour::Emerald:  eventBegin( name, context, 110, 231, 183 ); break;
        case PresetColour::Cyan:     eventBegin( name, context, 103, 232, 249 ); break;
        case PresetColour::Indigo:   eventBegin( name, context, 165, 180, 252 ); break;
        case PresetColour::Violet:   eventBegin( name, context, 196, 181, 253 ); break;
        case PresetColour::Pink:     eventBegin( name, context, 240, 171, 252 ); break;
        }
    }
};

} // namespace instr
} // namespace base