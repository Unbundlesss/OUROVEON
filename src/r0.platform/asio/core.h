//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
// ASIO homebrew SDK
//

#pragma once

#include "asio/common.h"
#include "asio/db.h"

#include "asio/asiosys.h"
#include "asio/asio.h"

#include "win32/utils.h"

#include <objbase.h>

#if NATIVE_INT64
#define ASIO64toDouble(a)  (a)
#else
template< typename _Tx >
inline constexpr double ASIO64toDouble( const _Tx& a )
{ 
    constexpr auto twoRaisedTo32 = 4294967296.;
    return (a.lo + a.hi * twoRaisedTo32);
}
#endif


// ---------------------------------------------------------------------------------------------------------------------
namespace asio {
namespace callbacks {

// this mess is a half-hearted attempt to fix a frustration with ASIO callbacks :- they lack any kind of caller context
// at all, so one cannot associate callback registration with a class or an ID or anything. The horror show below
// lets us declare a set of suffixed static functions which each look back into a 
struct CallbackInterface
{
    virtual void bufferSwitch( long index, ASIOBool processNow ) = 0;
    virtual ASIOTime* bufferSwitchTimeInfo( ASIOTime* timeInfo, long index, ASIOBool processNow ) = 0;
    virtual void sampleRateDidChange( ASIOSampleRate sRate ) = 0;
    virtual long asioMessage( long selector, long value, void* message, double* opt ) = 0;
};

static std::array<CallbackInterface*, 3>  gCallbackRoutes = { nullptr, nullptr, nullptr };

#define CALLBACK_STUBS(_idx)                                                                                                                                                                    \
    inline void bufferSwitch##_idx( long index, ASIOBool processNow )                                   { gCallbackRoutes[_idx]->bufferSwitch(index, processNow); }                             \
    inline ASIOTime* bufferSwitchTimeInfo##_idx( ASIOTime* timeInfo, long index, ASIOBool processNow )  { return gCallbackRoutes[_idx]->bufferSwitchTimeInfo( timeInfo, index, processNow ); }  \
    inline void sampleRateDidChange##_idx( ASIOSampleRate sRate )                                       { gCallbackRoutes[_idx]->sampleRateDidChange( sRate ); }                                \
    inline long asioMessage##_idx( long selector, long value, void* message, double* opt )              { return gCallbackRoutes[_idx]->asioMessage( selector, value, message, opt ); }
CALLBACK_STUBS( 0 )
CALLBACK_STUBS( 1 )
CALLBACK_STUBS( 2 )

} // namespace callbacks


// ---------------------------------------------------------------------------------------------------------------------
inline const char* errorToString( const ASIOError err )
{
    switch ( err )
    {
        case ASE_OK:                        return "ASE_OK";
        case ASE_SUCCESS:                   return "ASE_SUCCESS";
        case ASE_NotPresent:                return "ASE_NotPresent";
        case ASE_HWMalfunction:             return "ASE_HWMalfunction";
        case ASE_InvalidParameter:          return "ASE_InvalidParameter";
        case ASE_InvalidMode:               return "ASE_InvalidMode";
        case ASE_SPNotAdvancing:            return "ASE_SPNotAdvancing";
        case ASE_NoClock:                   return "ASE_NoClock";
        case ASE_NoMemory:                  return "ASE_NoMemory";
    }
    return "Unknown_Error";
}

// ---------------------------------------------------------------------------------------------------------------------
inline const char* messageToString( const long msgID )
{
    switch ( msgID )
    {
        case kAsioSelectorSupported:        return "kAsioSelectorSupported";
        case kAsioEngineVersion:            return "kAsioEngineVersion";
        case kAsioResetRequest:             return "kAsioResetRequest";
        case kAsioBufferSizeChange:         return "kAsioBufferSizeChange";
        case kAsioResyncRequest:            return "kAsioResyncRequest";
        case kAsioLatenciesChanged:         return "kAsioLatenciesChanged";
        case kAsioSupportsTimeInfo:         return "kAsioSupportsTimeInfo";
        case kAsioSupportsTimeCode:         return "kAsioSupportsTimeCode";
        case kAsioMMCCommand:               return "kAsioMMCCommand";
        case kAsioSupportsInputMonitor:     return "kAsioSupportsInputMonitor";
        case kAsioSupportsInputGain:        return "kAsioSupportsInputGain";
        case kAsioSupportsInputMeter:       return "kAsioSupportsInputMeter";
        case kAsioSupportsOutputGain:       return "kAsioSupportsOutputGain";
        case kAsioSupportsOutputMeter:      return "kAsioSupportsOutputMeter";
        case kAsioOverload:                 return "kAsioOverload";
        case kAsioNumMessageSelectors:      return "kAsioNumMessageSelectors";
    }
    return "Unknown_Message";
}

// ---------------------------------------------------------------------------------------------------------------------
inline const char* sampleTypeToString( const long sampleType )
{
    switch ( sampleType )
    {
        case ASIOSTInt16MSB:                return "Int16 MSB";
        case ASIOSTInt24MSB:                return "Int24 MSB";
        case ASIOSTInt32MSB:                return "Int32 MSB";
        case ASIOSTFloat32MSB:              return "Float32 MSB";
        case ASIOSTFloat64MSB:              return "Float64 MSB";
        case ASIOSTInt32MSB16:              return "Int32 MSB-16";
        case ASIOSTInt32MSB18:              return "Int32 MSB-18";
        case ASIOSTInt32MSB20:              return "Int32 MSB-20";
        case ASIOSTInt32MSB24:              return "Int32 MSB-24";
        case ASIOSTInt16LSB:                return "Int16 LSB";
        case ASIOSTInt24LSB:                return "Int24 LSB";
        case ASIOSTInt32LSB:                return "Int32 LSB";
        case ASIOSTFloat32LSB:              return "Float32 LSB";
        case ASIOSTFloat64LSB:              return "Float64 LSB";
        case ASIOSTInt32LSB16:              return "Int32 LSB-16";
        case ASIOSTInt32LSB18:              return "Int32 LSB-18";
        case ASIOSTInt32LSB20:              return "Int32 LSB-20";
        case ASIOSTInt32LSB24:              return "Int32 LSB-24";
        case ASIOSTDSDInt8LSB1:             return "DSDInt8 LSB-1";
        case ASIOSTDSDInt8MSB1:             return "DSDInt8 MSB-1";
        case ASIOSTDSDInt8NER8:             return "DSDInt8 NER-8";
    }
    return "Unknown_SampleType";
}

} // namespace asio

// ---------------------------------------------------------------------------------------------------------------------
// the ASIO COM interface spec
interface IASIO : public IUnknown
{
    virtual ASIOBool init( void* sysHandle ) = 0;
    virtual void getDriverName( char* name ) = 0;
    virtual long getDriverVersion() = 0;
    virtual void getErrorMessage( char* string ) = 0;
    virtual ASIOError start() = 0;
    virtual ASIOError stop() = 0;
    virtual ASIOError getChannels( long* numInputChannels, long* numOutputChannels ) = 0;
    virtual ASIOError getLatencies( long* inputLatency, long* outputLatency ) = 0;
    virtual ASIOError getBufferSize( long* minSize, long* maxSize, long* preferredSize, long* granularity ) = 0;
    virtual ASIOError canSampleRate( ASIOSampleRate sampleRate ) = 0;
    virtual ASIOError getSampleRate( ASIOSampleRate* sampleRate ) = 0;
    virtual ASIOError setSampleRate( ASIOSampleRate sampleRate ) = 0;
    virtual ASIOError getClockSources( ASIOClockSource* clocks, long* numSources ) = 0;
    virtual ASIOError setClockSource( long reference ) = 0;
    virtual ASIOError getSamplePosition( ASIOSamples* sPos, ASIOTimeStamp* tStamp ) = 0;
    virtual ASIOError getChannelInfo( ASIOChannelInfo* info ) = 0;
    virtual ASIOError createBuffers( ASIOBufferInfo* bufferInfos, long numChannels, long bufferSize, ASIOCallbacks* callbacks ) = 0;
    virtual ASIOError disposeBuffers() = 0;
    virtual ASIOError controlPanel() = 0;
    virtual ASIOError future( long selector, void* opt ) = 0;
    virtual ASIOError outputReady() = 0;
};
