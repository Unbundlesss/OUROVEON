//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  VST 2.x plugin loading support
//

#include "pch.h"

#if OURO_FEATURE_NST24

#include "app/module.audio.h"
#include "app/module.midi.msg.h"

#include "nlohmann/json.hpp"

// base64 codec; used for encoding serialized parameters
#include "base64_rfc4648.hpp"

#include "base/text.h"
#include "base/instrumentation.h"

#include "effect/vst2/host.h"

#include <nst24.h>

// enable to dump out what the input and output pin properties are
#define NST_IO_PROPERTY_DEBUG_VERBOSE 0

static const std::string c_flagFormatWhiteSpaces{ " |" };

// ---------------------------------------------------------------------------------------------------------------------
using VSTPluginMainFn = CPlugEffect* (*) (NstOpcodeCallback audioMaster);

inline std::string nstTimeInfoFlagsToString( NstTimeInfoFlags flags )
{
    std::string result = "";
    if ( ( flags & NstTIF_Transport_Changed         ) == NstTIF_Transport_Changed )      result += "TransportChanged | ";
    if ( ( flags & NstTIF_Transport_Playing         ) == NstTIF_Transport_Playing )      result += "TransportPlaying | ";
    if ( ( flags & NstTIF_Transport_Cycle_Active    ) == NstTIF_Transport_Cycle_Active ) result += "TransportCycleActive | ";
    if ( ( flags & NstTIF_Transport_Recording       ) == NstTIF_Transport_Recording )    result += "TransportRecording | ";
    if ( ( flags & NstTIF_Automation_Writing        ) == NstTIF_Automation_Writing )     result += "AutomationWriting | ";
    if ( ( flags & NstTIF_Automation_Reading        ) == NstTIF_Automation_Reading )     result += "AutomationReading | ";
    if ( ( flags & NstTIF_Nanos_Valid               ) == NstTIF_Nanos_Valid )            result += "NanosValid | ";
    if ( ( flags & NstTIF_PpqPos_Valid              ) == NstTIF_PpqPos_Valid )           result += "PpqPosValid | ";
    if ( ( flags & NstTIF_Tempo_Valid               ) == NstTIF_Tempo_Valid )            result += "TempoValid | ";
    if ( ( flags & NstTIF_Bars_Valid                ) == NstTIF_Bars_Valid )             result += "BarsValid | ";
    if ( ( flags & NstTIF_CyclePos_Valid            ) == NstTIF_CyclePos_Valid )         result += "CyclePosValid | ";
    if ( ( flags & NstTIF_TimeSig_Valid             ) == NstTIF_TimeSig_Valid )          result += "TimeSigValid | ";
    if ( ( flags & NstTIF_Smpte_Valid               ) == NstTIF_Smpte_Valid )            result += "SmpteValid | ";
    if ( ( flags & NstTIF_Clock_Valid               ) == NstTIF_Clock_Valid )            result += "ClockValid | ";

    base::trimRight( result, c_flagFormatWhiteSpaces );
    return result;
}

inline std::string nstPinPropertiesFlagsToString( NstPinPropertiesFlags flags )
{
    std::string result = "";
    if ( ( flags & NstPPF_Pin_Is_Active          ) == NstPPF_Pin_Is_Active )             result += "Active | ";
    if ( ( flags & NstPPF_Pin_Is_Stereo          ) == NstPPF_Pin_Is_Stereo )             result += "StereoLeft | ";
    if ( ( flags & NstPPF_Pin_Use_Speaker        ) == NstPPF_Pin_Use_Speaker )           result += "UseSpeaker | ";

    base::trimRight( result, c_flagFormatWhiteSpaces );
    return result;
}

inline const char* nstOpcodeToString( const int32_t opcode )
{
    switch ( opcode )
    {
        case NstOpcode_GetTime:                   return "GetTime";
        case NstOpcode_ProcessEvents:             return "ProcessEvents";
        case NstOpcode_IOChanged:                 return "IOChanged";
        case NstOpcode_SizeWindow:                return "SizeWindow";
        case NstOpcode_GetSampleRate:             return "GetSampleRate";
        case NstOpcode_GetBlockSize:              return "GetBlockSize";
        case NstOpcode_GetInputLatency:           return "GetInputLatency";
        case NstOpcode_GetOutputLatency:          return "GetOutputLatency";
        case NstOpcode_GetCurrentProcessLevel:    return "GetCurrentProcessLevel";
        case NstOpcode_GetAutomationState:        return "GetAutomationState";
        case NstOpcode_OfflineStart:              return "OfflineStart";
        case NstOpcode_OfflineRead:               return "OfflineRead";
        case NstOpcode_OfflineWrite:              return "OfflineWrite";
        case NstOpcode_OfflineGetCurrentPass:     return "OfflineGetCurrentPass";
        case NstOpcode_OfflineGetCurrentMetaPass: return "OfflineGetCurrentMetaPass";
        case NstOpcode_GetVendorString:           return "GetVendorString";
        case NstOpcode_GetProductString:          return "GetProductString";
        case NstOpcode_GetVendorVersion:          return "GetVendorVersion";
        case NstOpcode_VendorSpecific:            return "VendorSpecific";
        case NstOpcode_CanDo:                     return "CanDo";
        case NstOpcode_GetLanguage:               return "GetLanguage";
        case NstOpcode_GetDirectory:              return "GetDirectory";
        case NstOpcode_UpdateDisplay:             return "UpdateDisplay";
        case NstOpcode_BeginEdit:                 return "BeginEdit";
        case NstOpcode_EndEdit:                   return "EndEdit";
        case NstOpcode_OpenFileSelector:          return "OpenFileSelector";
        case NstOpcode_CloseFileSelector:         return "CloseFileSelector";
    }
    return "unknown_opcode";
}

// ---------------------------------------------------------------------------------------------------------------------
namespace nst {

bool Instance::DebugVerboseParameterLogging = false;

static constexpr wchar_t const* _nstWindowClass = L"_NSTOuro";

// ---------------------------------------------------------------------------------------------------------------------
struct Instance::Data
{
    static constexpr auto wStyle    = WS_POPUPWINDOW | WS_OVERLAPPED | WS_CAPTION | WS_VISIBLE | WS_MINIMIZEBOX;
    static constexpr auto wStyleEx  = WS_EX_PALETTEWINDOW | WS_EX_TOOLWINDOW;

    Data()
        : m_unifiedTimeInfo( nullptr )
        , m_nstTimeSampleRateRcp( 1.0 )
        , m_midiInputChannels( 0 )
        , m_midiOutputChannels( 0 )
        , m_updateIOChannels( false )
        , m_sampleRate( 0 )
        , m_maximumBlockSize( 0 )
        , m_automationCallbackFn( nullptr )
        , m_nstModule( nullptr )
        , m_nstEntrypoint( nullptr )
        , m_nstFilterInstance( nullptr )
        , m_nstThreadHandle( nullptr )
        , m_nstThreadAlive( false )
        , m_nstThreadInitComplete( false )
        , m_nstFailedToLoad( false )
        , m_nstEditorState( EditorState::Idle )
        , m_nstActivationState( ActivationState::Inactive )
        , m_nstActivationQueue( 2 )
        , m_nstEditorHWND( nullptr )
    {
        memset( &m_nstTimeInfo, 0, sizeof( m_nstTimeInfo ) );
    }

    static int64_t __cdecl NstOpcodeCallback( CPlugEffect* effect, int32_t opcode, int32_t index, int64_t value, void* ptr, float opt );

    static HWND nstCreateWindow( uint32_t width, uint32_t height, Instance::Data* dataPtr );
    static LRESULT WINAPI nstMsgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
    static DWORD WINAPI nstThread( LPVOID lpParameter );

    void runOnPluginThread();
    int64_t safeDispatch( int32_t opCode, int32_t index, int64_t value, void* ptr, float opt );

    // called from nstMsgProc
    void handleHostWindowClosing()
    {
        blog::plug( " NST | editor closing" );
        m_nstEditorState = Instance::Data::EditorState::Closing;
    }

    // flow for opening and closing the plugin editor gracefully
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

    // flow for turning plugin on/off via thread
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


    static std::mutex       m_initMutex;                // used to sync some console output across plugin start to avoid clashes

    const app::AudioPlaybackTimeInfo*  
                            m_unifiedTimeInfo;
    NstTimeInfo             m_nstTimeInfo;
    double                  m_nstTimeSampleRateRcp;

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

    HMODULE                 m_nstModule;
    VSTPluginMainFn         m_nstEntrypoint;
    CPlugEffect*            m_nstFilterInstance;
    HANDLE                  m_nstThreadHandle;
    std::atomic_bool        m_nstThreadAlive;
    std::atomic_bool        m_nstThreadInitComplete;
    std::atomic_bool        m_nstFailedToLoad;

    EditorStateAtomic       m_nstEditorState;           // request to open/close
    ActivationStateAtomic   m_nstActivationState;       // request to enable/disable (post boot)
    ActivationQueue         m_nstActivationQueue;
    HWND                    m_nstEditorHWND;
};

std::mutex Instance::Data::m_initMutex;

// ---------------------------------------------------------------------------------------------------------------------
void Instance::RegisterWndClass()
{
    WNDCLASSEX wc =
    {
        sizeof( WNDCLASSEX ),
        0,
        Data::nstMsgProc,
        0L,
        0L,
        GetModuleHandle( nullptr ),
        nullptr,
        LoadCursor( nullptr, IDC_ARROW ),
        nullptr,
        nullptr,
        _nstWindowClass,
        nullptr
    };
    ::RegisterClassEx( &wc );
}

// ---------------------------------------------------------------------------------------------------------------------
void Instance::UnregisterWndClass()
{
    ::UnregisterClass( _nstWindowClass, GetModuleHandle( nullptr ) );
}

// ---------------------------------------------------------------------------------------------------------------------
Instance::Instance( const char* pluginPath, const float sampleRate, const uint32_t maximumBlockSize, const app::AudioPlaybackTimeInfo* unifiedTime )
    : m_userdata( 0 )
{
    m_uid = absl::Hash< const char* >{}(pluginPath);

    m_data = std::make_unique< Instance::Data >();
    m_data->m_path                      = pluginPath;
    m_data->m_sampleRate                = sampleRate;
    m_data->m_maximumBlockSize          = maximumBlockSize;

    ABSL_ASSERT( unifiedTime != nullptr );
    m_data->m_unifiedTimeInfo           = unifiedTime;
    m_data->m_nstTimeInfo.sampleRate    = sampleRate;
    m_data->m_nstTimeSampleRateRcp      = 1.0 / sampleRate;
}

// ---------------------------------------------------------------------------------------------------------------------
Instance::~Instance()
{
    m_data->m_nstThreadAlive = false;
    ::WaitForSingleObject( m_data->m_nstThreadHandle, INFINITE );
}

// ---------------------------------------------------------------------------------------------------------------------
int64_t __cdecl Instance::Data::NstOpcodeCallback( CPlugEffect* effect, int32_t opcode, int32_t index, int64_t value, void* ptr, float opt )
{
    static constexpr double cSixtyRcp = 1.0 / 60.0;

    if ( opcode == NstOpcode_Version )
        return 2400;

    int64_t result = 0;
    if ( effect == nullptr )
    {
        if ( opcode == NstOpcode_GetVendorString       ||
             opcode == NstOpcode_GetProductString      ||
             opcode == NstOpcode_GetVendorVersion      ||
             opcode == NstOpcode_GetCurrentProcessLevel )
        {
            // these are stateless so it's okay if effect is null; this has been seen happen on some iZo plugs
        }
        else
        {
            // .. but otherwise this is an error, we need the userdata in effect to reach back to our host instance
            blog::error::plug( " - NstOpcodeCallback effect is nullptr for opcode {} ({})", opcode, nstOpcodeToString( opcode ) );
            return 0;
        }
    }

    switch ( opcode )
    {
        case NstOpcode_GetTime:
        {
            // PpqPosValid | TempoValid | BarsValid | CyclePosValid | TimeSigValid | SmpteValid | ClockValid |
            Instance::Data* localData = (Instance::Data*)effect->data_user;
            if ( localData )
            {
                ABSL_ASSERT( localData->m_unifiedTimeInfo != nullptr );
                const auto& unifiedTime = *localData->m_unifiedTimeInfo;
                      auto& nstTime     =  localData->m_nstTimeInfo;

                nstTime.samplePos           = unifiedTime.samplePos;
                nstTime.tempo               = unifiedTime.tempo;
                nstTime.sampleRate          = localData->m_sampleRate;
                nstTime.ppqPos              = (nstTime.samplePos * localData->m_nstTimeSampleRateRcp) * (nstTime.tempo * cSixtyRcp);
                nstTime.timeSigNumerator    = unifiedTime.timeSigNumerator;
                nstTime.timeSigDenominator  = unifiedTime.timeSigDenominator;
                nstTime.flags               = NstTIF_Tempo_Valid | NstTIF_PpqPos_Valid | NstTIF_TimeSig_Valid | NstTIF_Transport_Playing;

                return (int64_t)&localData->m_nstTimeInfo;
            }
            else
            {
                return (int64_t)nullptr;
            }
        }
        break;

        case NstOpcode_Automate:
        case NstOpcode_BeginEdit:
        case NstOpcode_EndEdit:
        {
            // if there is an automation callback registered, ping it and then release it
            Instance::Data* localData = (Instance::Data*)effect->data_user;
            if ( localData )
            {
                if ( Instance::DebugVerboseParameterLogging )
                {
                    blog::plug( "parameter #{} [{}] = {}",
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

        case NstOpcode_GetCurrentProcessLevel:
            return NstPL_Realtime;
            break;

        // still get this obsolete'd msg regularly; respond to identify what inputs we support
        case 4 /*NstOpcode_PinConnected*/:
        {
            const bool askingAboutInput = (value != 0);
            if ( askingAboutInput )
                return (index < 4) ? 0 : 1; // 0 = yes!
            else
                return (index < 2) ? 0 : 1;
        }

        case NstOpcode_CurrentId:
            return effect->plug_uid;

        case NstOpcode_Idle:
            effect->dispatcher( effect, NstHtpOpcode_EditIdle, 0, 0, nullptr, 0 );
            break;

        case NstOpcode_SizeWindow:
            {
                Instance::Data* localData = (Instance::Data*)effect->data_user;
                if ( localData )
                {
                    RECT wr = { 0, 0, (LONG)index, (LONG)value };
                    ::AdjustWindowRectEx( &wr, wStyle, FALSE, wStyleEx );
                    ::SetWindowPos( localData->m_nstEditorHWND, nullptr, 0, 0, wr.right, wr.bottom, SWP_NOMOVE | SWP_NOREPOSITION | SWP_NOZORDER );
                }
            }
            break;

        case NstOpcode_CanDo:
            if ( ptr != nullptr )
                blog::plug( " - NstOpcode_CanDo ({})", (const char*)ptr );
            break;

        case NstOpcode_IOChanged:
            {
                Instance::Data* localData = (Instance::Data*)effect->data_user;
                if ( localData )
                {
                    localData->m_updateIOChannels = true;
                }
            }
            return 1;

        case NstOpcode_GetVendorString:
            strcpy_s( (char*)ptr, 64, "ishani.org" );
            break;
        case NstOpcode_GetProductString:
            strcpy_s( (char*)ptr, 64, "OUROVEON" );
            break;
        case NstOpcode_GetVendorVersion:
            return 0x666;

        default:
            blog::plug( " - NstOpcodeCallback unhandled opcode {} ({})", opcode, nstOpcodeToString(opcode) );
            break;
    }
    return result;
}

// ---------------------------------------------------------------------------------------------------------------------
LRESULT WINAPI Instance::Data::nstMsgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    Instance::Data* instData = (Instance::Data*)::GetWindowLongPtr( hWnd, GWLP_USERDATA );

    switch ( msg )
    {
        case WM_CREATE:
        {
            instData = (Instance::Data*)((LPCREATESTRUCT)lParam)->lpCreateParams;
            ABSL_ASSERT( instData != nullptr );
            ::SetWindowLongPtr( hWnd, GWLP_USERDATA, (LONG_PTR) instData );

            instData->m_nstEditorState = Instance::Data::EditorState::Open;
            blog::plug( "opening editor for {}", instData->m_product );
        }
        break;

        case WM_DESTROY:
        {
            ABSL_ASSERT( instData != nullptr );
            instData->handleHostWindowClosing();
        }
        break;
    }
    return DefWindowProc( hWnd, msg, wParam, lParam );
}

// ---------------------------------------------------------------------------------------------------------------------
HWND  Instance::Data::nstCreateWindow( uint32_t width, uint32_t height, Instance::Data* dataPtr )
{
    RECT wr = { 0, 0, (LONG)width, (LONG)height };
    ::AdjustWindowRectEx( &wr, wStyle, FALSE, wStyleEx );

    return ::CreateWindowEx(
        wStyleEx,
        _nstWindowClass,
        L"Plugin",
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
DWORD WINAPI Instance::Data::nstThread( LPVOID lpParameter )
{
    // pass control over to the instance data
    auto* instData = (Instance::Data*)lpParameter;
    instData->runOnPluginThread();

    return 0;
}

// ---------------------------------------------------------------------------------------------------------------------
void Instance::Data::runOnPluginThread()
{
    // support 2+2 in (stereo + sidechain) and stereo out
    const int32_t cTotalKnownInputs  = 4;
    const int32_t cTotalKnownOutputs = 2;

    blog::plug( "trying to load [{}]", m_path );

    m_nstModule = LoadLibraryExA( m_path.c_str(), nullptr, 0 );
    if ( m_nstModule != nullptr )
    {
        m_nstEntrypoint = (VSTPluginMainFn)GetProcAddress( m_nstModule, "VSTPluginMain" );
        if ( m_nstEntrypoint == nullptr )
        {
            blog::error::plug( "VSTPluginMain entrypoint not found" );
            FreeLibrary( m_nstModule );

            m_nstFailedToLoad = true;
            return;
        }
    }
    else
    {
        blog::error::plug( "VST plugin file not found [{}]", m_path );
        m_nstFailedToLoad = true;
        return;
    }

    auto* vstFI = m_nstEntrypoint( NstOpcodeCallback );
    vstFI->data_user = this;

    m_nstFilterInstance = vstFI;

    int64_t dispRet;
    dispRet = safeDispatch( NstHtpOpcode_Open, 0, 0, nullptr, 0 );

    char vendorString[64]{ 0 };
    dispRet = safeDispatch( NstHtpOpcode_GetVendorString, 0, 0, vendorString, 0 );
    m_vendor = vendorString;

    char productString[64]{ 0 };
    dispRet = safeDispatch( NstHtpOpcode_GetProductString, 0, 0, productString, 0 );
    m_product = productString;

    if ( m_product.empty() )
    {
        m_product = fs::path( m_path ).filename().string();
        m_product = m_product.substr( 0, m_product.size() - 4 );
    }

    OuroveonThreadScope ots( fmt::format( OURO_THREAD_PREFIX "VST::{}", m_product ).c_str() );


    const auto vstVersion = safeDispatch( NstHtpOpcode_GetSysVersion, 0, 0, nullptr, 0 );
    if ( vstVersion >= 2 )
    {
        NstSpeakerArrangement saInput, saOutput;
        memset( &saInput, 0, sizeof( saInput ) );
        memset( &saOutput, 0, sizeof( saOutput ) );

        saInput.numChannels  = cTotalKnownInputs;
        saInput.type         = 1; // stereo
        saOutput.numChannels = cTotalKnownOutputs;
        saOutput.type        = 1; // stereo

        for ( int i = 0; i < 8; i++ )
        {
            saInput.speakers[i].azimuth   = saOutput.speakers[i].azimuth   = 0.0f;
            saInput.speakers[i].elevation = saOutput.speakers[i].elevation = 0.0f;
            saInput.speakers[i].radius    = saOutput.speakers[i].radius    = 0.0f;
            saInput.speakers[i].reserved  = saOutput.speakers[i].reserved  = 0.0f;

            constexpr int32_t SpeakerID_Left = 1;
            constexpr int32_t SpeakerID_Right = 2;

            switch ( i )
            {
                case 0:
                    saInput.speakers[i].type  = SpeakerID_Left;
                    saOutput.speakers[i].type = SpeakerID_Left;
                    strcpy_s( saInput.speakers[i].name, 64, "In-Left" );
                    strcpy_s( saOutput.speakers[i].name, 64, "Out-Left" );
                    break;
                case 1:
                    saInput.speakers[i].type  = SpeakerID_Right;
                    saOutput.speakers[i].type = SpeakerID_Right;
                    strcpy_s( saInput.speakers[i].name, 64, "In-Right" );
                    strcpy_s( saOutput.speakers[i].name, 64, "Out-Right" );
                    break;
                default:
                    saInput.speakers[i].type  = 0x7fffffff;
                    saOutput.speakers[i].type = 0x7fffffff;
                    break;
            }
        }
        safeDispatch( NstHtpOpcode_SetSpeakerArrangement, 0, ToNstPtr( &saInput ), &saOutput, 0.0f );

        NstPinProperties ioPinProperties;
        for ( int32_t index = 0; index < cTotalKnownInputs; index ++ )
        {
            safeDispatch( NstHtpOpcode_GetInputProperties, index, 0, &ioPinProperties, 0 );
#if NST_IO_PROPERTY_DEBUG_VERBOSE
            blog::plug( " - Input  {} = [ {:24} ] : [ {:32} ]", index, ioPinProperties.label, nstPinPropertiesFlagsToString( (NstPinPropertiesFlags)ioPinProperties.flags ) );
#endif // NST_IO_PROPERTY_DEBUG_VERBOSE
        }

        for ( int32_t index = 0; index < cTotalKnownOutputs; index ++ )
        {
            safeDispatch( NstHtpOpcode_GetOutputProperties, index, 0, &ioPinProperties, 0 );
#if NST_IO_PROPERTY_DEBUG_VERBOSE
            blog::plug( " - Output {} = [ {:24} ] : [ {:32} ]", index, ioPinProperties.label, nstPinPropertiesFlagsToString( (NstPinPropertiesFlags)ioPinProperties.flags ) );
#endif // NST_IO_PROPERTY_DEBUG_VERBOSE
        }
    }

    dispRet = safeDispatch( NstHtpOpcode_SetSampleRate, 0, 0, nullptr, m_sampleRate );
    dispRet = safeDispatch( NstHtpOpcode_SetBlockSize,  0, m_maximumBlockSize, nullptr, 0 );
    dispRet = safeDispatch( NstHtpOpcode_SetProcessPrecision, 0, 0 /* single-precision mode */, nullptr, 0.0f);
    dispRet = safeDispatch( NstHtpOpcode_SetSampleRate, 0, 0, nullptr, m_sampleRate );  // not a typo; some plugins like it before block-size, some after


    // update on first time through
    m_updateIOChannels = true;

    m_nstThreadInitComplete = true;

    // extract parameters
    {
        std::scoped_lock syncConsoleOutput( m_initMutex );
        blog::plug( "Loaded [ {} - {} ]", vendorString, productString );

        char paramLabel[255];
        for ( int32_t pI = 0; pI < vstFI->numParams; pI++ )
        {
            safeDispatch( NstHtpOpcode_GetParamName, pI, 0, paramLabel, 0 );

            m_paramIndexMapS2I.emplace( paramLabel, pI );
            m_paramIndexMapI2S.emplace( pI, paramLabel );
#if 0
            blog::plug( "  Param({}) = {}", pI, paramLabel );
#endif
        }
    }

    while ( m_nstThreadAlive )
    {
        if ( m_nstActivationState == ActivationState::Deactivating )
        {
            dispRet = safeDispatch( NstHtpOpcode_MainsChanged, 0, 0, nullptr, 0 );
            m_nstActivationState = ActivationState::Inactive;
        }
        else if ( m_nstActivationState == ActivationState::Activating )
        {
            dispRet = safeDispatch( NstHtpOpcode_MainsChanged, 0, 1, nullptr, 0 );
            m_nstActivationState = ActivationState::Active;
        }

        // handle requests to open or close the UI
        if ( m_nstEditorState == EditorState::WaitingToOpen )
        {
            m_nstEditorState = EditorState::Opening;

            NstRect* editorRect;
            safeDispatch( NstHtpOpcode_EditGetRect, 0, 0, &editorRect, 0 );

            m_nstEditorHWND = Instance::Data::nstCreateWindow(
                editorRect->right - editorRect->left,
                editorRect->bottom - editorRect->top,
                this );

            safeDispatch( NstHtpOpcode_EditOpen, 0, 0, m_nstEditorHWND, 0 );
        }
        else if ( m_nstEditorState == EditorState::WaitingToClose )
        {
            // this will trigger change to ::Closing
            ::DestroyWindow( m_nstEditorHWND );
        }
        else if ( m_nstEditorState == EditorState::Closing )
        {
            m_nstEditorState = EditorState::Idle;
            m_nstEditorHWND = nullptr;

            safeDispatch( NstHtpOpcode_EditClose, 0, 0, nullptr, 0 );
        }

        // run the message loop for a while
        {
            if ( m_nstEditorHWND != nullptr )
            {
                int32_t maxDispatch = 10; // arbitrary message read limit to avoid getting stuck in an endless 
                                          // message loop without checking in on all the other bits of the plugin thread logic

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
            m_midiInputChannels     = (int32_t)safeDispatch( NstHtpOpcode_GetNumMidiInputChannels, 0, 0, nullptr, 0 );
            m_midiOutputChannels    = (int32_t)safeDispatch( NstHtpOpcode_GetNumMidiOutputChannels, 0, 0, nullptr, 0 );
            m_updateIOChannels      = false;

            blog::plug( "MIDI Channels [{}] : In {} : Out {} ", productString, m_midiInputChannels, m_midiOutputChannels );
        }


        // process requests from main thread
        ActivationState requestedActivation;
        if ( m_nstActivationQueue.try_dequeue( requestedActivation ) )
        {
            m_nstActivationState = requestedActivation;
        }
    }

    m_nstThreadInitComplete = false;

    dispRet = safeDispatch( NstHtpOpcode_Close, 0, 0, nullptr, 0 );
    m_nstFilterInstance = nullptr;

    FreeLibrary( m_nstModule );
}

// ---------------------------------------------------------------------------------------------------------------------
int64_t Instance::Data::safeDispatch( int32_t opCode, int32_t index, int64_t value, void* ptr, float opt )
{
    int64_t result = 0;

    try
    {
        if ( m_nstFilterInstance->dispatcher != nullptr )
        {
            result = m_nstFilterInstance->dispatcher( m_nstFilterInstance, opCode, index, value, ptr, opt );
        }
    }
    catch ( ... )
    {
        blog::error::plug( "exception in SafeDispatch( {}, {}, {}, {}, {} )", opCode, index, value, ptr, opt  );
    }

    return result;
}

// ---------------------------------------------------------------------------------------------------------------------
void Instance::beginLoadAsync()
{
    // everything happens on the thread
    DWORD newThreadID;
    m_data->m_nstThreadAlive  = true;
    m_data->m_nstThreadHandle = ::CreateThread( nullptr, 0, Instance::Data::nstThread, m_data.get(), 0, &newThreadID );
                                ::SetThreadPriority( m_data->m_nstThreadHandle, THREAD_PRIORITY_ABOVE_NORMAL );

    blog::plug( "scheduled plugin load on thread [{}]", newThreadID );
}

// ---------------------------------------------------------------------------------------------------------------------
bool Instance::loaded() const
{
    return ( m_data->m_nstFilterInstance != nullptr &&
             m_data->m_nstThreadAlive &&
             m_data->m_nstThreadInitComplete );
}

// ---------------------------------------------------------------------------------------------------------------------
bool Instance::availableForUse() const
{
    return ( loaded() && 
             isActive() );
}

bool Instance::failedToLoad() const
{
    return m_data->m_nstFailedToLoad;
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

    return m_data->m_nstFilterInstance->plug_uid;
}

// ---------------------------------------------------------------------------------------------------------------------
bool Instance::isActive() const
{
    return ( m_data->m_nstActivationState == Instance::Data::ActivationState::Active );
}

// ---------------------------------------------------------------------------------------------------------------------
void Instance::requestActivationChange( bool onOff )
{
    m_data->m_nstActivationQueue.enqueue( onOff ?
        Instance::Data::ActivationState::Activating :
        Instance::Data::ActivationState::Deactivating );
}

// ---------------------------------------------------------------------------------------------------------------------
bool Instance::canChangeEditorState() const
{
    return ( m_data->m_nstEditorState == Instance::Data::EditorState::Idle ||
             m_data->m_nstEditorState == Instance::Data::EditorState::Open );
}

// ---------------------------------------------------------------------------------------------------------------------
bool Instance::openEditor()
{
    if ( m_data->m_nstEditorState != Instance::Data::EditorState::Idle )
        return false;

    m_data->m_nstEditorState = Instance::Data::EditorState::WaitingToOpen;
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
bool Instance::closeEditor()
{
    if ( m_data->m_nstEditorState != Instance::Data::EditorState::Open )
        return false;

    m_data->m_nstEditorState = Instance::Data::EditorState::WaitingToClose;
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
bool Instance::editorIsOpen() const
{
    return m_data->m_nstEditorState == Instance::Data::EditorState::Open;
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
        m_data->m_nstFilterInstance->setParameter( m_data->m_nstFilterInstance, index, value );
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
    int64_t chunkSize = m_data->m_nstFilterInstance->dispatcher( m_data->m_nstFilterInstance, NstHtpOpcode_GetChunk, 0, 0, &chunkData, 0 );

    // plugin supported getChunk, hooray
    if ( chunkSize > 0 )
    {
        return cppcodec::base64_rfc4648::encode( (const char*)chunkData, chunkSize );
    }
    
    // the manual fallback - fetch all the parameters we know about and save them to a JSON stream
    {
        std::string manualParamExport;
        manualParamExport.reserve( m_data->m_nstFilterInstance->numParams * 64 );

        // to differentiate us from the normal GetChunk data, prepend a clear magic ID we can look for on deserialize
        manualParamExport = OUROVEON_MAGIC_PARAMBLOCK_STR "{\n";

        for ( const auto& s2i : m_data->m_paramIndexMapS2I )
        {
            const float currentValue = m_data->m_nstFilterInstance->getParameter( m_data->m_nstFilterInstance, s2i.second );

            // save the values as hex-digit blobs to avoid any round-trip loss of precision
            uint32_t floatToHex;
            memcpy( &floatToHex, &currentValue, sizeof( float ) );

            manualParamExport += fmt::format( "  \"{}\" : \"{:#x}\",\n", s2i.first, floatToHex );
        }
        manualParamExport += fmt::format( "  \"_vst_version\" : {}\n", m_data->m_nstFilterInstance->plug_version );
        manualParamExport += "}\0\0\n";

        blog::plug( ".. manual parameter serialize ({} bytes)", manualParamExport.size() );

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
            blog::error::plug( "Instance::deserialize data empty" );
            return false;
        }

        // check for the magic key that indicates the data is one of our manual parameter encodings
        const size_t sizeOfOuroMagic = strlen( OUROVEON_MAGIC_PARAMBLOCK_STR );
        if ( memcmp( chunkData.data(), OUROVEON_MAGIC_PARAMBLOCK_STR, sizeOfOuroMagic ) == 0 )
        {
            blog::plug( ".. manual parameter deserialize" );

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

                    m_data->m_nstFilterInstance->setParameter( m_data->m_nstFilterInstance, paramLookup->second, newValue );
                }
            }
        }
        else
        {
            int64_t chunkSize = m_data->m_nstFilterInstance->dispatcher( m_data->m_nstFilterInstance, NstHtpOpcode_SetChunk, 0, chunkData.size(), chunkData.data(), 0 );
        }
    }
    catch ( nlohmann::json::parse_error& e )
    {
        blog::error::plug( "Instance::deserialize json parse failure, {}", e.what() );
        return false;
    }
    catch ( cppcodec::parse_error* e)
    {
        blog::error::plug( "Instance::deserialize failed, {}", e->what() );
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
void Instance::process( float** inputs, float** outputs, const int32_t sampleFrames )
{
    m_data->m_nstFilterInstance->processReplacing( m_data->m_nstFilterInstance, inputs, outputs, sampleFrames );
}

} // namespace nst

#endif // OURO_FEATURE_NST24
