//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  manager around connecting to Discord, marshalling voice comms and
//  doing packet dispatch from the OPUS sample processor
//

#include "pch.h"

#include "base/text.h"

#include "discord/discord.bot.ui.h"
#include "discord/discord.bot.h"
#include "discord/config.h"

#include "app/module.frontend.h"
#include "app/module.frontend.fonts.h"
#include "app/module.audio.h"


namespace discord {

// ---------------------------------------------------------------------------------------------------------------------
BotWithUI::BotWithUI( app::ICoreServices& coreServices )
    : m_services( coreServices )
    , m_trafficOutBytes( 0 )
{
}

// ---------------------------------------------------------------------------------------------------------------------
BotWithUI::~BotWithUI()
{
}

// ---------------------------------------------------------------------------------------------------------------------
void BotWithUI::imgui( app::CoreGUI& coreGUI )
{
    // delay-load any stored discord credentials
    if ( !m_config.has_value() )
    {
        config::discord::Connection connectionConfig;

        // try and load existing discord config, failure is fine
        std::ignore = config::load( coreGUI, connectionConfig );
        m_config = connectionConfig;
    }

    const auto pulseColour = ImGui::GetPulseColourVec4();

    const bool bIsBotBusy = ( m_discordBot && m_discordBot->isBotBusy() );

    if ( ImGui::Begin( ICON_FAB_DISCORD " Discord Stream###discord_view") )
    {
        const float discordViewWidth = ImGui::GetContentRegionAvail().x;

        ImGui::TextUnformatted( ICON_FA_ROBOT " Bot Connection  " );
        {
            ImGui::SameLine();
            ImGui::Dummy( ImVec2( ImGui::GetContentRegionAvail().x - 32.0f, 0.0f ) );
            ImGui::SameLine();
            ImGui::Spinner( "##syncing", bIsBotBusy, ImGui::GetTextLineHeight() * 0.4f, 3.0f, 0.0f, ImGui::GetColorU32( ImGuiCol_Text ) );
        }
        ImGui::Spacing();
        ImGui::Spacing();

        if ( m_discordBot )
        {
            // main-thread update for the bot manager instance
            discord::Bot::DispatchStats stats;
            m_discordBot->update( stats );

            if ( stats.m_averagePacketSize > 0 )
                m_avgPacketSize.update( (double)stats.m_averagePacketSize );

            m_trafficOutBytes += stats.m_packetsSentBytes;


            const auto botPhase = m_discordBot->getConnectionPhase();
            switch ( botPhase )
            {
                case discord::Bot::ConnectionPhase::Uninitialised:
                    ImGui::TextUnformatted( ICON_FA_CIRCLE " Idle" );
                    break;
                case discord::Bot::ConnectionPhase::Booting:
                    ImGui::TextColored( pulseColour, ICON_FA_CIRCLE_DOT " Booting interface ..." );
                    break;
                case discord::Bot::ConnectionPhase::RequestingGuildData:
                    ImGui::TextColored( pulseColour, ICON_FA_CIRCLE_DOT " Fetching guild data ..." );
                    break;
                case discord::Bot::ConnectionPhase::Ready:
                    ImGui::TextColored( ImGui::GetStyleColorVec4( ImGuiCol_NavHighlight ), ICON_FA_CIRCLE_USER " Connected as %s", m_discordBot->getBotName().c_str() );
                    break;

                // in the case of critical failures, offer to junk the bot instance and start afresh
                case discord::Bot::ConnectionPhase::UnableToStart:
                {
                    ImGui::TextColored( ImGui::GetErrorTextColour(), ICON_FA_TRIANGLE_EXCLAMATION " Critical boot failure" );
                    ImGui::Spacing();
                    if ( ImGui::Button( " Reset " ) )
                    {
                        m_discordBot.reset();
                    }
                }
                break;
            }

            if ( botPhase == discord::Bot::ConnectionPhase::Ready )
            {
                if ( !m_trafficOutBytesStatusHandle.has_value() )
                {
                    m_trafficOutBytesStatusHandle = coreGUI.registerStatusBarBlock( app::CoreGUI::StatusBarAlignment::Right, 120.0f, [this]()
                    {
                        ImGui::Text( ICON_FA_UPLOAD " %s", base::humaniseByteSize( "", m_trafficOutBytes ).c_str() );
                    });
                }

                static const ImVec2 channelButtonSize( 68.0f, 20.0f );

                // fetch voice status for UI
                const auto voiceState    = m_discordBot->getVoiceState();
                const auto voiceLiveID   = m_discordBot->getVoiceChannelLiveID();
                const bool voiceCanJoin  = ( voiceState == discord::Bot::VoiceState::NotJoined );
                const bool voiceCanLeave = ( voiceState == discord::Bot::VoiceState::Joined );
                const bool voiceInFlux   = ( voiceState == discord::Bot::VoiceState::Flux );

                ImGui::Spacing();
                ImGui::Spacing();

                {
                    ImGui::PushFont( coreGUI.getFrontend()->getFont( app::module::Frontend::FontChoice::MediumTitle ) );
                    const auto& guild = m_discordBot->getGuildMetadata();
                    ImGui::TextUnformatted( guild->m_name.c_str() );
                    ImGui::PopFont();
                }
                ImGui::Spacing();

                if ( voiceState != discord::Bot::VoiceState::NoConnection &&
                        ImGui::BeginTable( "##channels", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg ) )
                {
                    ImGui::PushStyleColor( ImGuiCol_Text, ImGui::GetStyleColorVec4( ImGuiCol_ResizeGripHovered ) );
                    ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthFixed, 20.0f );
                    ImGui::TableSetupColumn( "Channel", ImGuiTableColumnFlags_None );
                    ImGui::TableSetupColumn( "Kbps", ImGuiTableColumnFlags_WidthFixed, 35.0f );
                    ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthFixed, 70.0f );
                    ImGui::TableHeadersRow();
                    ImGui::PopStyleColor();

                    const discord::VoiceChannels& voiceChannelList = *m_discordBot->getVoiceChannels().get();
                    for ( const auto& channel : voiceChannelList )
                    {
                        const bool isLive = ( channel.m_id == voiceLiveID );

                        ImGui::TableNextColumn();
                        {
                            ImGui::BeginDisabledControls( !isLive );
                            ImGui::TextUnformatted( isLive ? ICON_FA_VOLUME_HIGH : ICON_FA_VOLUME_OFF );
                            ImGui::EndDisabledControls( !isLive );
                        }
                        ImGui::TableNextColumn(); ImGui::Text( "%s", channel.m_name.c_str() );
                        ImGui::TableNextColumn(); ImGui::Text( "%i", channel.m_bitrate );
                        ImGui::TableNextColumn();
                        ImGui::PushID( channel.m_order );

                        if ( isLive )
                        {
                            ImGui::BeginDisabledControls( !voiceCanLeave );
                            if ( ImGui::Button( "Leave", channelButtonSize ) )
                            {
                                m_discordBot->leaveVoiceChannel();
                            }
                            ImGui::EndDisabledControls( !voiceCanLeave );
                        }
                        else if ( voiceCanJoin )
                        {
                            if ( ImGui::Button( "Join", channelButtonSize ) )
                            {
                                m_discordBot->joinVoiceChannel( channel );
                            }
                        }
                        else
                        {
                            ImGui::Dummy( channelButtonSize );
                        }

                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }

                ImGui::Spacing();
                if ( !stats.m_dispatchRunning )
                {
                    if ( stats.m_bufferingProgress >= 0 )
                    {
                        ImGui::Text( "Initial buffering ..." );
                        ImGui::ProgressBar( stats.m_bufferingProgress, ImVec2( -1.0f, 5.0f ), "" );
                    }
                    else
                    {
                        ImGui::Text( "Join channel to begin streaming" );
                    }
                }
                else
                {
                    // latency estimation based on how many packets are sat in the DPP send queue
                    const float minimumLatency = (float)stats.m_voiceBufferQueueState * ssp::OpusStream::cFrameTimeSec;

                    ImGui::Text( "Voice Buffer Queue : %3u ( + ~%.1fs latency )", stats.m_voiceBufferQueueState, minimumLatency );
                    ImGui::Text( "Packet Size  (avg) : %4i bytes", (int32_t)m_avgPacketSize.m_average );

                    ImGui::PushItemWidth( discordViewWidth * 0.65f );

                    discord::Bot::UdpTuning::Enum udpTuning;
                    if ( m_discordBot->getUdpTuning( udpTuning ) )
                    {
                        if ( discord::Bot::UdpTuning::ImGuiCombo( " Dispatch Tuning", udpTuning ) )
                        {
                            m_discordBot->setUdpTuning( udpTuning );
                        }
                        ImGui::CompactTooltip( "Control of voice buffer transmission timing; bumping up may help solve gaps in audio stream" );
                    }

                    ssp::OpusStream::CompressionSetup setup;
                    bool setupChanged = false;
                    if ( m_discordBot->getCurrentCompressionSetup( setup ) )
                    {
                        // technically "500 to 512000" is viable, discord or dpp seems to have problems about 150k
                        setupChanged |= ImGui::SliderInt( " Target Bitrate",      &setup.m_bitrate, 20000, 150000 );
                                        ImGui::CompactTooltip( "OPUS compressor target bitrate" );
                    }
                    if ( setupChanged )
                        m_discordBot->setCompressionSetup( setup );

                    ImGui::PopItemWidth();
                }
            } // botPhase == Ready
            else
            {
                // remove status bar chunk when not live
                if ( m_trafficOutBytesStatusHandle.has_value() )
                {
                    coreGUI.unregisterStatusBarBlock( m_trafficOutBytesStatusHandle.value() );
                    m_trafficOutBytesStatusHandle.reset();
                }
            }
        }
        // -------------------------------------------------------------------------------------------------------------
        // unconnected state, offer configuration & connection
        else
        {
            if ( m_services.getAudioModule()->getSampleRate() != 48000 )
            {
                ImGui::TextDisabled( "[ Incompatible Audio Sample Rate ]" );
            }
            else
            {
                ABSL_ASSERT( m_config.has_value() );    // we expect the config to have been loaded by this point (or set to default)
                auto& connectionConfig = m_config.value();

                {
                    ImGui::SeparatorBreak();

                    ImGui::InputText( " Bot Token", &connectionConfig.botToken, ImGuiInputTextFlags_Password );
                    ImGui::SameLine();
                    ImGui::TextDisabled( "[?]" );
                    ImGui::CompactTooltip( "from https://discord.com/developers/applications/<bot_id>/bot" );

                    // enable Developer Mode in your account settings, then you get Copy ID on right-click menus
                    ImGui::InputText( " Guild ID ", &connectionConfig.guildSID, ImGuiInputTextFlags_CharsDecimal );
                    ImGui::SameLine();
                    ImGui::TextDisabled( "[?]" );
                    ImGui::CompactTooltip( "Server ID of the guild to connect with\nhttps://tinyurl.com/server-id-discord" );

                    ImGui::SeparatorBreak();
                }
            
                ImGui::Spacing();
                ImGui::Spacing();

                if ( connectionConfig.botToken.empty() ||
                     connectionConfig.guildSID.empty() )
                {
                    ImGui::TextDisabled( ICON_FA_TRIANGLE_EXCLAMATION " Discord configuration data missing" );
                }
                else
                {
                    static std::string lastConnectResult;

                    if ( ImGui::Button( "Save & Connect", ImVec2( -1.0f, 34.0f ) ) )
                    {
                        // (re)save the entered credentials
                        const auto discordSaveResult = config::save( coreGUI, connectionConfig );
                        if ( discordSaveResult != config::SaveResult::Success )
                        {
                            blog::error::cfg( "Unable to save discord configuration" );
                        }

                        // create a new bot instance
                        m_discordBot = std::make_unique<discord::Bot>();

                        // boot the bot with the credentials
                        const auto discordStatus = m_discordBot->initialise( m_services, connectionConfig );
                        if ( !discordStatus.ok() )
                        {
                            blog::error::discord( fmt::format( FMTX( "bot failed to initialise; {}" ), discordStatus.ToString() ) );

                            lastConnectResult = ( ICON_FA_TRIANGLE_EXCLAMATION " initialisation failed, check log" );
                            m_discordBot.reset();
                        }
                    }

                    // report a basic connection failure, set above
                    if ( !lastConnectResult.empty() )
                    {
                        ImGui::Spacing();
                        ImGui::TextUnformatted( lastConnectResult );
                    }
                }
            }
        }
    }
    ImGui::End();
}

} // namespace discord
