//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  VST 2.x plugin loading support
//

#pragma once

#if OURO_FEATURE_VST24

namespace app { struct AudioPlaybackTimeInfo; namespace midi { struct Message; } }
namespace vst {

// ---------------------------------------------------------------------------------------------------------------------
class Instance
{
public:

    // set to true to log out parameter value changes during message processing (to check limits, indices)
    static bool DebugVerboseParameterLogging;


    static void RegisterWndClass();
    static void UnregisterWndClass();

    Instance( const char* pluginPath, const float sampleRate, const uint32_t maximumBlockSize, const app::AudioPlaybackTimeInfo* unifiedTime );
    ~Instance();

    // a hash of the plugin path
    inline int32_t getVSTUID() const { return m_uid; }

    // arbitrary user data
    inline void setUserData( uint64_t ud64 ) { m_userdata = ud64; }
    inline uint64_t getUserData() const { return m_userdata; }


    void beginLoadAsync();
    bool loaded() const;                // has instance, thread running

    bool availableForUse() const;       // loaded(), isActive()
    bool failedToLoad() const;

    // identity
    const std::string getPath() const;
    const std::string& getVendorName() const;
    const std::string& getProductName() const;
    const int32_t getUniqueID() const;

    // on/off
    bool isActive() const;
    void requestActivationChange( bool onOff ); // added to a queue, may not be immediate

    // UI access
    bool canChangeEditorState() const;
    bool openEditor();
    bool closeEditor();
    bool editorIsOpen() const;

    // poke individual parameters
    int32_t lookupParameterIndex( const std::string& byName );
    const char* lookupParameterName( const int32_t index );
    bool setParameter( const int32_t index, const float value );

    using AutomationCallbackFn = std::function<void( const int32_t parameterIndex, const char* parameterName, const float curValue )>;
    void requestAutomationHook( const AutomationCallbackFn& hookFn );
    bool hasAutomationHook() const;

    // chunk IO with base64 encoding
    std::string serialize();
    bool deserialize( const std::string& data );

    // run the VST against some data
    void process(float** inputs, float** outputs, const int32_t sampleFrames);


private:

    int32_t                 m_uid;
    uint64_t                m_userdata;

    struct Data;
    std::unique_ptr<Data>   m_data;
};

// ---------------------------------------------------------------------------------------------------------------------
// instantiate one of these in the app loop to get host window classes registered/unregistered
struct ScopedInitialiseVSTHosting
{
    ScopedInitialiseVSTHosting() { Instance::RegisterWndClass(); }
    ~ScopedInitialiseVSTHosting() { Instance::UnregisterWndClass(); }
};

} // namespace vst

#endif // OURO_FEATURE_VST24
