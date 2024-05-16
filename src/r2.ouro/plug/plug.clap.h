//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#pragma once

#include "plug/known.plugin.h"
#include "sys/dynlib.h"

#include "clap/clap.h"

namespace app { struct CoreGUI; }

namespace plug {
namespace online { struct CLAP; struct Processing; }
namespace runtime {

// ---------------------------------------------------------------------------------------------------------------------
// the runtime object is responsible for loading the dynamic library & working with the CLAP factory interface to create
// a new inactive clap_plugin instance. it is then passed over to the online::CLAP object which takes ownership and
// performs one-off activation against current audio system parameters
//
struct CLAP
{
    using Instance = std::unique_ptr<CLAP>;

    ~CLAP();

    static absl::StatusOr< Instance > load( const plug::KnownPlugin& knownPlugin, clap_host* clapHost );

    enum class UIState
    {
        Unsupported,
        Available,
        Shown,
        Hidden,
    };

protected:
    friend online::CLAP;
    friend online::Processing;

    CLAP( const plug::KnownPlugin& knownPlugin );

private:

    using AudioBufferConfigs = std::vector< clap_audio_buffer_t >;

    absl::Status load( clap_host* clapHost ) noexcept;

    template <typename T>
    void getExtension( const T*& ptr, const char* id ) const noexcept 
    {
        ABSL_ASSERT( !ptr );
        ABSL_ASSERT( id );
        ABSL_ASSERT( m_pluginInstance );

        if ( m_pluginInstance->get_extension )
            ptr = static_cast<const T*>( m_pluginInstance->get_extension( m_pluginInstance, id ) );
    }


    plug::KnownPlugin               m_knownPlugin;

    sys::DynLib::Instance           m_pluginLibrary;
    const clap_plugin_entry*        m_pluginEntry           = nullptr;
    const clap_plugin*              m_pluginInstance        = nullptr;

    const clap_plugin_audio_ports*  m_pluginAudioPorts      = nullptr;
    const clap_plugin_gui*          m_pluginGui             = nullptr;
    const clap_plugin_latency*      m_pluginLatency         = nullptr;
    const clap_plugin_state*        m_pluginState           = nullptr;

    uint32_t                        m_pluginInputPortCount  = 0;
    uint32_t                        m_pluginOutputPortCount = 0;
    AudioBufferConfigs              m_pluginInputBuffers;
    AudioBufferConfigs              m_pluginOutputBuffers;
    bool                            m_pluginValidEffect     = false;        // is this a plugin that supports stereo input and output

    GLFWwindow*                     m_guiWindow             = nullptr;
    std::string                     m_guiWindowTitle;
    UIState                         m_guiState              = UIState::Unsupported;

public:

    constexpr bool isValidEffectPlugin() const { return m_pluginValidEffect; }

    constexpr AudioBufferConfigs& getInputAudioBuffers() { return m_pluginInputBuffers; }
    constexpr AudioBufferConfigs& getOutputAudioBuffers() { return m_pluginOutputBuffers; }


    void updateLatency();
    constexpr int64_t getLatency() const { return m_latencyInSamples; }

    void showUI( app::CoreGUI& coreGUI );
    
    constexpr UIState getUIState() const { return m_guiState; }
    constexpr bool canShowUI() const { return getUIState() != UIState::Unsupported; }


    bool uiRequestResize( uint32_t width, uint32_t height );
    bool uiRequestShow();
    bool uiRequestHide();
    void uiRequestClosed( bool bWasDestroyed );

private:
    static void uiWindowCloseCallback(GLFWwindow* window);

private:
    int64_t                         m_latencyInSamples  = -1;       // -1 means we don't have a valid latency reading
};

} // namespace runtime

// ---------------------------------------------------------------------------------------------------------------------

namespace online {

struct CLAP
{
    using Instance = std::unique_ptr<CLAP>;

    ~CLAP();

    static absl::StatusOr< Instance > activate(
        runtime::CLAP::Instance& runtimeInstance,      // take ownership of this runtime for duration of activation
        const uint32_t sampleRate,
        const uint32_t minFrameCount,
        const uint32_t maxFrameCount
        );

    static runtime::CLAP::Instance deactivate(
        online::CLAP::Instance& onlineInstance );


    runtime::CLAP& getRuntimeInstance() { return *(m_runtimeInstance.get()); }

protected:
    friend online::Processing;

    CLAP( runtime::CLAP::Instance&& runtimeInstance );


private:

    runtime::CLAP::Instance     m_runtimeInstance;
};

// ---------------------------------------------------------------------------------------------------------------------

// RAII wrapping around start_processing / stop_processing
struct Processing
{
    Processing() = delete;
    Processing( const online::CLAP::Instance& onlineInstance );
    ~Processing();

    bool isValid() const { return m_pluginInstance != nullptr; }

    int32_t operator()( const clap_process& process ); // returning clap_process_status

private:
    friend struct online::CLAP;

    const clap_plugin* m_pluginInstance = nullptr;
};

} // namespace online
} // namespace plug
