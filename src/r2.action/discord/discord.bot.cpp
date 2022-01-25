//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  manager around connecting to Discord, marshalling voice comms and
//  doing packet dispatch from the OPUS sample processor
//

#include "pch.h"

#include "discord/discord.bot.h"
#include "discord/config.h"

#include "base/spacetime.h"

#include "app/core.h"
#include "app/module.audio.h"

#include "ssp/opus.h"

#include "dpp/dpp.h"


namespace discord {

// ---------------------------------------------------------------------------------------------------------------------
VoiceChannel::VoiceChannel( const dpp::channel* channelData )
    :   m_order( channelData->position )
    ,      m_id( channelData->id )
    ,    m_name( channelData->name )
    , m_bitrate( channelData->bitrate )
{}
using VoiceChannelNameMap = std::unordered_map< dpp::snowflake, std::string >;

// ---------------------------------------------------------------------------------------------------------------------
struct Bot::State
{
    // lf queue exchanging completed blocks of packets from sample processor (audio thread) and our dispatch (main thread)
    using OpusPacketQueue = mcc::ReaderWriterQueue<ssp::OpusPacketStreamInstance>;


    State( app::ICoreServices& coreServices, const config::discord::Connection& configConnection )
        :     m_appCoreServices( coreServices )
        ,               m_phase( Bot::ConnectionPhase::Uninitialised )
        ,             m_cluster( configConnection.botToken )
        ,      m_commandHandler( &m_cluster )
        ,            m_guildSID( std::stoull( configConnection.guildSID ) )
        ,           m_liveVoice( nullptr )
        ,          m_voiceState( Bot::VoiceState::NoConnection )
        ,  m_voiceChannelLiveID( 0 )
        , m_opusDispatchRunning( false )
    {
        m_commandHandler.add_prefix( "." )
                        .add_prefix( "/" );
    }

    ~State()
    {
        m_appCoreServices.getAudioModule()->clearAuxSampleProcessor();
        m_phase = Bot::ConnectionPhase::Uninitialised;
    }

    ConnectionPhase getConnectionPhase() const 
    { 
        return m_phase; 
    }

    bool initialise()
    {
        if ( m_phase != Bot::ConnectionPhase::Uninitialised )
        {
            blog::error::discord( "already initialised / cannot start" );
            return false;
        }

        // called when shard is ready
        m_cluster.on_ready( [this]( const dpp::ready_t& event )
        {
            blog::discord( "cluster manager ready" );
            onBotReady();
        });

        m_cluster.on_critical_boot_failure( [this]( const dpp::log_t& event )
        {
            blog::error::discord( "boot failure; {}", event.message );
            m_phase = Bot::ConnectionPhase::UnableToStart;
        });

        m_cluster.on_log( []( const dpp::log_t& event )
        {
            if ( event.severity > dpp::ll_trace )
            {
                blog::discord( "{} : {}", dpp::utility::loglevel( event.severity ), event.message );
            }
        });

        // triggered when we arrive in a voice channel; need to capture and stash the voice_client instance
        m_cluster.on_voice_ready( [this]( const dpp::voice_ready_t& event )
        {
            blog::discord( "voice channel connected (ch:{})", event.voice_channel_id );

            m_liveVoice          = event.voice_client;
            m_voiceChannelLiveID = event.voice_channel_id;
            m_voiceState         = Bot::VoiceState::Joined;
        });

        m_cluster.on_voice_state_update( [this]( const dpp::voice_state_update_t& event )
        {
            blog::discord( "on_voice_state_update (user:{}, ch:{})", event.state.user_id, event.state.channel_id );

            if ( event.state.user_id == m_cluster.me.id )
            {
                if ( event.state.channel_id == 0 )
                {
                    std::lock_guard<std::mutex> voiceLock( m_liveVoiceGuard );

                    m_liveVoice          = nullptr;
                    m_voiceChannelLiveID = 0;
                    m_voiceState         = Bot::VoiceState::NotJoined;
                }
            }
        });


        // boot the threads and such, off we go
        m_cluster.start( true );
        m_phase = Bot::ConnectionPhase::Booting;

        return true;
    }

    void onBotReady()
    {
        m_commandHandler.add_command(
            "nowplaying",
            {
            },
            [this]( const std::string& command, const dpp::parameter_list_t& parameters, dpp::command_source src )
            {
                const auto summary = generateNowPlayingSummary();
                m_commandHandler.reply( dpp::message( summary ), src );
            },
            "What's blasting out in the voice channel?",
            m_guildSID );

        m_commandHandler.register_commands();

        // ask for guild data to be cached
        m_phase = Bot::ConnectionPhase::RequestingGuildData;
        m_cluster.guild_get( m_guildSID, [this]( const dpp::confirmation_callback_t& cb )
        {
            blog::discord( "decoding guild data" );
            onGuildData();
        });

        // create and install an OPUS sample processor that points back to this bot instance; we will accept and
        // handle the blocks of packets it returns.
        // nb. 48000 hz sample rate is required by Discord and should be enforced by the UI / app boot process if you want to use the bot interface
        auto opusProcessor = ssp::PagedOpus::Create( std::bind( &State::onOpusPacketBlock, this, std::placeholders::_1 ), 48000 );
        m_appCoreServices.getAudioModule()->installAuxSampleProcessor( std::move( opusProcessor ) );
    }

    void onOpusPacketBlock( ssp::OpusPacketStreamInstance&& packets )
    {
        if ( m_voiceState != Bot::VoiceState::Joined )
            return;

        blog::discord( "[ OPUS ] packet stream enqueued" );

        m_opusQueue.enqueue( std::move( packets ) );
    }

    // called once guild_get() returns something useful
    void onGuildData()
    {
        // this should now return a filled instance from the guild cache
        dpp::guild* guildInstance = dpp::find_guild( m_guildSID );
        if ( guildInstance == nullptr )
        {
            blog::error::discord( "guild data fetch failed, cannot isolate voice channels" );
            return;
        }

        // snag the top drawer metadata for the server for UI display if desired
        {
            GuildMetadata metadata;
            metadata.m_name     = guildInstance->name;
            metadata.m_shardID  = guildInstance->shard_id;

            m_guildMetadata = metadata;
        }

        // we are interested in the available voice channels on this server, boil down a list
        m_voiceChannelNamesByID.clear();
        VoiceChannels* channelList = new VoiceChannels();
        for ( const auto& c : guildInstance->channels )
        {
            // only care about voice transmission channels
            const dpp::channel* ch = dpp::find_channel( c );
            if ( !ch || (!ch->is_voice_channel() && !ch->is_stage_channel()) )
                continue;

            channelList->emplace_back( ch );
            m_voiceChannelNamesByID.emplace( ch->id, ch->name );
        }
        blog::discord( "found {} potential voice channels", channelList->size() );

        // stash voice metadata ready for use
        m_voiceChannels.store( std::shared_ptr< const VoiceChannels >( channelList ) );
        m_voiceState = Bot::VoiceState::NotJoined;

        // ready to use the connection
        m_phase = Bot::ConnectionPhase::Ready;
    }

    bool isBotBusy() const
    {
        if ( m_phase == Bot::ConnectionPhase::RequestingGuildData ||
             m_phase == Bot::ConnectionPhase::Booting )
            return true;

        if ( m_phase == Bot::ConnectionPhase::Ready )
        {
            return m_voiceState == Bot::VoiceState::Flux;
        }
        return false;
    }

    VoiceState getVoiceState() const
    {
        return m_voiceState;
    }

    dpp::snowflake getVoiceChannelLiveID() const
    {
        return m_voiceChannelLiveID;
    }

    bool joinVoiceChannel( const VoiceChannel& vc )
    {
        blog::discord( "joinVoiceChannel({}:{})", vc.m_name, vc.m_id );

        if ( m_voiceState != Bot::VoiceState::NotJoined )
        {
            blog::error::discord( "voiceState ({}) not in NotJoined, can't trigger Join event", m_voiceState.load() );
            return false;
        }

        std::lock_guard<std::mutex> voiceLock( m_liveVoiceGuard );
        m_voiceState          = Bot::VoiceState::Flux;
        m_opusDispatchRunning = false;

        dpp::discord_client* clientForGuild = m_cluster.get_shard( m_guildMetadata->m_shardID );
        clientForGuild->connect_voice( m_guildSID, vc.m_id, false, true );
        return true;
    }

    bool leaveVoiceChannel()
    {
        blog::discord( "leaveVoiceChannel()" );

        if ( m_voiceState != Bot::VoiceState::Joined )
        {
            blog::error::discord( "voiceState ({}) not in Joined, can't trigger Leave event", m_voiceState.load() );
            return false;
        }

        std::lock_guard<std::mutex> voiceLock( m_liveVoiceGuard );
        m_voiceState          = Bot::VoiceState::Flux;
        m_opusDispatchRunning = false;

        dpp::discord_client* clientForGuild = m_cluster.get_shard( m_guildMetadata->m_shardID );
        clientForGuild->disconnect_voice( m_guildSID );
        return true;
    }

    std::string generateNowPlayingSummary() const
    {
        const auto& exchangeData = m_appCoreServices.getEndlesssExchange();
        if ( m_voiceState == Bot::VoiceState::Joined && exchangeData.m_live )
        {
            std::string pastRiff = "";

            if ( exchangeData.m_timestamp > 0 )
            {
                const auto riffTimeDelta = base::spacetime::calculateDeltaFromNow(
                    base::spacetime::InSeconds( std::chrono::seconds{ exchangeData.m_timestamp } ) ).asPastTenseString( 3 );

                pastRiff = fmt::format( " ( riff recorded {} )", riffTimeDelta );
            }

            // sort out a list of unique jammer names to report
            std::unordered_set< std::string > uniqueJammerSet;
            for ( auto i = 0; i < 8; i++ )
            {
                if ( exchangeData.isJammerNameValid( i ) )
                    uniqueJammerSet.emplace( exchangeData.getJammerName( i ) );
            }
            
            const auto numUniqueJammers = uniqueJammerSet.size();
            size_t addIndex = 0;

            std::string jammers;
            jammers.reserve( 128 );
            for ( const auto& uniqueJammer : uniqueJammerSet )
            {
                if ( addIndex > 0 )
                {
                    if ( addIndex >= numUniqueJammers - 1 )
                        jammers.append( " and " );
                    else
                        jammers.append( ", " );
                }
                jammers.append( "`" );
                jammers.append( uniqueJammer );
                jammers.append( "`" );
                addIndex++;
            }

            return fmt::format( "Live in <#{}>, we're tuned into [**{}**] with {}{}", 
                m_voiceChannelLiveID.load(),
                exchangeData.m_jamName,
                jammers,
                pastRiff );
        }
        else
        {
            return "We're not playing anything at the moment";
        }
    }

    void update( DispatchStats& stats );


    app::ICoreServices&                     m_appCoreServices;

    std::atomic< Bot::ConnectionPhase >     m_phase;

    dpp::cluster                            m_cluster;
    dpp::commandhandler                     m_commandHandler;

    const dpp::snowflake                    m_guildSID;
    GuildMetadataOptional                   m_guildMetadata;

    std::mutex                              m_liveVoiceGuard;       // defense against the packet processing loop on main thread getting
                                                                    // blindsided by voice channel disconnection 
    dpp::discord_voice_client*              m_liveVoice;

    std::atomic< Bot::VoiceState >          m_voiceState;
    std::atomic< dpp::snowflake >           m_voiceChannelLiveID;   // if VoiceState is Joined, this is the ID of the one we're on
    VoiceChannelsAtomic                     m_voiceChannels;
    VoiceChannelNameMap                     m_voiceChannelNamesByID;

    OpusPacketQueue                         m_opusQueue;
    ssp::OpusPacketStreamInstance           m_opusPacketInProgress;
    ssp::OpusPacketStreamInstance           m_opusPacketInReserve;
    bool                                    m_opusDispatchRunning;
};

// ---------------------------------------------------------------------------------------------------------------------
void Bot::State::update( DispatchStats& stats )
{
    static const size_t maxPacketsDispatchedPerUpdate = 3;

    stats.m_packetsSentBytes    = 0;
    stats.m_packetsSentCount    = 0;
    stats.m_bufferingProgress   = -1;

    // packet dispatch
    {
        std::lock_guard<std::mutex> voiceLock( m_liveVoiceGuard );
        if ( m_voiceState == Bot::VoiceState::Joined && m_liveVoice != nullptr )
        {
            if ( m_opusPacketInProgress == nullptr )
            {
                if ( m_opusPacketInReserve != nullptr )
                    std::swap( m_opusPacketInProgress, m_opusPacketInReserve );
            }
            if ( m_opusPacketInReserve == nullptr )
            {
                m_opusQueue.try_dequeue( m_opusPacketInReserve );
            }

            if ( !m_opusDispatchRunning )
            {
                const auto queueLength = m_opusQueue.size_approx();
                if ( queueLength >= 3 &&
                     m_opusPacketInProgress &&
                     m_opusPacketInReserve )
                    m_opusDispatchRunning = true;
                else
                {
                    stats.m_bufferingProgress = (float)queueLength;
                    if ( m_opusPacketInProgress )
                        stats.m_bufferingProgress++;
                    if ( m_opusPacketInReserve )
                        stats.m_bufferingProgress++;
                    stats.m_bufferingProgress /= 5.0f;
                }
            }
            if ( m_opusDispatchRunning && 
                 m_opusPacketInProgress )
            {
                const auto remainingPackets = std::min( m_opusPacketInProgress->m_opusPacketSizes.size() - m_opusPacketInProgress->m_dispatchedPackets, maxPacketsDispatchedPerUpdate );
                for ( auto pkt = 0; pkt < remainingPackets; pkt++ )
                {
                    const size_t packetLength = m_opusPacketInProgress->m_opusPacketSizes[m_opusPacketInProgress->m_dispatchedPackets];

                    m_liveVoice->send_audio_opus( &m_opusPacketInProgress->m_opusData[m_opusPacketInProgress->m_dispatchedSize], packetLength );

                    m_opusPacketInProgress->m_dispatchedPackets++;
                    m_opusPacketInProgress->m_dispatchedSize += packetLength;

                    stats.m_packetsSentCount++;
                    stats.m_packetsSentBytes += (uint32_t)packetLength;
                }

                if ( m_opusPacketInProgress->m_dispatchedPackets >= m_opusPacketInProgress->m_opusPacketSizes.size() )
                {
                    m_opusPacketInProgress.reset();
                }
            }
        }
        else if ( m_voiceState == Bot::VoiceState::Flux )
        {
            // flush all the dispatch state when in Flux
            ssp::OpusPacketStreamInstance flushQueue;
            while ( m_opusQueue.try_dequeue( flushQueue ) )
            {
                blog::discord( "draining packet queue..." );
                flushQueue.reset();
            }
            m_opusPacketInProgress.reset();
            m_opusPacketInReserve.reset();
            m_opusDispatchRunning = false;
        }
    }

    stats.m_packetBlobQueueLength   = (uint32_t)m_opusQueue.size_approx();
    stats.m_dispatchRunning         = m_opusDispatchRunning;
}


// ---------------------------------------------------------------------------------------------------------------------

Bot::Bot()
    : m_initialised( false )
{
}

Bot::~Bot()
{
}

bool Bot::initialise( app::ICoreServices& coreServices, const config::discord::Connection& configConnection )
{
    blog::discord( "connection initialising ..." );
    try
    {
        m_state = std::make_unique<State>( coreServices, configConnection );
        return m_state->initialise();
    }
    catch ( dpp::exception* )
    {
        // #HDD todo abseil return exception data
        m_state = nullptr;
        return false;
    }
}

void Bot::update( DispatchStats& stats )
{
    if ( m_state )
        return m_state->update( stats );
}

Bot::VoiceState Bot::getVoiceState() const
{
    if ( m_state )
        return m_state->getVoiceState();

    return Bot::VoiceState::NoConnection;
}

uint64_t Bot::getVoiceChannelLiveID() const
{
    if ( m_state )
        return m_state->getVoiceChannelLiveID();

    return 0;
}

bool Bot::joinVoiceChannel( const VoiceChannel& vc )
{
    if ( getConnectionPhase() == Bot::ConnectionPhase::Ready )
        return m_state->joinVoiceChannel( vc );

    return false;
}

bool Bot::leaveVoiceChannel()
{
    if ( getConnectionPhase() == Bot::ConnectionPhase::Ready )
        return m_state->leaveVoiceChannel();

    return false;
}

Bot::ConnectionPhase Bot::getConnectionPhase() const
{
    if ( m_state )
        return m_state->getConnectionPhase();

    return Bot::ConnectionPhase::Uninitialised;
}

std::string Bot::getBotName() const
{
    if ( getConnectionPhase() == Bot::ConnectionPhase::Ready )
        return m_state->m_cluster.me.username;

    return "[none]";
}

discord::GuildMetadataOptional Bot::getGuildMetadata() const
{
    if ( m_state )
        return m_state->m_guildMetadata;

    return std::nullopt;
}

bool Bot::isBotBusy() const
{
    if ( m_state )
        return m_state->isBotBusy();

    return false;
}

discord::VoiceChannelsPtr Bot::getVoiceChannels() const
{
    if ( m_state )
        return m_state->m_voiceChannels.load();

    return nullptr;
}

} // namespace discord