//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#include "pch.h"
#include "base/instrumentation.h"
#include "net/bond.riffpush.h"

#include "nlohmann/json.hpp"

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/server.hpp>

namespace net {
namespace bond {

using ConnectionHandle  = websocketpp::connection_hdl;
using ConnectionSet     = std::set<ConnectionHandle, std::owner_less<ConnectionHandle>>;
using ServerEndpoint    = websocketpp::server<websocketpp::config::asio>;
using ClientEndpoint    = websocketpp::client<websocketpp::config::asio>;



// ---------------------------------------------------------------------------------------------------------------------
struct RiffPushServer::State
{
    DECLARE_NO_COPY_NO_MOVE( State );

    State()
    {
        m_server.clear_access_channels( websocketpp::log::alevel::all );
        m_server.set_access_channels( websocketpp::log::alevel::access_core );
        m_server.set_access_channels( websocketpp::log::alevel::app );

        m_server.init_asio();

        m_server.set_open_handler( [this]( ConnectionHandle hdl )
        {
            blog::app( "rp server opening connection" );
            m_connections.insert( hdl );
        });
        m_server.set_close_handler( [this]( ConnectionHandle hdl )
        {
            blog::app( "rp server closing connection" );
            m_connections.erase( hdl );
        });
        m_server.set_message_handler( [this]( ConnectionHandle hdl, ServerEndpoint::message_ptr msg )
        {
            const auto message = msg->get_payload();
            blog::app( "rp server msg : {}", message );

            if ( message.starts_with("V2RP") && message.length() > 4 )
            {
                std::string_view jsonSubstr( message.begin() + 4, message.end() );
                auto riffPushData = nlohmann::json::parse( jsonSubstr, nullptr, false );
                if ( riffPushData.is_discarded() )
                {
                    blog::error::app( "V2RP failed to parse" );
                    return;
                }

                const auto jsJamID      = riffPushData.at( "jam" );
                const auto jsRiffID     = riffPushData.at( "riff" );

                std::string jamIDString;
                std::string riffIDString;
                jsJamID.get_to( jamIDString );
                jsRiffID.get_to( riffIDString );

                // gain data is optional, check and construct the std::opt if its present and correct
                endlesss::types::RiffPlaybackPermutationOpt permutationOpt;
                if ( riffPushData.contains( "gains" ) )
                {
                    std::vector< float > stemGains;
                    const auto jsStemGains = riffPushData.at( "gains" );
                    jsStemGains.get_to( stemGains );

                    endlesss::types::RiffPlaybackPermutation resolvedPermutation;
                    if ( stemGains.size() == 8 )
                    {
                        for ( std::size_t idx = 0; idx < 8; idx++ )
                            resolvedPermutation.m_layerGainMultiplier[idx] = stemGains[idx];

                        permutationOpt.emplace( resolvedPermutation );
                    }
                    else
                    {
                        blog::error::app( "invalid stem gains ({}) received over V2RP", stemGains.size() );
                    }
                }

                if ( m_riffPushCallback )
                {
                    m_riffPushCallback( 
                        endlesss::types::JamCouchID( jamIDString ),
                        endlesss::types::RiffCouchID( riffIDString ),
                        permutationOpt );
                }
            }
        });
    }

    ~State()
    {
        std::ignore = stop();
    }

    ouro_nodiscard absl::Status start()
    {
        if ( m_serverRun )
        {
            ABSL_ASSERT( m_serverThread != nullptr );
            return absl::AlreadyExistsError( "existing server thread still running, cannot start" );
        }
        if ( m_serverState != BondState::Disconnected )
        {
            return absl::AlreadyExistsError( "server already running" );
        }

        m_serverState   = BondState::InFlux;
        m_serverRun     = true;
        m_serverThread  = std::make_unique<std::thread>( &State::serverThread, this );

        return absl::OkStatus();
    }

    ouro_nodiscard absl::Status stop()
    {
        if ( m_serverState == BondState::Disconnected )
        {
            return absl::UnavailableError( "server is already stopped" );
        }
        if ( m_serverRun )
        {
            m_serverRun = false;
            m_server.stop();
            m_serverThread->join();
            m_serverThread.reset();
        }
        ABSL_ASSERT( m_serverThread == nullptr );
        ABSL_ASSERT( m_serverState == BondState::InFlux );
        m_serverState = BondState::Disconnected;

        return absl::OkStatus();
    }

    ouro_nodiscard BondState getState() const
    {
        return m_serverState;
    }

    void setRiffPushedCallback( const RiffPushCallback& cb )
    {
        m_riffPushCallback = cb;
    }

    void clearRiffPushedCallback()
    {
        m_riffPushCallback = nullptr;
    }

private:

    void serverThread()
    {
        OuroveonThreadScope ots( OURO_THREAD_PREFIX "RiffPushServer" );

        try
        {
            m_server.listen( 9002 );
            m_server.start_accept();

            blog::app( "RiffPushServer running ... " );
            while ( m_serverRun )
            {
                m_serverState = BondState::Connected;
                m_server.run_one();
            }
            blog::app( "... RiffPushServer stopped" );
            m_serverState = BondState::InFlux;

            m_server.stop_listening();
        }
        catch ( websocketpp::exception const& e )
        {
            blog::app( "RiffPushServer failed with ws exception; {}", e.what() );
        }
        catch ( std::exception const& e )
        {
            blog::app( "RiffPushServer failed with exception; {}", e.what() );
        }
        blog::app( "... RiffPushServer thread exit" );
    }



    void sendText( const std::string& string )
    {
        for ( auto& cx : m_connections )
        {
            m_server.send( cx, string, websocketpp::frame::opcode::text );
        }
    }

    std::unique_ptr<std::thread>    m_serverThread;
    std::atomic_bool                m_serverRun     = false;
    std::atomic< BondState >        m_serverState   = BondState::Disconnected;

    RiffPushCallback                m_riffPushCallback;

    ServerEndpoint  m_server;
    ConnectionSet   m_connections;
};

RiffPushServer::RiffPushServer()
    : m_state( std::make_unique<State>() )
{
    
}

RiffPushServer::~RiffPushServer()
{
    
}

absl::Status RiffPushServer::start()
{
    return m_state->start();
}

absl::Status RiffPushServer::stop()
{
    return m_state->stop();
}

BondState RiffPushServer::getState() const
{
    return m_state->getState();
}

void RiffPushServer::setRiffPushedCallback(const RiffPushCallback& cb)
{
    m_state->setRiffPushedCallback( cb );
}

void RiffPushServer::clearRiffPushedCallback()
{
    m_state->clearRiffPushedCallback();
}

// ---------------------------------------------------------------------------------------------------------------------


// ---------------------------------------------------------------------------------------------------------------------
struct RiffPushClient::State
{
    DECLARE_NO_COPY_NO_MOVE( State );

    State( const std::string& appName )
    {
        m_client.set_user_agent( fmt::format("{}/{}", appName, OURO_FRAMEWORK_VERSION) );

        m_client.clear_access_channels( websocketpp::log::alevel::all );
        m_client.set_access_channels( websocketpp::log::alevel::connect );
        m_client.set_access_channels( websocketpp::log::alevel::disconnect );
        m_client.set_access_channels( websocketpp::log::alevel::app );

        m_client.init_asio();

        m_client.set_open_handler( [this]( ConnectionHandle hdl )
        {
            blog::app( "[RiffPushClient] -> connection opened" );
            m_clientState = BondState::Connected;
        });
        m_client.set_close_handler( [this]( ConnectionHandle hdl )
        {
            blog::app( "[RiffPushClient] <- connection closed" );

            m_clientConnectionPtr.reset();
            m_clientConnectionHandle.reset();
            m_clientState = BondState::Disconnected;
        });
        m_client.set_fail_handler( [this]( ConnectionHandle hdl )
        {
            blog::app( "[RiffPushClient] <- connection failed" );

            m_clientConnectionPtr.reset();
            m_clientConnectionHandle.reset();
            m_clientState = BondState::Disconnected;
        });
        m_client.set_message_handler( [this]( ConnectionHandle hdl, ServerEndpoint::message_ptr msg )
        {
            blog::app( "[RiffPushClient] >> client msg : {}", msg->get_payload() );
        });
    }

    ~State()
    {
        clientThreadStop();
    }

    ouro_nodiscard absl::Status connect( const std::string& uri )
    {
        if ( m_clientThreadRun )
        {
            ABSL_ASSERT( m_clientThread != nullptr );
            return absl::AlreadyExistsError( "existing client thread still running, cannot connect" );
        }
        if ( m_clientState != BondState::Disconnected )
        {
            return absl::AlreadyExistsError( "client already running or in-flux" );
        }

        // Create a new connection to the given URI
        websocketpp::lib::error_code ec;
        m_clientConnectionPtr = m_client.get_connection( uri, ec );
        if ( ec ) 
        {
            return absl::UnknownError( fmt::format( "unable to start [RiffPushClient] : {}", ec.message() ) );
        }
        m_clientConnectionHandle = m_clientConnectionPtr->get_handle();

        // ensure network processing thread is running
        clientThreadStart();

        m_clientState = BondState::InFlux;
        m_client.connect( m_clientConnectionPtr );

        return absl::OkStatus();
    }

    ouro_nodiscard absl::Status disconnect()
    {
        if ( m_clientState == BondState::Disconnected )
        {
            return absl::UnavailableError( "client is already disconnected" );
        }

        m_clientState = BondState::InFlux;
        try
        {
            m_client.close( m_clientConnectionHandle, websocketpp::close::status::normal, "disconnecting" );
        }
        catch ( websocketpp::exception const& e )
        {
            blog::app( "[RiffPushClient] disconnect exception; {}", e.what() );
        }

        return absl::OkStatus();
    }

    ouro_nodiscard BondState getState() const
    {
        return m_clientState;
    }

    void sendRiffID(
        const endlesss::types::JamCouchID& jamID,
        const endlesss::types::RiffCouchID& riffID,
        const endlesss::types::RiffPlaybackPermutationOpt& permutationOpt )
    {
        nlohmann::json msg = {
            { "jam" , jamID.value() },
            { "riff" , riffID.value() }
        };
        if ( permutationOpt.has_value() )
        {
            msg.emplace( "gains", permutationOpt->m_layerGainMultiplier );
        }

        const std::string riffPushJson = "V2RP" + msg.dump();
        m_client.send( 
            m_clientConnectionHandle,
            riffPushJson,
            websocketpp::frame::opcode::text );
    }


private:

    void clientThreadStart()
    {
        // already running?
        if ( m_clientThreadRun )
            return;

        if ( m_clientThread )
        {
            m_clientThread->join();
            m_clientThread.reset();
        }

        m_clientThreadRun   = true;
        m_clientThread      = std::make_unique<std::thread>( &State::clientThread, this );
    }

    void clientThreadStop()
    {
        if ( m_clientThreadRun )
        {
            m_client.stop();
        }
        if ( m_clientThread )
        {
            m_clientThread->join();
            m_clientThread.reset();
        }
        ABSL_ASSERT( m_clientThreadRun == false );
    }

    void clientThread()
    {
        OuroveonThreadScope ots( OURO_THREAD_PREFIX "RiffPushClient" );

        try
        {
            blog::app( "[RiffPushClient] clientThread run()" );
            m_client.run();
        }
        catch ( websocketpp::exception const& e )
        {
            blog::app( "[RiffPushClient] thread abort with ws exception; {}", e.what() );
        }
        catch ( std::exception const& e )
        {
            blog::app( "[RiffPushClient] thread abort with exception; {}", e.what() );
        }
        // recycle the client for future run()s
        m_client.reset();

        m_clientThreadRun = false;
        blog::app( "[RiffPushClient] thread exit" );
    }


    std::unique_ptr<std::thread>    m_clientThread;
    std::atomic_bool                m_clientThreadRun   = false;
    std::atomic< BondState >        m_clientState       = BondState::Disconnected;

    ClientEndpoint                  m_client;
    ClientEndpoint::connection_ptr  m_clientConnectionPtr;
    ConnectionHandle                m_clientConnectionHandle;
};

RiffPushClient::RiffPushClient( const std::string& appName )
    : m_state( std::make_unique<State>( appName ) )
{
    
}

RiffPushClient::~RiffPushClient()
{
    
}

absl::Status RiffPushClient::connect( const std::string& uri )
{
    return m_state->connect( uri );
}

absl::Status RiffPushClient::disconnect()
{
    return m_state->disconnect();
}

BondState RiffPushClient::getState() const
{
    return m_state->getState();
}

void RiffPushClient::pushRiffById(
    const endlesss::types::JamCouchID& jamID,
    const endlesss::types::RiffCouchID& riffID,
    const endlesss::types::RiffPlaybackPermutationOpt& permutationOpt )
{
    m_state->sendRiffID( jamID, riffID, permutationOpt );
}

} // namespace bond
} // namespace net

