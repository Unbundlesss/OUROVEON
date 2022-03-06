//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "base/metaenum.h"

#include "ssp/ssp.stream.opus.h"

namespace config { namespace discord { struct Connection; } }
namespace app {
    struct ICoreServices;
    namespace module { struct Audio; }
    using AudioModule = std::unique_ptr<module::Audio>;
}

namespace dpp { class channel; }

namespace discord {

struct VoiceChannel
{
    VoiceChannel() = default;
    VoiceChannel( const dpp::channel* channelData );

    uint16_t        m_order;
    uint64_t        m_id;
    std::string     m_name;
    uint16_t        m_bitrate;
};
using VoiceChannels         = std::vector< VoiceChannel >;
using VoiceChannelsPtr      = std::shared_ptr< const VoiceChannels >;
using VoiceChannelsAtomic   = VoiceChannelsPtr; // #HDD CXX20 atomic shared_ptr is not widely available :(

struct GuildMetadata
{
    uint32_t        m_shardID;
    std::string     m_name;
};
using GuildMetadataOptional = std::optional< GuildMetadata >;

struct Bot
{
    enum class ConnectionPhase
    {
        Uninitialised,
        Booting,
        RequestingGuildData,
        Ready,
        UnableToStart
    };

    enum class VoiceState
    {
        NoConnection,           // not through guild connection phase
        NotJoined,              // joining a voice channel is possible; bot is not in any active call
        Flux,                   // in the process of joining or leaving
        Joined                  // we're on the mic
    };

    // mapping timing tuning values added to DPP; declared as meta so we get easy imgui support
#define _UDP_TUNING(_action) \
      _action(Default)       \
      _action(Delicate)      \
      _action(Optimistic)    \
      _action(Aggressive)
    REFLECT_ENUM( UdpTuning, uint32_t, _UDP_TUNING );
#undef _UDP_TUNING


    struct DispatchStats
    {
        uint32_t    m_packetBlobQueueLength = 0;

        uint32_t    m_packetsSentCount      = 0;
        uint32_t    m_packetsSentBytes      = 0;

        uint32_t    m_voiceBufferQueueState = 0;
        uint32_t    m_averagePacketSize     = 0;

        float       m_bufferingProgress     = 0;
        bool        m_dispatchRunning       = false;
    };

    Bot();
    ~Bot();

    bool initialise( app::ICoreServices& coreServices, const config::discord::Connection& configConnection );
    inline bool isInitialised() const { return m_initialised && m_state != nullptr; }


    // call from main thread to let bot do regular main-thread processing tasks
    void update( DispatchStats& stats );


    // examine what stage of the start-up process we're in; anything other than Ready and most other get##() calls
    // won't return anything interesting
    ConnectionPhase         getConnectionPhase() const;

    std::string             getBotName() const;
    GuildMetadataOptional   getGuildMetadata() const;

    // generic catch all for "are we in some kind of async flux state doing stuff" - be it setting up the 
    // initial connection, binding to a voice channel, etc.
    bool                    isBotBusy() const;

    // voice channel management

    // get list of voice channels available, assuming connection phase is Ready (otherwise returns nullptr)
    VoiceChannelsPtr        getVoiceChannels() const;
    // voice state tracks if we're on an active voice channel or not (or in flux between those states)
    VoiceState              getVoiceState() const;
    // if voice is connected, this is the ID of the channel we're live on; otherwise 0
    uint64_t                getVoiceChannelLiveID() const;

    // ask to join the given channel, if VoiceState is NotJoined
    bool joinVoiceChannel( const VoiceChannel& vc );
    // ask to leave our current channel, if VoiceState is Joined
    bool leaveVoiceChannel();


    // change UDP tuning
    bool getUdpTuning( UdpTuning::Enum& tuning ) const;
    bool setUdpTuning( const UdpTuning::Enum& tuning );

    // tunnel through to set Opus compressor tuning
    bool getCurrentCompressionSetup( ssp::OpusStream::CompressionSetup& setup ) const;
    bool setCompressionSetup( const ssp::OpusStream::CompressionSetup& setup );

protected:

    bool                        m_initialised;

    struct State;
    std::unique_ptr< State >    m_state;

};

} // namespace discord