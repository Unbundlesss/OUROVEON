//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  

#include "pch.h"

#include "base/instrumentation.h"

namespace base {
namespace instr {

#ifdef PERF_SUPERLUMINAL

#define PERFORMANCEAPI_ENABLED 1

#include "Superluminal/PerformanceAPI_capi.h"

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

#else

void setThreadName( const char* name ) {}
void eventBegin( const char* name, const char* context, uint8_t colorR, uint8_t colorG, uint8_t colorB ) {}
void eventEnd() {}

#endif

} // namespace instr
} // namespace base


namespace tf
{
    // injected from taskflow executor, name the worker threads 
    void _taskflow_worker_thread_init( size_t threadID )
    {
        base::instr::setThreadName( fmt::format( OURO_THREAD_PREFIX "TaskFlow:{}", threadID ).c_str() );
    }
}