//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  VST 2.x plugin loading support
//

#include "pch.h"

#if OURO_FEATURE_VST24

#include "app/module.audio.h"
#include "app/module.midi.msg.h"

#include "nlohmann/json.hpp"

#include "base/utils.h"
#include "base/instrumentation.h"

#include "effect/vst2/host.h"

#include "vst_2_4/aeffectx.h"

// enable to dump out what the input and output pin properties are
#define VST_IO_PROPERTY_VERBOSE 0

static const std::string c_flagFormatWhiteSpaces{ " |" };

// ---------------------------------------------------------------------------------------------------------------------
using VSTPluginMainFn = AEffect* (*) (audioMasterCallback audioMaster);

inline std::string vstTimeInfoFlagsToString( VstTimeInfoFlags flags )
{
    std::string result = "";
    if ( ( flags & kVstTransportChanged     ) == kVstTransportChanged )     result += "TransportChanged | ";
    if ( ( flags & kVstTransportPlaying     ) == kVstTransportPlaying )     result += "TransportPlaying | ";
    if ( ( flags & kVstTransportCycleActive ) == kVstTransportCycleActive ) result += "TransportCycleActive | ";
    if ( ( flags & kVstTransportRecording   ) == kVstTransportRecording )   result += "TransportRecording | ";
    if ( ( flags & kVstAutomationWriting    ) == kVstAutomationWriting )    result += "AutomationWriting | ";
    if ( ( flags & kVstAutomationReading    ) == kVstAutomationReading )    result += "AutomationReading | ";
    if ( ( flags & kVstNanosValid           ) == kVstNanosValid )           result += "NanosValid | ";
    if ( ( flags & kVstPpqPosValid          ) == kVstPpqPosValid )          result += "PpqPosValid | ";
    if ( ( flags & kVstTempoValid           ) == kVstTempoValid )           result += "TempoValid | ";
    if ( ( flags & kVstBarsValid            ) == kVstBarsValid )            result += "BarsValid | ";
    if ( ( flags & kVstCyclePosValid        ) == kVstCyclePosValid )        result += "CyclePosValid | ";
    if ( ( flags & kVstTimeSigValid         ) == kVstTimeSigValid )         result += "TimeSigValid | ";
    if ( ( flags & kVstSmpteValid           ) == kVstSmpteValid )           result += "SmpteValid | ";
    if ( ( flags & kVstClockValid           ) == kVstClockValid )           result += "ClockValid | ";

    base::trimRight( result, c_flagFormatWhiteSpaces );
    return result;
}

inline std::string vstPinPropertiesFlagsToString( VstPinPropertiesFlags flags )
{
    std::string result = "";
    if ( ( flags & kVstPinIsActive          ) == kVstPinIsActive )          result += "Active | ";
    if ( ( flags & kVstPinIsStereo          ) == kVstPinIsStereo )          result += "StereoLeft | ";
    if ( ( flags & kVstPinUseSpeaker        ) == kVstPinUseSpeaker )        result += "UseSpeaker | ";

    base::trimRight( result, c_flagFormatWhiteSpaces );
    return result;
}

inline const char* vstOpcodeToString( const VstInt32 opcode )
{
    switch ( opcode )
    {
        case audioMasterGetTime:                   return "GetTime";
        case audioMasterProcessEvents:             return "ProcessEvents";
        case audioMasterIOChanged:                 return "IOChanged";
        case audioMasterSizeWindow:                return "SizeWindow";
        case audioMasterGetSampleRate:             return "GetSampleRate";
        case audioMasterGetBlockSize:              return "GetBlockSize";
        case audioMasterGetInputLatency:           return "GetInputLatency";
        case audioMasterGetOutputLatency:          return "GetOutputLatency";
        case audioMasterGetCurrentProcessLevel:    return "GetCurrentProcessLevel";
        case audioMasterGetAutomationState:        return "GetAutomationState";
        case audioMasterOfflineStart:              return "OfflineStart";
        case audioMasterOfflineRead:               return "OfflineRead";
        case audioMasterOfflineWrite:              return "OfflineWrite";
        case audioMasterOfflineGetCurrentPass:     return "OfflineGetCurrentPass";
        case audioMasterOfflineGetCurrentMetaPass: return "OfflineGetCurrentMetaPass";
        case audioMasterGetVendorString:           return "GetVendorString";
        case audioMasterGetProductString:          return "GetProductString";
        case audioMasterGetVendorVersion:          return "GetVendorVersion";
        case audioMasterVendorSpecific:            return "VendorSpecific";
        case audioMasterCanDo:                     return "CanDo";
        case audioMasterGetLanguage:               return "GetLanguage";
        case audioMasterGetDirectory:              return "GetDirectory";
        case audioMasterUpdateDisplay:             return "UpdateDisplay";
        case audioMasterBeginEdit:                 return "BeginEdit";
        case audioMasterEndEdit:                   return "EndEdit";
        case audioMasterOpenFileSelector:          return "OpenFileSelector";
        case audioMasterCloseFileSelector:         return "CloseFileSelector";
    }
    return "unknown_opcode";
}

// ---------------------------------------------------------------------------------------------------------------------
namespace vst {

bool Instance::DebugVerboseParameterLogging = false;

static constexpr wchar_t const* _vstWindowClass = L"_VSTOuro";

// ---------------------------------------------------------------------------------------------------------------------
struct Instance::Data
{
    static constexpr auto wStyle    = WS_POPUPWINDOW | WS_OVERLAPPED | WS_CAPTION | WS_VISIBLE | WS_MINIMIZEBOX;
    static constexpr auto wStyleEx  = WS_EX_PALETTEWINDOW | WS_EX_TOOLWINDOW;

    Data()
        : m_unifiedTimeInfo( nullptr )
        , m_vstTimeSampleRateRcp( 1.0 )
        , m_midiInputChannels( 0 )
        , m_midiOutputChannels( 0 )
        , m_updateIOChannels( false )
        , m_sampleRate( 0 )
        , m_maximumBlockSize( 0 )
        , m_automationCallbackFn( nullptr )
        , m_vstModule( nullptr )
        , m_vstEntrypoint( nullptr )
        , m_vstFilterInstance( nullptr )
        , m_vstThreadHandle( nullptr )
        , m_vstThreadAlive( false )
        , m_vstThreadInitComplete( false )
        , m_vstFailedToLoad( false )
        , m_vstEditorState( EditorState::Idle )
        , m_vstActivationState( ActivationState::Inactive )
        , m_vstActivationQueue( 2 )
        , m_vstEditorHWND( nullptr )
    {
        memset( &m_vstTimeInfo, 0, sizeof( m_vstTimeInfo ) );
    }

    static VstIntPtr __cdecl vstAudioMasterCallback( AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt );

    static HWND vstCreateWindow( uint32_t width, uint32_t height, Instance::Data* dataPtr );
    static LRESULT WINAPI vstMsgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
    static DWORD WINAPI vstThread( LPVOID lpParameter );

    void runOnVstThread();
    VstIntPtr safeDispatch( VstInt32 opCode, VstInt32 index, VstIntPtr value, void* ptr, float opt );

    // called from vstMsgProc
    void handleHostWindowClosing()
    {
        blog::vst( " VST | editor closing" );
        m_vstEditorState = Instance::Data::EditorState::Closing;
    }

    // flow for opening and closing the VST editor gracefully
    enum class EditorState
    {
        Idle,
        WaitingToOpen,
        Opening,
        Open,
        WaitingToClose,
        Closing,
    };
    using EditorStateAtomic = std::atomic< EditorState >;

    // flow for turning VST on/off via thread
    enum class ActivationState
    {
        Active,
        Deactivating,
        Inactive,
        Activating
    };
    using ActivationStateAtomic = std::atomic< ActivationState >;
    using ActivationQueue       = mcc::ReaderWriterQueue<ActivationState>;

    using ParameterIndexMapS2I  = std::unordered_map< std::string, int32_t >;
    using ParameterIndexMapI2S  = std::unordered_map< int32_t, std::string >;

    inline const char* lookupParameterName( const int32_t index )
    {
        auto paramIt = m_paramIndexMapI2S.find( index );
        if ( paramIt != m_paramIndexMapI2S.end() )
            return (*paramIt).second.c_str();

        return "unknown_parameter";
    }


    static std::mutex       m_initMutex;                // used to sync some console output across VST start to avoid clashes

    const app::AudioPlaybackTimeInfo*  
                            m_unifiedTimeInfo;
    VstTimeInfo             m_vstTimeInfo;
    double                  m_vstTimeSampleRateRcp;

    std::string             m_vendor;
    std::string             m_product;
    int32_t                 m_midiInputChannels;
    int32_t                 m_midiOutputChannels;
    std::atomic_bool        m_updateIOChannels;

    std::string             m_path;
    float                   m_sampleRate;
    uint32_t                m_maximumBlockSize;

    ParameterIndexMapS2I    m_paramIndexMapS2I;
    ParameterIndexMapI2S    m_paramIndexMapI2S;
    AutomationCallbackFn    m_automationCallbackFn;

    HMODULE                 m_vstModule;
    VSTPluginMainFn         m_vstEntrypoint;
    AEffect*                m_vstFilterInstance;
    HANDLE                  m_vstThreadHandle;
    std::atomic_bool        m_vstThreadAlive;
    std::atomic_bool        m_vstThreadInitComplete;
    std::atomic_bool        m_vstFailedToLoad;

    EditorStateAtomic       m_vstEditorState;           // request to open/close
    ActivationStateAtomic   m_vstActivationState;       // request to enable/disable (post boot)
    ActivationQueue         m_vstActivationQueue;
    HWND                    m_vstEditorHWND;
};

std::mutex Instance::Data::m_initMutex;

// ---------------------------------------------------------------------------------------------------------------------
void Instance::RegisterWndClass()
{
    WNDCLASSEX wc =
    {
        sizeof( WNDCLASSEX ),
        0,
        Data::vstMsgProc,
        0L,
        0L,
        GetModuleHandle( nullptr ),
        nullptr,
        LoadCursor( nullptr, IDC_ARROW ),
        nullptr,
        nullptr,
        _vstWindowClass,
        nullptr
    };
    ::RegisterClassEx( &wc );
}

// ---------------------------------------------------------------------------------------------------------------------
void Instance::UnregisterWndClass()
{
    ::UnregisterClass( _vstWindowClass, GetModuleHandle( nullptr ) );
}

// ---------------------------------------------------------------------------------------------------------------------
Instance::Instance( const char* pluginPath, const float sampleRate, const uint32_t maximumBlockSize, const app::AudioPlaybackTimeInfo* unifiedTime )
    : m_userdata( 0 )
{
    m_uid = CityHash32( pluginPath, strlen( pluginPath ) );

    m_data = std::make_unique< Instance::Data >();
    m_data->m_path                      = pluginPath;
    m_data->m_sampleRate                = sampleRate;
    m_data->m_maximumBlockSize          = maximumBlockSize;

    assert( unifiedTime != nullptr );
    m_data->m_unifiedTimeInfo           = unifiedTime;
    m_data->m_vstTimeInfo.sampleRate    = sampleRate;
    m_data->m_vstTimeSampleRateRcp      = 1.0 / sampleRate;
}

// ---------------------------------------------------------------------------------------------------------------------
Instance::~Instance()
{
    m_data->m_vstThreadAlive = false;
    ::WaitForSingleObject( m_data->m_vstThreadHandle, INFINITE );
}

// ---------------------------------------------------------------------------------------------------------------------
VstIntPtr __cdecl Instance::Data::vstAudioMasterCallback( AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt )
{
    static constexpr double cSixtyRcp = 1.0 / 60.0;

    if ( opcode == audioMasterVersion )
        return kVstVersion;

    VstIntPtr result = 0;
    if ( effect == nullptr )
    {
        if ( opcode == audioMasterGetVendorString       ||
             opcode == audioMasterGetProductString      ||
             opcode == audioMasterGetVendorVersion      ||
             opcode == audioMasterGetCurrentProcessLevel )
        {
            // these are stateless so it's okay if effect is null; this has been seen happen on some iZo plugs
        }
        else
        {
            // .. but otherwise this is an error, we need the userdata in effect to reach back to our host instance
            blog::error::vst( " - vstAudioMasterCallback effect is nullptr for opcode {} ({})", opcode, vstOpcodeToString( opcode ) );
            return 0;
        }
    }

    switch ( opcode )
    {
        case audioMasterGetTime:
        {
            // PpqPosValid | TempoValid | BarsValid | CyclePosValid | TimeSigValid | SmpteValid | ClockValid |
            Instance::Data* localData = (Instance::Data*)effect->user;
            if ( localData )
            {
                assert( localData->m_unifiedTimeInfo != nullptr );
                const auto& unifiedTime = *localData->m_unifiedTimeInfo;
                      auto& vstTime     =  localData->m_vstTimeInfo;

                vstTime.samplePos           = unifiedTime.samplePos;
                vstTime.tempo               = unifiedTime.tempo;
                vstTime.sampleRate          = localData->m_sampleRate;
                vstTime.ppqPos              = (vstTime.samplePos * localData->m_vstTimeSampleRateRcp) * (vstTime.tempo * cSixtyRcp);
                vstTime.timeSigNumerator    = unifiedTime.timeSigNumerator;
                vstTime.timeSigDenominator  = unifiedTime.timeSigDenominator;
                vstTime.flags               = kVstTempoValid | kVstPpqPosValid | kVstTimeSigValid | kVstTransportPlaying;

                return (VstIntPtr)&localData->m_vstTimeInfo;
            }
            else
            {
                return (VstIntPtr)nullptr;
            }
        }
        break;

        case audioMasterAutomate:
        case audioMasterBeginEdit:
        case audioMasterEndEdit:
        {
            // if there is an automation callback registered, ping it and then release it
            Instance::Data* localData = (Instance::Data*)effect->user;
            if ( localData )
            {
                if ( Instance::DebugVerboseParameterLogging )
                {
                    blog::vst( "parameter #{} [{}] = {}",
                        index, 
                        localData->lookupParameterName( index ), 
                        effect->getParameter( effect, index ) );
                }

                if ( localData->m_automationCallbackFn )
                {
                    const float paramValue = effect->getParameter( effect, index );

                    localData->m_automationCallbackFn( index, localData->lookupParameterName( index ), paramValue );
                    localData->m_automationCallbackFn = nullptr;
                }
            }
        }
        break;

        case audioMasterGetCurrentProcessLevel:
            return kVstProcessLevelRealtime;
            break;

        // still get this obsolete'd msg regularly; respond to identify what inputs we support
        case 4 /*audioMasterPinConnected*/:
        {
            const bool askingAboutInput = (value != 0);
            if ( askingAboutInput )
                return (index < 4) ? 0 : 1; // 0 = yes!
            else
                return (index < 2) ? 0 : 1;
        }

        case audioMasterCurrentId:
            return effect->uniqueID;

        case audioMasterIdle:
            effect->dispatcher( effect, effEditIdle, 0, 0, nullptr, 0 );
            break;

        case audioMasterSizeWindow:
            {
                Instance::Data* localData = (Instance::Data*)effect->user;
                if ( localData )
                {
                    RECT wr = { 0, 0, (LONG)index, (LONG)value };
                    ::AdjustWindowRectEx( &wr, wStyle, FALSE, wStyleEx );
                    ::SetWindowPos( localData->m_vstEditorHWND, nullptr, 0, 0, wr.right, wr.bottom, SWP_NOMOVE | SWP_NOREPOSITION | SWP_NOZORDER );
                }
            }
            break;

        case audioMasterCanDo:
            if ( ptr != nullptr )
                blog::vst( " - audioMasterCanDo ({})", (const char*)ptr );
            break;

        case audioMasterIOChanged:
            {
                Instance::Data* localData = (Instance::Data*)effect->user;
                if ( localData )
                {
                    localData->m_updateIOChannels = true;
                }
            }
            return 1;

        case audioMasterGetVendorString:
            strcpy_s( (char*)ptr, kVstMaxVendorStrLen, "ishani.org" );
            break;
        case audioMasterGetProductString:
            strcpy_s( (char*)ptr, kVstMaxVendorStrLen, "OUROVEON" );
            break;
        case audioMasterGetVendorVersion:
            return 0x666;

        default:
            blog::vst( " - vstAudioMasterCallback unhandled opcode {} ({})", opcode, vstOpcodeToString(opcode) );
            break;
    }
    return result;
}

// ---------------------------------------------------------------------------------------------------------------------
LRESULT WINAPI Instance::Data::vstMsgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    Instance::Data* vstData = (Instance::Data*)::GetWindowLongPtr( hWnd, GWLP_USERDATA );

    switch ( msg )
    {
        case WM_CREATE:
        {
            vstData = (Instance::Data*)((LPCREATESTRUCT)lParam)->lpCreateParams;
            assert( vstData != nullptr );
            ::SetWindowLongPtr( hWnd, GWLP_USERDATA, (LONG_PTR) vstData );

            vstData->m_vstEditorState = Instance::Data::EditorState::Open;
            blog::vst( "opening editor for {}", vstData->m_product );
        }
        break;

        case WM_DESTROY:
        {
            assert( vstData != nullptr );
            vstData->handleHostWindowClosing();
        }
        break;
    }
    return DefWindowProc( hWnd, msg, wParam, lParam );
}

// ---------------------------------------------------------------------------------------------------------------------
HWND  Instance::Data::vstCreateWindow( uint32_t width, uint32_t height, Instance::Data* dataPtr )
{
    RECT wr = { 0, 0, (LONG)width, (LONG)height };
    ::AdjustWindowRectEx( &wr, wStyle, FALSE, wStyleEx );

    return ::CreateWindowEx(
        wStyleEx,
        _vstWindowClass,
        L"VST",
        wStyle,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        wr.right - wr.left,
        wr.bottom - wr.top,
        nullptr,
        nullptr,
        GetModuleHandle( nullptr ),
        (LPVOID)dataPtr );
}

// ---------------------------------------------------------------------------------------------------------------------
DWORD WINAPI Instance::Data::vstThread( LPVOID lpParameter )
{
    // pass control over to the instance data
    auto* vstData = (Instance::Data*)lpParameter;
    vstData->runOnVstThread();

    return 0;
}

// ---------------------------------------------------------------------------------------------------------------------
void Instance::Data::runOnVstThread()
{
    // support 2+2 in (stereo + sidechain) and stereo out
    const int32_t cTotalKnownInputs  = 4;
    const int32_t cTotalKnownOutputs = 2;

    blog::vst( "trying to load [{}]", m_path );

    m_vstModule = LoadLibraryExA( m_path.c_str(), nullptr, 0 );
    if ( m_vstModule != nullptr )
    {
        m_vstEntrypoint = (VSTPluginMainFn)GetProcAddress( m_vstModule, "VSTPluginMain" );
        if ( m_vstEntrypoint == nullptr )
        {
            blog::error::vst( "VSTPluginMain entrypoint not found" );
            FreeLibrary( m_vstModule );

            m_vstFailedToLoad = true;
            return;
        }
    }

    auto* vstFI = m_vstEntrypoint( vstAudioMasterCallback );
    vstFI->user = this;

    m_vstFilterInstance = vstFI;

    VstIntPtr dispRet;
    dispRet = safeDispatch( effOpen, 0, 0, nullptr, 0 );

    const auto vstVersion = safeDispatch( effGetVstVersion, 0, 0, nullptr, 0 );
    if ( vstVersion >= 2 )
    {
        VstSpeakerArrangement saInput, saOutput;
        memset( &saInput, 0, sizeof( saInput ) );
        memset( &saOutput, 0, sizeof( saOutput ) );

        saInput.numChannels  = cTotalKnownInputs;
        saInput.type         = kSpeakerArrStereo;
        saOutput.numChannels = cTotalKnownOutputs;
        saOutput.type        = kSpeakerArrStereo;

        for ( int i = 0; i < 8; i++ )
        {
            saInput.speakers[i].azimuth   = saOutput.speakers[i].azimuth   = 0.0f;
            saInput.speakers[i].elevation = saOutput.speakers[i].elevation = 0.0f;
            saInput.speakers[i].radius    = saOutput.speakers[i].radius    = 0.0f;
            saInput.speakers[i].reserved  = saOutput.speakers[i].reserved  = 0.0f;

            switch ( i )
            {
                case 0:
                    saInput.speakers[i].type  = kSpeakerL;
                    saOutput.speakers[i].type = kSpeakerL;
                    strcpy_s( saInput.speakers[i].name, kVstMaxNameLen, "In-Left" );
                    strcpy_s( saOutput.speakers[i].name, kVstMaxNameLen, "Out-Left" );
                    break;
                case 1:
                    saInput.speakers[i].type  = kSpeakerR;
                    saOutput.speakers[i].type = kSpeakerR;
                    strcpy_s( saInput.speakers[i].name, kVstMaxNameLen, "In-Right" );
                    strcpy_s( saOutput.speakers[i].name, kVstMaxNameLen, "Out-Right" );
                    break;
                case 2:
                    saInput.speakers[i].type = kSpeakerSl;  // ??
                    strcpy_s( saInput.speakers[i].name, kVstMaxNameLen, "Sidechain-Left" );
                    saOutput.speakers[i].type = kSpeakerUndefined;
                    break;
                case 3:
                    saInput.speakers[i].type = kSpeakerSr;  // ??
                    strcpy_s( saInput.speakers[i].name, kVstMaxNameLen, "Sidechain-Right" );
                    saOutput.speakers[i].type = kSpeakerUndefined;
                    break;
                default:
                    saInput.speakers[i].type  = kSpeakerUndefined;
                    saOutput.speakers[i].type = kSpeakerUndefined;
                    break;
            }
        }
        safeDispatch( effSetSpeakerArrangement, 0, ToVstPtr( &saInput ), &saOutput, 0.0f );

        VstPinProperties ioPinProperties;
        for ( int32_t index = 0; index < cTotalKnownInputs; index ++ )
        {
            safeDispatch( effGetInputProperties, index, 0, &ioPinProperties, 0 );
#if VST_IO_PROPERTY_VERBOSE
            blog::vst( " - Input  {} = [ {:24} ] : [ {:32} ]", index, ioPinProperties.label, vstPinPropertiesFlagsToString( (VstPinPropertiesFlags)ioPinProperties.flags ) );
#endif // VST_IO_PROPERTY_VERBOSE
        }

        for ( int32_t index = 0; index < cTotalKnownOutputs; index ++ )
        {
            safeDispatch( effGetOutputProperties, index, 0, &ioPinProperties, 0 );
#if VST_IO_PROPERTY_VERBOSE
            blog::vst( " - Output {} = [ {:24} ] : [ {:32} ]", index, ioPinProperties.label, vstPinPropertiesFlagsToString( (VstPinPropertiesFlags)ioPinProperties.flags ) );
#endif // VST_IO_PROPERTY_VERBOSE
        }
    }

    dispRet = safeDispatch( effSetSampleRate, 0, 0, nullptr, m_sampleRate );
    dispRet = safeDispatch( effSetBlockSize,  0, m_maximumBlockSize, nullptr, 0 );
    dispRet = safeDispatch( effSetProcessPrecision, 0, kVstProcessPrecision32, nullptr, 0.0f );
    dispRet = safeDispatch( effSetSampleRate, 0, 0, nullptr, m_sampleRate );  // not a typo; some plugins like it before block-size, some after

    char vendorString[kVstMaxVendorStrLen]{ 0 };
    dispRet = safeDispatch( effGetVendorString, 0, 0, vendorString, 0 );
    m_vendor = vendorString;

    char productString[kVstMaxProductStrLen]{ 0 };
    dispRet = safeDispatch( effGetProductString, 0, 0, productString, 0 );
    m_product = productString;
    
    if ( m_product.empty() )
    {
        m_product = fs::path( m_path ).filename().string();
        m_product = m_product.substr( 0, m_product.size() - 4 );
    }

    OuroveonThreadScope ots( fmt::format( OURO_THREAD_PREFIX "VST::{}", m_product ).c_str() );

    // update on first time through
    m_updateIOChannels = true;

    m_vstThreadInitComplete = true;

    // extract parameters
    {
        std::scoped_lock syncConsoleOutput( m_initMutex );
        blog::vst( "Loaded [ {} - {} ]", vendorString, productString );

        char paramLabel[255];
        for ( int32_t pI = 0; pI < vstFI->numParams; pI++ )
        {
            safeDispatch( effGetParamName, pI, 0, paramLabel, 0 );

            m_paramIndexMapS2I.emplace( paramLabel, pI );
            m_paramIndexMapI2S.emplace( pI, paramLabel );
#if 0
            blog::vst( "  Param({}) = {}", pI, paramLabel );
#endif
        }
    }

    while ( m_vstThreadAlive )
    {
        if ( m_vstActivationState == ActivationState::Deactivating )
        {
            dispRet = safeDispatch( effMainsChanged, 0, 0, nullptr, 0 );
            m_vstActivationState = ActivationState::Inactive;
        }
        else if ( m_vstActivationState == ActivationState::Activating )
        {
            dispRet = safeDispatch( effMainsChanged, 0, 1, nullptr, 0 );
            m_vstActivationState = ActivationState::Active;
        }

        // handle requests to open or close the UI
        if ( m_vstEditorState == EditorState::WaitingToOpen )
        {
            m_vstEditorState = EditorState::Opening;

            ERect* editorRect;
            safeDispatch( effEditGetRect, 0, 0, &editorRect, 0 );

            m_vstEditorHWND = Instance::Data::vstCreateWindow(
                editorRect->right - editorRect->left,
                editorRect->bottom - editorRect->top,
                this );

            safeDispatch( effEditOpen, 0, 0, m_vstEditorHWND, 0 );
        }
        else if ( m_vstEditorState == EditorState::WaitingToClose )
        {
            // this will trigger change to ::Closing
            ::DestroyWindow( m_vstEditorHWND );
        }
        else if ( m_vstEditorState == EditorState::Closing )
        {
            m_vstEditorState = EditorState::Idle;
            m_vstEditorHWND = nullptr;

            safeDispatch( effEditClose, 0, 0, nullptr, 0 );
        }

        // run the message loop for a while
        {
            if ( m_vstEditorHWND != nullptr )
            {
                int32_t maxDispatch = 10; // arbitrary message read limit to avoid getting stuck in an endless 
                                          // message loop without checking in on all the other bits of the VST thread logic

                MSG msg;
                //default message processing
                while ( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
                {
                    TranslateMessage( &msg );
                    DispatchMessage( &msg );

                    --maxDispatch;
                    if ( maxDispatch < 0 )
                        break;
                }
            }
        }
        Sleep( 10 );

        if ( m_updateIOChannels )
        {
            m_midiInputChannels     = (int32_t)safeDispatch( effGetNumMidiInputChannels, 0, 0, nullptr, 0 );
            m_midiOutputChannels    = (int32_t)safeDispatch( effGetNumMidiOutputChannels, 0, 0, nullptr, 0 );
            m_updateIOChannels      = false;

            blog::vst( "MIDI Channels [{}] : In {} : Out {} ", productString, m_midiInputChannels, m_midiOutputChannels );
        }


        // process requests from main thread
        ActivationState requestedActivation;
        if ( m_vstActivationQueue.try_dequeue( requestedActivation ) )
        {
            m_vstActivationState = requestedActivation;
        }
    }

    m_vstThreadInitComplete = false;

    dispRet = safeDispatch( effClose, 0, 0, nullptr, 0 );
    m_vstFilterInstance = nullptr;

    FreeLibrary( m_vstModule );
}

// ---------------------------------------------------------------------------------------------------------------------
VstIntPtr Instance::Data::safeDispatch( VstInt32 opCode, VstInt32 index, VstIntPtr value, void* ptr, float opt )
{
    VstIntPtr result = 0;

    try
    {
        if ( m_vstFilterInstance->dispatcher != nullptr )
        {
            result = m_vstFilterInstance->dispatcher( m_vstFilterInstance, opCode, index, value, ptr, opt );
        }
    }
    catch ( ... )
    {
        blog::error::vst( "exception in SafeDispatch( {}, {}, {}, {}, {} )", opCode, index, value, ptr, opt  );
    }

    return result;
}

// ---------------------------------------------------------------------------------------------------------------------
void Instance::beginLoadAsync()
{
    // everything happens on the thread
    DWORD newThreadID;
    m_data->m_vstThreadAlive  = true;
    m_data->m_vstThreadHandle = ::CreateThread( nullptr, 0, Instance::Data::vstThread, m_data.get(), 0, &newThreadID );
                                ::SetThreadPriority( m_data->m_vstThreadHandle, THREAD_PRIORITY_ABOVE_NORMAL );

    blog::vst( "scheduled VST load on thread [{}]", newThreadID );
}

// ---------------------------------------------------------------------------------------------------------------------
bool Instance::loaded() const
{
    return ( m_data->m_vstFilterInstance != nullptr &&
             m_data->m_vstThreadAlive &&
             m_data->m_vstThreadInitComplete );
}

// ---------------------------------------------------------------------------------------------------------------------
bool Instance::availableForUse() const
{
    return ( loaded() && 
             isActive() );
}

bool Instance::failedToLoad() const
{
    return m_data->m_vstFailedToLoad;
}

// ---------------------------------------------------------------------------------------------------------------------
const std::string Instance::getPath() const
{
    return m_data->m_path;
}

// ---------------------------------------------------------------------------------------------------------------------
const std::string& Instance::getVendorName() const
{
    static std::string sUnknown = "unknown_vendor";
    if ( !loaded() )
        return sUnknown;

    return m_data->m_vendor;
}

// ---------------------------------------------------------------------------------------------------------------------
const std::string& Instance::getProductName() const
{
    static std::string sUnknown = "unknown_product";
    if ( !loaded() )
        return sUnknown;

    return m_data->m_product;
}

// ---------------------------------------------------------------------------------------------------------------------
const int32_t Instance::getUniqueID() const
{
    if ( !loaded() )
        return -1;

    return m_data->m_vstFilterInstance->uniqueID;
}

// ---------------------------------------------------------------------------------------------------------------------
bool Instance::isActive() const
{
    return ( m_data->m_vstActivationState == Instance::Data::ActivationState::Active );
}

// ---------------------------------------------------------------------------------------------------------------------
void Instance::requestActivationChange( bool onOff )
{
    m_data->m_vstActivationQueue.enqueue( onOff ?
        Instance::Data::ActivationState::Activating :
        Instance::Data::ActivationState::Deactivating );
}

// ---------------------------------------------------------------------------------------------------------------------
bool Instance::canChangeEditorState() const
{
    return ( m_data->m_vstEditorState == Instance::Data::EditorState::Idle ||
             m_data->m_vstEditorState == Instance::Data::EditorState::Open );
}

// ---------------------------------------------------------------------------------------------------------------------
bool Instance::openEditor()
{
    if ( m_data->m_vstEditorState != Instance::Data::EditorState::Idle )
        return false;

    m_data->m_vstEditorState = Instance::Data::EditorState::WaitingToOpen;
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
bool Instance::closeEditor()
{
    if ( m_data->m_vstEditorState != Instance::Data::EditorState::Open )
        return false;

    m_data->m_vstEditorState = Instance::Data::EditorState::WaitingToClose;
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
bool Instance::editorIsOpen() const
{
    return m_data->m_vstEditorState == Instance::Data::EditorState::Open;
}

// ---------------------------------------------------------------------------------------------------------------------
int32_t Instance::lookupParameterIndex( const std::string& byName )
{
    auto paramIt = m_data->m_paramIndexMapS2I.find( byName );
    if ( paramIt != m_data->m_paramIndexMapS2I.end() )
        return (*paramIt).second;

    return -1;
}

// ---------------------------------------------------------------------------------------------------------------------
const char* Instance::lookupParameterName( const int32_t index )
{
    return m_data->lookupParameterName(index);
}

// ---------------------------------------------------------------------------------------------------------------------
bool Instance::setParameter( const int32_t index, const float value )
{
    if ( availableForUse() && 
         index >= 0 )
    {
        m_data->m_vstFilterInstance->setParameter( m_data->m_vstFilterInstance, index, value );
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------------------------------------------------
void Instance::requestAutomationHook( const AutomationCallbackFn& hookFn )
{
    m_data->m_automationCallbackFn = hookFn;
}

// ---------------------------------------------------------------------------------------------------------------------
bool Instance::hasAutomationHook() const
{
    return (m_data->m_automationCallbackFn != nullptr);
}

#define OUROVEON_MAGIC_PARAMBLOCK_STR   "OUROVEON########"

// ---------------------------------------------------------------------------------------------------------------------
std::string Instance::serialize()
{
    void* chunkData = nullptr;
    VstIntPtr chunkSize = m_data->m_vstFilterInstance->dispatcher( m_data->m_vstFilterInstance, effGetChunk, 0, 0, &chunkData, 0 );

    // VST supported getChunk, hooray
    if ( chunkSize > 0 )
    {
        return cppcodec::base64_rfc4648::encode( (const char*)chunkData, chunkSize );
    }
    
    // the manual fallback - fetch all the parameters we know about and save them to a JSON stream
    {
        std::string manualParamExport;
        manualParamExport.reserve( m_data->m_vstFilterInstance->numParams * 64 );

        // to differentiate us from the normal GetChunk data, prepend a clear magic ID we can look for on deserialize
        manualParamExport = OUROVEON_MAGIC_PARAMBLOCK_STR "{\n";

        for ( const auto& s2i : m_data->m_paramIndexMapS2I )
        {
            const float currentValue = m_data->m_vstFilterInstance->getParameter( m_data->m_vstFilterInstance, s2i.second );

            // save the values as hex-digit blobs to avoid any round-trip loss of precision
            uint32_t floatToHex;
            memcpy( &floatToHex, &currentValue, sizeof( float ) );

            manualParamExport += fmt::format( "  \"{}\" : \"{:#x}\",\n", s2i.first, floatToHex );
        }
        manualParamExport += fmt::format( "  \"_vst_version\" : {}\n", m_data->m_vstFilterInstance->version );
        manualParamExport += "}\0\0\n";

        blog::vst( ".. manual parameter serialize ({} bytes)", manualParamExport.size() );

        return cppcodec::base64_rfc4648::encode( (const char*)manualParamExport.c_str(), manualParamExport.size() + 1 );
    }
}

// ---------------------------------------------------------------------------------------------------------------------
bool Instance::deserialize( const std::string& data )
{
    try
    {
        auto chunkData = cppcodec::base64_rfc4648::decode( data );
        if ( chunkData.size() <= 0 )
        {
            blog::error::vst( "VSTInstance::deserialize data empty" );
            return false;
        }

        // check for the magic key that indicates the data is one of our manual parameter encodings
        const size_t sizeOfOuroMagic = strlen( OUROVEON_MAGIC_PARAMBLOCK_STR );
        if ( memcmp( chunkData.data(), OUROVEON_MAGIC_PARAMBLOCK_STR, sizeOfOuroMagic ) == 0 )
        {
            blog::vst( ".. manual parameter deserialize" );

            // .. if it is, decode the JSON and walk the parameters, setting all the ones we can reliably find in our existing k:v name map
            const char* manualParamJson = (const char* )chunkData.data() + sizeOfOuroMagic;
            auto paramMap = nlohmann::json::parse( manualParamJson );

            for ( const auto kvEntry : paramMap.items() )
            {
                const auto paramLookup = m_data->m_paramIndexMapS2I.find( kvEntry.key() );
                if ( paramLookup != m_data->m_paramIndexMapS2I.end() )
                {
                    // floats are stores as hex strings, parse back to uint32
                    const uint32_t floatAsHex = std::stoul( std::string( kvEntry.value() ), nullptr, 16 );

                    // .. and then copy back into their true form
                    float newValue;
                    memcpy( &newValue, &floatAsHex, sizeof( uint32_t ) );

                    m_data->m_vstFilterInstance->setParameter( m_data->m_vstFilterInstance, paramLookup->second, newValue );
                }
            }
        }
        else
        {
            VstIntPtr chunkSize = m_data->m_vstFilterInstance->dispatcher( m_data->m_vstFilterInstance, effSetChunk, 0, chunkData.size(), chunkData.data(), 0 );
        }
    }
    catch ( nlohmann::json::parse_error& e )
    {
        blog::error::vst( "VSTInstance::deserialize json parse failure, {}", e.what() );
        return false;
    }
    catch ( cppcodec::parse_error* e)
    {
        blog::error::vst( "VSTInstance::deserialize failed, {}", e->what() );
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
void Instance::process( float** inputs, float** outputs, const int32_t sampleFrames )
{
    m_data->m_vstFilterInstance->processReplacing( m_data->m_vstFilterInstance, inputs, outputs, sampleFrames );
}

/*
void Instance::dispatchMidi( const app::midi::Message& midiMsg )
{
    if ( loaded() && m_data->m_midiInputChannels > 0 )
    {
        VstEvents events;
        events.numEvents = 1;
        events.reserved = 0;

        VstMidiEvent midiEvent;
        memset( &midiEvent, 0, sizeof( VstMidiEvent ) );
        midiEvent.type      = kVstMidiType;
        midiEvent.byteSize  = sizeof( VstMidiEvent );
        midiEvent.flags     = kVstMidiEventIsRealtime;

        midiEvent.midiData[0] = app::midi::Message::typeAsByte( midiMsg.m_type );
        midiEvent.midiData[1] = midiMsg.m_data0_u7;
        midiEvent.midiData[2] = midiMsg.m_data1_u7;

        events.events[0] = (VstEvent *)&midiEvent;

        m_data->safeDispatch( effProcessEvents, 0, 0, &events, 0 );
    }
}
*/

} // namespace vst

#endif // OURO_FEATURE_VST24
