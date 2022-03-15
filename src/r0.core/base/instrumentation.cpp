//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  

#include "pch.h"

#include "base/instrumentation.h"

// ---------------------------------------------------------------------------------------------------------------------
#ifdef OURO_FEATURE_SUPERLUMINAL

#define PERFORMANCEAPI_ENABLED 1

#include "Superluminal/PerformanceAPI_capi.h"

namespace base {
namespace instr {

void setThreadName( const char* name )
{
    PerformanceAPI_SetCurrentThreadName( name );
}

void eventBegin( const char* name, const char* context, uint8_t colorR, uint8_t colorG, uint8_t colorB )
{
    PerformanceAPI_BeginEvent( name, context, PERFORMANCEAPI_MAKE_COLOR( colorR, colorG, colorB ) );
}

void eventEnd()
{
    PerformanceAPI_EndEvent();
}

} // namespace instr
} // namespace base


// ---------------------------------------------------------------------------------------------------------------------
#else

namespace base {
namespace instr {

// null stubs for no profiler
void setThreadName( const char* name ) {}
void eventBegin( const char* name, const char* context, uint8_t colorR, uint8_t colorG, uint8_t colorB ) {}
void eventEnd() {}

} // namespace instr
} // namespace base

#endif
