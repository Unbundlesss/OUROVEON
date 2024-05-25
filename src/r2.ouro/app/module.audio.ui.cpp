//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#include "pch.h"
#include "app/core.h"
#include "app/imgui.ext.h"
#include "app/module.audio.h"
#include "app/module.frontend.h"
#include "app/module.frontend.fonts.h"

#include "plug/plug.clap.h"

namespace app {
namespace module {

// ---------------------------------------------------------------------------------------------------------------------
void Audio::imgui( app::CoreGUI& coreGUI )
{

#if OURO_HAS_CLAP

    if ( ImGui::Begin( ICON_FA_PLUG " Signal Path###audiomodule_signal" ) )
    {
        if ( m_clapEffectTest )
        {
            bool bIsActivated = ( m_clapEffectTest->m_online != nullptr );
            if ( ImGui::Checkbox( m_clapEffectTest->m_displayName.c_str(), &bIsActivated ) )
            {
                if ( bIsActivated )
                {
                    auto onlineActivateStatus = plug::online::CLAP::activate(
                        m_clapEffectTest->m_runtime,
                        m_outSampleRate,
                        8,
                        m_outMaxBufferSize );

                    if ( onlineActivateStatus.ok() )
                    {
                        m_clapEffectTest->m_online = std::move( onlineActivateStatus.value() );
                        m_clapEffectTest->m_ready = true;
                    }
                }
                else
                {
                    m_clapEffectTest->m_ready = false;
                    m_clapEffectTest->m_runtime = plug::online::CLAP::deactivate( m_clapEffectTest->m_online );
                }
            }
            ImGui::SameLine();
            {
                const auto clapUIState = m_clapEffectTest->getRuntimeInstance().getUIState();

                ImGui::Scoped::Enabled se( m_clapEffectTest->getRuntimeInstance().canShowUI() );
                if ( ImGui::Button( "GUI" ) )
                {
                    m_clapEffectTest->getRuntimeInstance().showUI( coreGUI );
                }
            }
        }

        if ( m_pluginStashClap->asyncAllTasksComplete() )
        {
            m_pluginStashClap->iterateKnownPluginsValidAndSorted( [this]( const plug::KnownPlugin& knownPlugin, plug::KnownPluginIndex index )
                {
                    if ( ImGui::Button( knownPlugin.m_sortable.c_str() ) )
                    {
                        CLAPEffect* clapEffect = new CLAPEffect();

                        clapEffect->m_audioModule = this;
                        clapEffect->m_host = m_clapHost;
                        clapEffect->m_host.host_data = clapEffect;

                        clapEffect->m_displayName = knownPlugin.m_name;

                        auto runtimeLoadStatus = plug::runtime::CLAP::load( knownPlugin, &clapEffect->m_host );
                        if ( runtimeLoadStatus.ok() )
                        {
                            clapEffect->m_runtime = std::move( runtimeLoadStatus.value() );
                        }

                        m_clapEffectTest = clapEffect;
                    }
                });
        }
    }
    ImGui::End();

#endif // OURO_HAS_CLAP

}

} // namespace module
} // namespace app
