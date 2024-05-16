
// some kind of obsolete(?) audio plugin format. the details are lost to time.

#pragma once


#pragma pack(push)
#pragma pack(8)
#define NST_CALLBACK_CONVENTION __cdecl

struct CPlugEffect;

typedef	int64_t (NST_CALLBACK_CONVENTION* NstOpcodeCallback ) (CPlugEffect* effect, int32_t opcode, int32_t index, int64_t value, void* ptr, float opt);
typedef int64_t (NST_CALLBACK_CONVENTION* CPlugEffectDispatcherProc ) (CPlugEffect* effect, int32_t opcode, int32_t index, int64_t value, void* ptr, float opt);
typedef void    (NST_CALLBACK_CONVENTION* CPlugEffectProcessProc) (CPlugEffect* effect, float** inputs, float** outputs, int32_t sampleFrames);
typedef void    (NST_CALLBACK_CONVENTION* CPlugEffectProcessDoubleProc) (CPlugEffect* effect, double** inputs, double** outputs, int32_t sampleFrames);
typedef void    (NST_CALLBACK_CONVENTION* CPlugEffectSetParameterProc) (CPlugEffect* effect, int32_t index, float parameter);
typedef float   (NST_CALLBACK_CONVENTION* CPlugEffectGetParameterProc) (CPlugEffect* effect, int32_t index);

template <class T> inline int64_t ToNstPtr( T* ptr )
{
    int64_t* address = (int64_t*)&ptr;
    return *address;
}

struct NstRect
{
    int16_t top;
    int16_t left;
    int16_t bottom;
    int16_t right;
};

struct CPlugEffect
{
    int32_t magic;

    CPlugEffectDispatcherProc dispatcher;
    CPlugEffectProcessProc _padding0;
    CPlugEffectSetParameterProc setParameter;
    CPlugEffectGetParameterProc getParameter;

    int32_t numPrograms;
    int32_t numParams;
    int32_t numInputs;
    int32_t numOutputs;

    int32_t flags;

    int64_t resvd1;
    int64_t resvd2;

    int32_t initialDelay;

    int32_t _padding1;
    int32_t _padding2;
    float   _padding3;

    void* data_effect;
    void* data_user;

    int32_t plug_uid;
    int32_t plug_version;

    CPlugEffectProcessProc processReplacing;
    CPlugEffectProcessDoubleProc processDoubleReplacing;

    char future[56];
};

struct NstPinProperties
{
    char label[64];
    int32_t flags;
    int32_t arrangementType;
    char shortLabel[8];

    char future[48];
};

struct NstTimeInfo
{
	double samplePos;
	double sampleRate;
	double nanoSeconds;
	double ppqPos;
	double tempo;
	double barStartPos;
	double cycleStartPos;
	double cycleEndPos;
	int32_t timeSigNumerator;
	int32_t timeSigDenominator;
	int32_t smpteOffset;
	int32_t smpteFrameRate;
	int32_t samplesToNextClock;
	int32_t flags;
};

struct NstSpeakerProperties
{
    float azimuth;
    float elevation;
    float radius;
    float reserved;
    char name[64];
    int32_t type;

    char future[28];
};

struct NstSpeakerArrangement
{
    int32_t type;
    int32_t numChannels;
    NstSpeakerProperties speakers[8];
};

enum NstTimeInfoFlags
{
	NstTIF_Transport_Changed        = 1,
	NstTIF_Transport_Playing        = 1 << 1,
	NstTIF_Transport_Cycle_Active   = 1 << 2,
	NstTIF_Transport_Recording      = 1 << 3,
	NstTIF_Automation_Writing       = 1 << 6,
	NstTIF_Automation_Reading       = 1 << 7,
	NstTIF_Nanos_Valid              = 1 << 8,
	NstTIF_PpqPos_Valid             = 1 << 9,
	NstTIF_Tempo_Valid              = 1 << 10,
	NstTIF_Bars_Valid               = 1 << 11,
	NstTIF_CyclePos_Valid           = 1 << 12,
	NstTIF_TimeSig_Valid            = 1 << 13,
	NstTIF_Smpte_Valid              = 1 << 14,
	NstTIF_Clock_Valid              = 1 << 15
};

enum NstPinPropertiesFlags
{
    NstPPF_Pin_Is_Active            = 1 << 0,
    NstPPF_Pin_Is_Stereo            = 1 << 1,
    NstPPF_Pin_Use_Speaker          = 1 << 2
};

enum NstProcessLevels
{
    NstPL_Unknown,
    NstPL_User,
    NstPL_Realtime,
    NstPL_Prefetch,
    NstPL_Offline
};

enum NstOpcodes
{
    NstOpcode_Automate = 0,
    NstOpcode_Version,
    NstOpcode_CurrentId,
    NstOpcode_Idle,
    __NstOpcode_Padding_0, 

    __NstOpcode_Padding_1,
    __NstOpcode_Padding_2,

    NstOpcode_GetTime,
    NstOpcode_ProcessEvents,

    __NstOpcode_Padding_3,
    __NstOpcode_Padding_4,
    __NstOpcode_Padding_5,
    __NstOpcode_Padding_6,

    NstOpcode_IOChanged,

    __NstOpcode_Padding_7,

    NstOpcode_SizeWindow,
    NstOpcode_GetSampleRate,
    NstOpcode_GetBlockSize,
    NstOpcode_GetInputLatency,
    NstOpcode_GetOutputLatency,

    __NstOpcode_Padding_8,
    __NstOpcode_Padding_9,
    __NstOpcode_Padding_10,

    NstOpcode_GetCurrentProcessLevel,
    NstOpcode_GetAutomationState,

    NstOpcode_OfflineStart,
    NstOpcode_OfflineRead,
    NstOpcode_OfflineWrite,
    NstOpcode_OfflineGetCurrentPass,
    NstOpcode_OfflineGetCurrentMetaPass,

    __NstOpcode_Padding_11,
    __NstOpcode_Padding_12,

    NstOpcode_GetVendorString,
    NstOpcode_GetProductString,
    NstOpcode_GetVendorVersion,
    NstOpcode_VendorSpecific,
    
    __NstOpcode_Padding_13,

    NstOpcode_CanDo,
    NstOpcode_GetLanguage,

    __NstOpcode_Padding_14,
    __NstOpcode_Padding_15,

    NstOpcode_GetDirectory,
    NstOpcode_UpdateDisplay,
    NstOpcode_BeginEdit,
    NstOpcode_EndEdit,
    NstOpcode_OpenFileSelector,
    NstOpcode_CloseFileSelector,
};

enum NstHostToPluginOpcode
{
    NstHtpOpcode_Open = 0,
    NstHtpOpcode_Close,

    NstHtpOpcode_SetProgram,
    NstHtpOpcode_GetProgram,
    NstHtpOpcode_SetProgramName,
    NstHtpOpcode_GetProgramName,

    NstHtpOpcode_GetParamLabel,
    NstHtpOpcode_GetParamDisplay,
    NstHtpOpcode_GetParamName,

    __NstHtpOpcode_Padding_0,

    NstHtpOpcode_SetSampleRate,
    NstHtpOpcode_SetBlockSize,
    NstHtpOpcode_MainsChanged,

    NstHtpOpcode_EditGetRect,
    NstHtpOpcode_EditOpen,
    NstHtpOpcode_EditClose,

    __NstHtpOpcode_Padding_1,
    __NstHtpOpcode_Padding_2,
    __NstHtpOpcode_Padding_3,

    NstHtpOpcode_EditIdle,

    __NstHtpOpcode_Padding_4,
    __NstHtpOpcode_Padding_5,
    __NstHtpOpcode_Padding_6,

    NstHtpOpcode_GetChunk,
    NstHtpOpcode_SetChunk,

    NstHtpOpcode_ProcessEvents,

    NstHtpOpcode_CanBeAutomated,
    NstHtpOpcode_String2Parameter,

    __NstHtpOpcode_Padding_7,

    NstHtpOpcode_GetProgramNameIndexed,

    __NstHtpOpcode_Padding_8,
    __NstHtpOpcode_Padding_9,
    __NstHtpOpcode_Padding_10,

    NstHtpOpcode_GetInputProperties,
    NstHtpOpcode_GetOutputProperties,
    NstHtpOpcode_GetPlugCategory,

    __NstHtpOpcode_Padding_11,
    __NstHtpOpcode_Padding_12,

    NstHtpOpcode_OfflineNotify,
    NstHtpOpcode_OfflinePrepare,
    NstHtpOpcode_OfflineRun,

    NstHtpOpcode_ProcessVarIo,
    NstHtpOpcode_SetSpeakerArrangement,

    __NstHtpOpcode_Padding_13,

    NstHtpOpcode_SetBypass,
    NstHtpOpcode_GetEffectName,

    __NstHtpOpcode_Padding_14,

    NstHtpOpcode_GetVendorString,
    NstHtpOpcode_GetProductString,
    NstHtpOpcode_GetVendorVersion,
    NstHtpOpcode_VendorSpecific,
    NstHtpOpcode_CanDo,
    NstHtpOpcode_GetTailSize,

    __NstHtpOpcode_Padding_15,
    __NstHtpOpcode_Padding_16,
    __NstHtpOpcode_Padding_17,

    NstHtpOpcode_GetParameterProperties,

    __NstHtpOpcode_Padding_18,

    NstHtpOpcode_GetSysVersion,

    NstHtpOpcode_EditKeyDown,
    NstHtpOpcode_EditKeyUp,
    NstHtpOpcode_SetEditKnobMode,

    NstHtpOpcode_GetMidiProgramName,
    NstHtpOpcode_GetCurrentMidiProgram,
    NstHtpOpcode_GetMidiProgramCategory,
    NstHtpOpcode_HasMidiProgramsChanged,
    NstHtpOpcode_GetMidiKeyName,

    NstHtpOpcode_BeginSetProgram,
    NstHtpOpcode_EndSetProgram,

    NstHtpOpcode_GetSpeakerArrangement,
    NstHtpOpcode_ShellGetNextPlugin,

    NstHtpOpcode_StartProcess,
    NstHtpOpcode_StopProcess,
    NstHtpOpcode_SetTotalSampleToProcess,
    NstHtpOpcode_SetPanLaw,

    NstHtpOpcode_BeginLoadBank,
    NstHtpOpcode_BeginLoadProgram,

    NstHtpOpcode_SetProcessPrecision,
    NstHtpOpcode_GetNumMidiInputChannels,
    NstHtpOpcode_GetNumMidiOutputChannels,
};

#pragma pack(pop)
