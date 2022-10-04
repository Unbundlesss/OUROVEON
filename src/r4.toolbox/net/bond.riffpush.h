//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "endlesss/core.types.h"

namespace net {
namespace bond {

enum BondState
{
    Disconnected,
    InFlux,
    Connected
};

// ---------------------------------------------------------------------------------------------------------------------
struct RiffPushServer
{
    using RiffPushCallback = std::function< void( const endlesss::types::JamCouchID&, const endlesss::types::RiffCouchID&, const endlesss::types::RiffPlaybackPermutationOpt& permutationOpt ) >;

    DECLARE_NO_COPY_NO_MOVE( RiffPushServer );

    RiffPushServer();
    ~RiffPushServer();

    ouro_nodiscard absl::Status start();
    ouro_nodiscard absl::Status stop();
    ouro_nodiscard BondState getState() const;

    void setRiffPushedCallback( const RiffPushCallback& cb );
    void clearRiffPushedCallback();

private:
    struct State;
    std::unique_ptr< State >    m_state;
};

// ---------------------------------------------------------------------------------------------------------------------
struct RiffPushClient
{
    DECLARE_NO_COPY_NO_MOVE( RiffPushClient );

    RiffPushClient( const std::string& appName );
    ~RiffPushClient();

    ouro_nodiscard absl::Status connect( const std::string& uri );
    ouro_nodiscard absl::Status disconnect();
    ouro_nodiscard BondState getState() const;

    inline void pushRiff(
        const endlesss::types::RiffComplete& riff,
        const endlesss::types::RiffPlaybackPermutationOpt& permutationOpt )
    {
        pushRiffById( riff.jam.couchID, riff.riff.couchID, permutationOpt );
    }

    void pushRiffById( 
        const endlesss::types::JamCouchID& jamID,
        const endlesss::types::RiffCouchID& riffID,
        const endlesss::types::RiffPlaybackPermutationOpt& permutationOpt );

private:
    struct State;
    std::unique_ptr< State >    m_state;
};

} // namespace bond
} // namespace net

