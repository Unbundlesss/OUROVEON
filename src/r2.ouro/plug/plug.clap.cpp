//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#include "pch.h"

#include "plug/plug.clap.h"
#include "base/construction.h"
#include "app/core.h"
#include "app/module.frontend.h"

#include "clap/clap.h"
#include "clap/version.h"


namespace plug {
namespace runtime {

// ---------------------------------------------------------------------------------------------------------------------
// return the appropriate UI choice based on our build configuration defines
//
constexpr const char* getClapUIForPlatform()
{
#if OURO_PLATFORM_WIN
    return CLAP_WINDOW_API_WIN32;
#elif OURO_PLATFORM_OSX
    return CLAP_WINDOW_API_COCOA;
#elif OURO_PLATFORM_LINUX
    return CLAP_WINDOW_API_X11;
#else
#error no platform defined
    return nullptr;
#endif
}

// ---------------------------------------------------------------------------------------------------------------------
CLAP::CLAP( const plug::KnownPlugin& knownPlugin )
    : m_knownPlugin( knownPlugin )
{
}

// ---------------------------------------------------------------------------------------------------------------------
CLAP::~CLAP()
{
    if ( m_pluginInstance != nullptr )
    {
        m_pluginInstance->destroy( m_pluginInstance );
        m_pluginInstance = nullptr;
    }

    if ( m_pluginEntry != nullptr )
    {
        m_pluginEntry->deinit();
        m_pluginEntry = nullptr;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
absl::StatusOr< CLAP::Instance > CLAP::load( const plug::KnownPlugin& knownPlugin, clap_host* clapHost )
{
    Instance result = base::protected_make_unique<CLAP>( knownPlugin );

    const auto loadStatus = result->load( clapHost );
    if ( loadStatus.ok() )
        return result;

    return loadStatus;
}

// ---------------------------------------------------------------------------------------------------------------------
absl::Status CLAP::load( clap_host* clapHost ) noexcept
{
    auto clapLib = sys::DynLib::loadFromFile( m_knownPlugin.m_fullLibraryPath );
    if ( clapLib.ok() )
    {
        m_pluginEntry = clapLib.value()->resolve< const clap_plugin_entry >( "clap_entry" );
        if ( m_pluginEntry == nullptr )
        {
            return absl::NotFoundError( "clap_entry not found" );
        }

        if ( m_pluginEntry->init( m_knownPlugin.m_fullLibraryPath.string().c_str() ) )
        {
            const clap_plugin_factory* clapFactory = static_cast<const clap_plugin_factory*>(m_pluginEntry->get_factory( CLAP_PLUGIN_FACTORY_ID ));
            if ( clapFactory == nullptr )
            {
                m_pluginEntry->deinit();
                m_pluginEntry = nullptr;
                return absl::InternalError( "clap_plugin_factory not found" );
            }

            m_pluginInstance = clapFactory->create_plugin( clapFactory, clapHost, m_knownPlugin.m_uid.c_str() );
            if ( m_pluginInstance == nullptr )
            {
                m_pluginEntry->deinit();
                m_pluginEntry = nullptr;
                return absl::InternalError( "create_plugin failed" );
            }

            if ( !m_pluginInstance->init( m_pluginInstance ) )
            {
                m_pluginInstance->destroy( m_pluginInstance );
                m_pluginEntry->deinit();
                m_pluginEntry = nullptr;
                return absl::InternalError( "clap_plugin->init() failed" );
            }

            // collect the plugin extensions points for future use
            getExtension( m_pluginAudioPorts,   CLAP_EXT_AUDIO_PORTS );
            getExtension( m_pluginGui,          CLAP_EXT_GUI );
            getExtension( m_pluginLatency,      CLAP_EXT_LATENCY );
            getExtension( m_pluginState,        CLAP_EXT_STATE );

            // check that we can scan the audio ports
            if ( m_pluginAudioPorts == nullptr ||
                 m_pluginAudioPorts->count == nullptr ||
                 m_pluginAudioPorts->get == nullptr )
            {
                m_pluginInstance->destroy( m_pluginInstance );
                m_pluginEntry->deinit();
                m_pluginEntry = nullptr;
                return absl::InternalError( "clap_plugin did not provide audio ports interface" );
            }

            // ask for what IO ports the plugin can provide us
            m_pluginInputPortCount  = m_pluginAudioPorts->count( m_pluginInstance, true );
            m_pluginOutputPortCount = m_pluginAudioPorts->count( m_pluginInstance, false );

            const auto isSupportedPort = []( const clap_audio_port_info_t& portInfo )
                {
                    if ( portInfo.port_type == nullptr )
                        return false;
                    if ( strcmp( portInfo.port_type, CLAP_PORT_STEREO ) == 0 && portInfo.channel_count == 2 )
                        return true;
                    if ( strcmp( portInfo.port_type, CLAP_PORT_MONO ) == 0 && portInfo.channel_count == 1 )
                        return true;
                    return false;
                };

            // if we find something that isn't a stereo or mono port, we probably can't deal with it
            bool bInvalidPortFound = false;

            // scan for a stereo input port
            bool bFoundValidStereoInput = false;
            for ( uint32_t inputIndex = 0; inputIndex < m_pluginInputPortCount; inputIndex++ )
            {
                clap_audio_port_info_t portInfo;
                m_pluginAudioPorts->get( m_pluginInstance, inputIndex, true, &portInfo );

                bool bIsSupportedPort = isSupportedPort( portInfo );
                if ( bIsSupportedPort )
                {
                    // our assumption is the main channel - index 0 - should be a stereo input for 
                    // us to use this plugin as an effect processor
                    if ( inputIndex == 0 && portInfo.channel_count == 2 )
                        bFoundValidStereoInput = true;
                }
                else
                {
                    bInvalidPortFound = true;
                }

                // stash our processing buffer setup
                {
                    clap_audio_buffer_t inputBuffer;
                    memset( &inputBuffer, 0, sizeof( inputBuffer ) );
                    inputBuffer.channel_count = portInfo.channel_count;
                    m_pluginInputBuffers.emplace_back( inputBuffer );
                }

                blog::debug::plug( FMTX( "[CLAP:{}] Input {} = {:15} | channels = {} | {}" ),
                    m_knownPlugin.m_name,
                    inputIndex,
                    portInfo.name,
                    portInfo.channel_count,
                    ( portInfo.port_type != nullptr ) ? portInfo.port_type : "unknown" );
            }

            // scan for a stereo output port
            bool bFoundValidStereoOutput = false;
            for ( uint32_t outputIndex = 0; outputIndex < m_pluginOutputPortCount; outputIndex++ )
            {
                clap_audio_port_info_t portInfo;
                m_pluginAudioPorts->get( m_pluginInstance, outputIndex, false, &portInfo );

                bool bIsSupportedPort = isSupportedPort( portInfo );
                if ( bIsSupportedPort )
                {
                    // we also need a stereo output as the main index-0 channel
                    if ( outputIndex == 0 && portInfo.channel_count == 2 )
                        bFoundValidStereoOutput = true;
                }
                else
                {
                    bInvalidPortFound = true;
                }

                // stash our processing buffer setup
                {
                    clap_audio_buffer_t outputBuffer;
                    memset( &outputBuffer, 0, sizeof( outputBuffer ) );
                    outputBuffer.channel_count = portInfo.channel_count;
                    m_pluginOutputBuffers.emplace_back( outputBuffer );
                }

                blog::debug::plug( FMTX( "[CLAP:{}] Output {} = {:15} | channels = {} | {}" ),
                    m_knownPlugin.m_name,
                    outputIndex,
                    portInfo.name,
                    portInfo.channel_count,
                    (portInfo.port_type != nullptr) ? portInfo.port_type : "unknown" );
            }

            // record if this plugin meets our expectations for a processing plugin that 
            // consumes and produces a stereo feed
            m_pluginValidEffect = ( bFoundValidStereoInput && bFoundValidStereoOutput ) && ( bInvalidPortFound == false );
            blog::debug::plug( FMTX( "[CLAP:{}] pluginValidEffect = {} (in:{} | out:{} | invalid:{})" ),
                m_knownPlugin.m_name,
                m_pluginValidEffect,
                bFoundValidStereoInput,
                bFoundValidStereoOutput,
                bInvalidPortFound );


            // ask the plugin if a GUI will be possible and valid on the current platform
            if ( m_pluginGui &&
                 m_pluginGui->is_api_supported )
            {
                bool uiSupported = m_pluginGui->is_api_supported(
                    m_pluginInstance,
                    getClapUIForPlatform(),
                    false );

                blog::debug::plug( FMTX( "[CLAP:{}] uiSupported({}) = {}" ), m_knownPlugin.m_name, getClapUIForPlatform(), uiSupported );

                // if it's reported as supported, double-check that we have all the function hooks we want too
                if ( uiSupported )
                {
                    uiSupported &= ( m_pluginGui->create        != nullptr );
                    uiSupported &= ( m_pluginGui->destroy       != nullptr );
                    uiSupported &= ( m_pluginGui->adjust_size   != nullptr );
                    uiSupported &= ( m_pluginGui->set_size      != nullptr );
                    uiSupported &= ( m_pluginGui->set_parent    != nullptr );
                    uiSupported &= ( m_pluginGui->show          != nullptr );
                    uiSupported &= ( m_pluginGui->hide          != nullptr );

                    // report that we *could* have had a UI but the plugin is missing some hooks
                    if ( !uiSupported )
                    {
                        blog::debug::plug( FMTX( "[CLAP:{}] missing required clap_plugin_gui functions, disabling ui support" ), m_knownPlugin.m_name );
                    }
                }

                m_guiState = ( uiSupported ) ? UIState::Available : UIState::Unsupported;
            }

            m_pluginLibrary = std::move( clapLib.value() );
            return absl::OkStatus();
        }
        else
        {
            m_pluginEntry = nullptr;
            return absl::InternalError( "clap_entry->init() failed" );
        }
    }
    return clapLib.status();
}

// ---------------------------------------------------------------------------------------------------------------------
void CLAP::updateLatency()
{
    if ( m_pluginLatency != nullptr && m_pluginLatency->get )
        m_latencyInSamples = static_cast< int64_t >( m_pluginLatency->get( m_pluginInstance ) );
    else
        m_latencyInSamples = -1;

    blog::debug::plug( FMTX( "[CLAP:{}] updateLatency() = {}" ), m_knownPlugin.m_name, m_latencyInSamples );
}

// ---------------------------------------------------------------------------------------------------------------------
void CLAP::showUI( app::CoreGUI& coreGUI )
{
    ABSL_ASSERT( canShowUI() );

    switch ( m_guiState )
    {
        default:
        case UIState::Unsupported:
            break;

        case UIState::Available:
        {
            const bool bDidCreateGUI = m_pluginGui->create( m_pluginInstance, getClapUIForPlatform(), false );

            m_guiWindowTitle = fmt::format( FMTX( "CLAP | {} | {} {}" ),
                m_knownPlugin.m_vendor,
                m_knownPlugin.m_name,
                m_knownPlugin.m_version );

            if ( bDidCreateGUI == false )
            {
                coreGUI.getEventBusClient().Send<::events::AddToastNotification>( ::events::AddToastNotification::Type::Error,
                    m_guiWindowTitle,
                    "Unable to create plugin UI" );
            }
            else
            {
                uint32_t width = 0, height = 0;
                m_pluginGui->get_size( m_pluginInstance, &width, &height );

                m_guiWindow = glfwCreateWindow(
                    width,
                    height,
                    m_guiWindowTitle.c_str(),
                    nullptr,
                    coreGUI.getFrontend()->getGLFWWindow()
                );

                // set resizability based on plugin choice
                glfwSetWindowAttrib( m_guiWindow, GLFW_RESIZABLE, m_pluginGui->can_resize( m_pluginInstance ) ? 1 : 0 );

                // tell glfw that we'll handle closing this window ourselves, we need to tell the plugin when it happens
                glfwSetWindowUserPointer( m_guiWindow, this );
                glfwSetWindowCloseCallback( m_guiWindow, uiWindowCloseCallback );

                // encode native handle of our window into the generic clap_window object
                // to be handed to the plugin as the parent window to build / render their UI into
                clap_window clapWindow;
                clapWindow.api      = getClapUIForPlatform();
#if OURO_PLATFORM_WIN
                clapWindow.win32    = glfwGetWin32Window( m_guiWindow );
#elif OURO_PLATFORM_OSX
                clapWindow.cocoa    = glfwGetCocoaWindow( m_guiWindow );
#elif OURO_PLATFORM_LINUX
                clapWindow.x11      = glfwGetX11Window( m_guiWindow );
#endif
                m_pluginGui->set_parent( m_pluginInstance, &clapWindow );

                // all done, show by default
                m_pluginGui->show( m_pluginInstance );

                m_guiState = UIState::Shown;
            }
        }
        break;

        case UIState::Shown:
        {
            glfwFocusWindow( m_guiWindow );
        }
        break;

        case UIState::Hidden:
        {
            m_pluginGui->show( m_pluginInstance );
            glfwShowWindow( m_guiWindow );
        }
        break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
bool CLAP::uiRequestResize( uint32_t width, uint32_t height )
{
    glfwSetWindowSize( m_guiWindow, static_cast<int32_t>( width ), static_cast< int32_t >( height ) );
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
bool CLAP::uiRequestShow()
{
    if ( m_pluginGui->show( m_pluginInstance ) )
    {
        glfwFocusWindow( m_guiWindow );
        m_guiState = UIState::Shown;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------------------------------------------------
bool CLAP::uiRequestHide()
{
    if ( m_pluginGui->hide( m_pluginInstance ) )
    {
        glfwHideWindow( m_guiWindow );
        m_guiState = UIState::Hidden;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------------------------------------------------
void CLAP::uiRequestClosed( bool bWasDestroyed )
{
    // hide the window, not sure if that's really required
    uiRequestHide();

    // .. optionally destroy it too, resetting it entirely
    if ( bWasDestroyed )
    {
        m_pluginGui->destroy( m_pluginInstance );
        glfwDestroyWindow( m_guiWindow );
        m_guiWindow = nullptr;
        m_guiWindowTitle = "invalid";

        m_guiState = UIState::Available;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
void CLAP::uiWindowCloseCallback( GLFWwindow* window )
{
    CLAP* clapInst = static_cast< CLAP* >( glfwGetWindowUserPointer( window ) );
    ABSL_ASSERT( clapInst != nullptr );

    clapInst->uiRequestClosed( true );
}


} // namespace runtime
namespace online {

// ---------------------------------------------------------------------------------------------------------------------
CLAP::CLAP( runtime::CLAP::Instance&& runtimeInstance )
    : m_runtimeInstance( std::move( runtimeInstance) )
{
    blog::plug( FMTX( "[CLAP] activated {} {}" ),
        m_runtimeInstance->m_knownPlugin.m_name,
        m_runtimeInstance->m_knownPlugin.m_version );
}

// ---------------------------------------------------------------------------------------------------------------------
CLAP::~CLAP()
{
    ABSL_ASSERT( m_runtimeInstance == nullptr );    // to ensure symmetric activate/deactivate; we could just deactivate stuff here?
}

// ---------------------------------------------------------------------------------------------------------------------
absl::StatusOr< CLAP::Instance > CLAP::activate(
    runtime::CLAP::Instance& runtimeInstance,
    const uint32_t sampleRate,
    const uint32_t minFrameCount,
    const uint32_t maxFrameCount )
{
    bool activateOk = runtimeInstance->m_pluginInstance->activate(
        runtimeInstance->m_pluginInstance,
        static_cast<double>(sampleRate),
        minFrameCount,
        maxFrameCount );

    if ( activateOk )
    {
        // fetch initial states post-activation
        runtimeInstance->updateLatency();

        return base::protected_make_unique<CLAP>( std::move( runtimeInstance ) );
    }

    return absl::UnknownError( "failed to activate()" );
}

// ---------------------------------------------------------------------------------------------------------------------
plug::runtime::CLAP::Instance CLAP::deactivate(
    online::CLAP::Instance& onlineInstance )
{
    plug::runtime::CLAP::Instance runtimeInstance = std::move( onlineInstance->m_runtimeInstance );
    runtimeInstance->m_pluginInstance->deactivate(
        runtimeInstance->m_pluginInstance );

    blog::plug( FMTX( "[CLAP] deactivated {} {}" ),
        runtimeInstance->m_knownPlugin.m_name,
        runtimeInstance->m_knownPlugin.m_version );

    onlineInstance.reset();
    return std::move( runtimeInstance );
}

// ---------------------------------------------------------------------------------------------------------------------
Processing::Processing( const online::CLAP::Instance& onlineInstance )
    : m_pluginInstance( onlineInstance->getRuntimeInstance().m_pluginInstance )
{
    // attempt to start_processing; if that fails, null the plugin instance to mark the Processing block is invalid
    if ( m_pluginInstance->start_processing( m_pluginInstance ) == false )
    {
        m_pluginInstance = nullptr;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
Processing::~Processing()
{
    if ( m_pluginInstance != nullptr )
    {
        m_pluginInstance->stop_processing( m_pluginInstance );
    }
}

// ---------------------------------------------------------------------------------------------------------------------
int32_t Processing::operator()( const clap_process& process )
{
    ABSL_ASSERT( isValid() );
    if ( isValid() )
    {
        return m_pluginInstance->process( m_pluginInstance, &process );
    }
    return CLAP_PROCESS_ERROR;
}

} // namespace online
} // namespace plug
