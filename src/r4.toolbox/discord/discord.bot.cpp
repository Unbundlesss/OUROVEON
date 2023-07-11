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

#include "spacetime/chronicle.h"

#include "app/core.h"
#include "app/module.audio.h"

#include "ssp/ssp.stream.opus.h"

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
    using OpusPacketQueue = mcc::ReaderWriterQueue<ssp::OpusPacketDataInstance>;


    State( app::ICoreServices& coreServices, const config::discord::Connection& configConnection )
        :       m_appCoreServices( coreServices )
        ,                 m_phase( Bot::ConnectionPhase::Uninitialised )
        ,               m_cluster( configConnection.botToken )
        ,        m_commandHandler( &m_cluster )
        ,              m_guildSID( std::stoull( configConnection.guildSID ) )
        , m_opusStreamProcessorID( ssp::StreamProcessorInstanceID::invalid() )
        ,             m_liveVoice( nullptr )
        , m_voiceBufferQueueState( 0 )
#if OURO_PLATFORM_OSX
        ,        m_voiceUdpTuning( Bot::UdpTuning::Delicate )
#else
        ,        m_voiceUdpTuning( Bot::UdpTuning::Default )
#endif
        ,            m_voiceState( Bot::VoiceState::NoConnection )
        ,    m_voiceChannelLiveID( 0 )
        ,   m_opusDispatchRunning( false )
    {
        m_commandHandler.add_prefix( "." )
                        .add_prefix( "/" );

        m_workingMemory.opusData = nullptr;
        m_workingMemory.opusLength = 0;
    }

    ~State()
    {
        if ( m_opusStreamProcessorID != ssp::StreamProcessorInstanceID::invalid() )
        {
            m_appCoreServices.getAudioModule()->blockUntil(
                m_appCoreServices.getAudioModule()->detachSampleProcessor( m_opusStreamProcessorID ) );
        }
        
        m_phase = Bot::ConnectionPhase::Uninitialised;
    }

    ConnectionPhase getConnectionPhase() const 
    { 
        return m_phase; 
    }

    absl::Status initialise()
    {
        blog::discord( FMTX( "init {}" ), DPP_VERSION_TEXT );

        if ( m_phase != Bot::ConnectionPhase::Uninitialised )
        {
            return absl::AlreadyExistsError( "already initialised / cannot start" );
        }

        // called when shard is ready
        m_cluster.on_ready( [this]( const dpp::ready_t& event )
        {
            blog::discord( FMTX( "cluster manager ready" ) );
            onBotReady();
        });

        m_cluster.on_critical_boot_failure( [this]( const dpp::log_t& event )
        {
            blog::error::discord( FMTX( "boot failure; {}" ), event.message );
            m_phase = Bot::ConnectionPhase::UnableToStart;
        });

        m_cluster.on_log( []( const dpp::log_t& event )
        {
            if ( event.severity > dpp::ll_trace )
            {
                blog::discord( FMTX( "{} : {}" ), dpp::utility::loglevel( event.severity ), event.message );
            }
        });

        // triggered when we arrive in a voice channel; need to capture and stash the voice_client instance
        m_cluster.on_voice_ready( [this]( const dpp::voice_ready_t& event )
        {
            blog::discord( FMTX( "voice channel connected (ch:{})" ), event.voice_channel_id );

            m_liveVoice          = event.voice_client;
            m_voiceChannelLiveID = event.voice_channel_id;
            m_voiceState         = Bot::VoiceState::Joined;
        });

        m_cluster.on_voice_state_update( [this]( const dpp::voice_state_update_t& event )
        {
            blog::discord( FMTX( "on_voice_state_update (user:{}, ch:{})" ), event.state.user_id, event.state.channel_id );

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

        return absl::OkStatus();
    }

    void onBotReady()
    {
        // create and install an OPUS sample processor that points back to this bot instance; we will accept and
        // handle the blocks of packets it returns.
        // nb. 48000 hz sample rate is required by Discord and should be enforced by the UI / app boot process if you want to use the bot interface
        const auto statusOrPtr = ssp::OpusStream::Create( std::bind( &State::onOpusPacketBlock, this, std::placeholders::_1 ), 48000 );

        // i'm not sure what would lead to OPUS failing to boot but .. it's technically possible
        if ( !statusOrPtr.ok() )
        {
            blog::error::discord( FMTX( "fatal bot error - cannot create OPUS instance" ) );
            blog::error::discord( FMTX( " --> {}" ), statusOrPtr.status().ToString() );
            m_phase = Bot::ConnectionPhase::UnableToStart;
            return;
        }
        m_opusStreamProcessor = *statusOrPtr;

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
            blog::discord( FMTX( "decoding guild data" ) );
            onGuildData();
        });

        m_cluster.on_voice_buffer_send( [this]( const dpp::voice_buffer_send_t& bufferSend )
        {
            m_voiceBufferQueueState = bufferSend.buffer_size;
        });


        assert( m_opusStreamProcessorID == ssp::StreamProcessorInstanceID::invalid() );
        m_opusStreamProcessorID = m_opusStreamProcessor->getInstanceID();
        m_appCoreServices.getAudioModule()->attachSampleProcessor( m_opusStreamProcessor );
    }

    // called from compressor thread; take the data and return as fast as we can
    void onOpusPacketBlock( ssp::OpusPacketDataInstance&& packets )
    {
        if ( m_voiceState != Bot::VoiceState::Joined )
            return;

        m_opusQueue.enqueue( std::move( packets ) );
    }

    // called once guild_get() returns something useful
    void onGuildData()
    {
        // this should now return a filled instance from the guild cache
        dpp::guild* guildInstance = dpp::find_guild( m_guildSID );
        if ( guildInstance == nullptr )
        {
            blog::error::discord( FMTX( "guild data fetch failed, cannot isolate voice channels" ) );
            m_phase = Bot::ConnectionPhase::UnableToStart;
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
        blog::discord( FMTX( "found {} potential voice channels" ), channelList->size() );

        // stash voice metadata ready for use
        m_voiceChannels = std::shared_ptr< const VoiceChannels >( channelList );
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
        blog::discord( FMTX( "joinVoiceChannel({}:{})" ), vc.m_name, vc.m_id );

        if ( m_voiceState != Bot::VoiceState::NotJoined )
        {
            blog::error::discord( FMTX( "voiceState ({}) not in NotJoined, can't trigger Join event" ), (int32_t)m_voiceState.load() );
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
        blog::discord( FMTX( "leaveVoiceChannel()" ) );

        if ( m_voiceState != Bot::VoiceState::Joined )
        {
            blog::error::discord( FMTX( "voiceState ({}) not in Joined, can't trigger Leave event" ), (int32_t)m_voiceState.load() );
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
        if ( m_voiceState == Bot::VoiceState::Joined && exchangeData.hasRiffData() )
        {
            std::string pastRiff = "";

            if ( exchangeData.m_riffTimestamp > 0 )
            {
                const auto riffTimeDelta = spacetime::calculateDeltaFromNow(
                    spacetime::InSeconds( std::chrono::seconds{ exchangeData.m_riffTimestamp } ) ).asPastTenseString( 3 );

                pastRiff = fmt::format( FMTX( " ( riff recorded {} )" ), riffTimeDelta );
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

            return fmt::format( FMTX( "Live in <#{}>, we're tuned into [**{}**] with {}{}" ),
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

    ssp::OpusStream::SharedPtr              m_opusStreamProcessor;
    ssp::StreamProcessorInstanceID          m_opusStreamProcessorID;

    dpp::discord_voice_client::OpusDispatchWorkingMemory
                                            m_workingMemory;        // reusable memory block for encryption + send of opus packets

    const dpp::snowflake                    m_guildSID;
    GuildMetadataOptional                   m_guildMetadata;

    std::mutex                              m_liveVoiceGuard;       // defense against the packet processing loop on main thread getting
                                                                    // blindsided by voice channel disconnection 
    dpp::discord_voice_client*              m_liveVoice;
    std::atomic_uint32_t                    m_voiceBufferQueueState;
    Bot::UdpTuning::Enum                    m_voiceUdpTuning;

    std::atomic< Bot::VoiceState >          m_voiceState;
    std::atomic< dpp::snowflake >           m_voiceChannelLiveID;   // if VoiceState is Joined, this is the ID of the one we're on
    VoiceChannelsAtomic                     m_voiceChannels;
    VoiceChannelNameMap                     m_voiceChannelNamesByID;

    OpusPacketQueue                         m_opusQueue;
    ssp::OpusPacketDataInstance             m_opusPacketInProgress;
    ssp::OpusPacketDataInstance             m_opusPacketInReserve;
    bool                                    m_opusDispatchRunning;
};

// ---------------------------------------------------------------------------------------------------------------------
void Bot::State::update( DispatchStats& stats )
{
    stats.m_packetsSentBytes    = 0;
    stats.m_packetsSentCount    = 0;
    stats.m_bufferingProgress   = -1;

    stats.m_voiceBufferQueueState = m_voiceBufferQueueState;

    const size_t maxPacketsDispatchedPerUpdate = 4;

    // packet dispatch
    {
        std::lock_guard<std::mutex> voiceLock( m_liveVoiceGuard );
        if ( m_voiceState == Bot::VoiceState::Joined && m_liveVoice != nullptr )
        {
            // translate our UDP tuning value into the enum in dpp
            switch ( m_voiceUdpTuning )
            {
                default:
                case Bot::UdpTuning::Default:    m_liveVoice->udpSendTiming = dpp::discord_voice_client::UdpSendTiming::Default;     break;
                case Bot::UdpTuning::Delicate:   m_liveVoice->udpSendTiming = dpp::discord_voice_client::UdpSendTiming::Delicate;    break;
                case Bot::UdpTuning::Optimistic: m_liveVoice->udpSendTiming = dpp::discord_voice_client::UdpSendTiming::Optimistic;  break;
                case Bot::UdpTuning::Aggressive: m_liveVoice->udpSendTiming = dpp::discord_voice_client::UdpSendTiming::Aggressive;  break;
            }
            
            // no current packet block being drained? switch in the reserve
            if ( m_opusPacketInProgress == nullptr )
            {
                if ( m_opusPacketInReserve != nullptr )
                    std::swap( m_opusPacketInProgress, m_opusPacketInReserve );
            }
            // reserve empty? try dequeing one from the compressor thread's pile
            if ( m_opusPacketInReserve == nullptr )
            {
                m_opusQueue.try_dequeue( m_opusPacketInReserve );
            }

            // pre-buffer stage - wait until we've got a bunch of packets to begin working with before 
            // starting for real
            if ( !m_opusDispatchRunning )
            {
                const auto queueLength = m_opusQueue.size_approx();
                if ( queueLength >= 2 &&
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
                    stats.m_bufferingProgress /= 4.0f;
                }
            }

            // voice transmission is live, continuously stream packets to Discord
            if ( m_opusDispatchRunning && 
                 m_opusPacketInProgress )
            {
                const auto remainingPackets = std::min( m_opusPacketInProgress->m_opusPacketSizes.size() - m_opusPacketInProgress->m_dispatchedPackets, maxPacketsDispatchedPerUpdate );
                for ( auto pkt = 0; pkt < remainingPackets; pkt++ )
                {
                    const size_t packetLength = m_opusPacketInProgress->m_opusPacketSizes[m_opusPacketInProgress->m_dispatchedPackets];

                    // set working memory block with the data to send
                    m_workingMemory.opusData = &m_opusPacketInProgress->m_opusData[m_opusPacketInProgress->m_dispatchedSize];
                    m_workingMemory.opusLength = packetLength;

                    m_liveVoice->send_audio_opus_memopt( m_workingMemory );

                    m_opusPacketInProgress->m_dispatchedPackets++;
                    m_opusPacketInProgress->m_dispatchedSize += packetLength;

                    stats.m_packetsSentCount++;
                    stats.m_packetsSentBytes += (uint32_t)packetLength;
                    stats.m_averagePacketSize = m_opusPacketInProgress->m_averagePacketSize;
                }

                // depleted the current block of packets; this will be swapped out the next time through
                if ( m_opusPacketInProgress->m_dispatchedPackets >= m_opusPacketInProgress->m_opusPacketSizes.size() )
                {
                    m_opusPacketInProgress.reset();
                }
            }
        }
        else if ( m_voiceState == Bot::VoiceState::Flux )
        {
            // flush all the dispatch state when in Flux
            ssp::OpusPacketDataInstance flushQueue;
            while ( m_opusQueue.try_dequeue( flushQueue ) )
            {
                blog::discord( FMTX( "draining packet queue..." ) );
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

absl::Status Bot::initialise( app::ICoreServices& coreServices, const config::discord::Connection& configConnection )
{
    blog::discord( FMTX( "connection initialising ..." ) );
    try
    {
        m_state = std::make_unique<State>( coreServices, configConnection );
        return m_state->initialise();
    }
    catch ( dpp::exception* dpex )
    {
        m_state = nullptr;
        return absl::UnknownError( dpex->what() );
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

bool Bot::getUdpTuning( Bot::UdpTuning::Enum& tuning ) const
{
    if ( m_state )
    {
        tuning = m_state->m_voiceUdpTuning;
        return true;
    }
    return false;
}

bool Bot::setUdpTuning( const Bot::UdpTuning::Enum& tuning )
{
    if ( m_state )
    {
        m_state->m_voiceUdpTuning = tuning;
        return true;
    }
    return false;
}

bool Bot::getCurrentCompressionSetup( ssp::OpusStream::CompressionSetup& setup ) const
{
    if ( m_state )
    {
        setup = m_state->m_opusStreamProcessor->getCurrentCompressionSetup();
        return true;
    }
    return false;
}

bool Bot::setCompressionSetup( const ssp::OpusStream::CompressionSetup& setup )
{
    if ( m_state )
    {
        m_state->m_opusStreamProcessor->setCompressionSetup( setup );
        return true;
    }
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
        return m_state->m_voiceChannels;

    return nullptr;
}

} // namespace discord
