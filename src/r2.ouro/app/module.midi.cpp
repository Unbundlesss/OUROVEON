//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"
#include "app/core.h"
#include "app/module.midi.h"

#include "RtMidi.h"

namespace app {
namespace module {

// ---------------------------------------------------------------------------------------------------------------------
struct Midi::State : public Midi::InputControl
{
    using MidiMessageQueue = mcc::ReaderWriterQueue< app::midi::Message >;


    State( base::EventBusClient&& eventBusClient )
        : m_eventBusClient( std::move(eventBusClient) )
    {
        m_midiIn          = std::make_unique<RtMidiIn>();

        blog::core( "initialised RtMidi " RTMIDI_VERSION " ({})", m_midiIn->getApiName( m_midiIn->getCurrentApi() ) );

        m_inputPortCount  = m_midiIn->getPortCount();

        m_inputPortNames.reserve( m_inputPortCount );
        for ( uint32_t i = 0; i < m_inputPortCount; i++ )
        {
            const auto portName = m_midiIn->getPortName( i );
            m_inputPortNames.push_back( portName );
            blog::core( " -> [{}] {}", i, portName );
        }

        m_midiIn->setCallback( &onMidiData, this );
    }

    ~State()
    {
        try
        {
            m_midiIn->cancelCallback();

            if ( m_midiIn->isPortOpen() )
                m_midiIn->closePort();
        }
        catch ( RtMidiError& error )
        {
            blog::error::core( "RtMidi error during shutdown ({})", error.getMessage() );
        }
    }

    // decode midi message and enqueue anything we understand into our threadsafe pile of messages
    static void onMidiData( double timeStamp, std::vector<unsigned char>* message, void* userData )
    {
        Midi::State* state = (Midi::State*)userData;
        
        // https://www.midi.org/specifications-old/item/table-1-summary-of-midi-message
        if ( message && message->size() <= 4 )
        {
            const uint8_t controlMessage = message->at( 0 );
            const uint8_t channelNumber  = ( controlMessage & 0x0F );
            const uint8_t channelMessage = ( controlMessage & 0xF0 );
           
            if ( channelMessage == midi::NoteOn::u7Type )
            {
                const uint8_t u7OnKey = message->at( 1 ) & 0x7F;
                const uint8_t u7OnVel = message->at( 2 ) & 0x7F;

                blog::core( "midi::NoteOn  (#{}) [{}] [{}]", channelNumber, u7OnKey, u7OnVel );

                const ::events::MidiEvent midiMsg( { timeStamp, midi::Message::Type::NoteOn, u7OnKey, u7OnVel }, app::module::MidiDeviceID(0) );
                state->m_eventBusClient.Send< ::events::MidiEvent >( midiMsg );
            }
            else
            if ( channelMessage == midi::NoteOff::u7Type )
            {
                const uint8_t u7OffKey = message->at( 1 ) & 0x7F;
                const uint8_t u7OffVel = message->at( 2 ) & 0x7F;

                blog::core( "midi::NoteOff (#{}) [{}] [{}]", channelNumber, u7OffKey, u7OffVel );

                state->m_midiMessageQueue.emplace( timeStamp, midi::Message::Type::NoteOff, u7OffKey, u7OffVel );
            }
            else
            if ( channelMessage == midi::ControlChange::u7Type )
            {
                const uint8_t u7CtrlNum = message->at( 1 ) & 0x7F;
                const uint8_t u7CtrlVal = message->at( 2 ) & 0x7F;

                blog::core( "midi::ControlChange(#{}) [{}] = {}", channelNumber, u7CtrlNum, u7CtrlVal );

                state->m_midiMessageQueue.emplace( timeStamp, midi::Message::Type::ControlChange, u7CtrlNum, u7CtrlVal );
            }
        }
    }

    bool getInputPorts( std::vector<std::string>& names ) override
    {
        names.assign( m_inputPortNames.begin(), m_inputPortNames.end() );
        return true;
    }

    bool openInputPort( const uint32_t index ) override
    {
        try
        {
            m_midiIn->openPort( index );
        }
        catch ( RtMidiError& error )
        {
            blog::error::core( "RtMidi could not open port {} ({})", index, error.getMessage() );
            return false;
        }
        m_inputPortOpenedIndex = index;
        return true;
    }

    bool getOpenPortIndex( uint32_t& result ) override
    {
        try
        {
            if ( m_midiIn->isPortOpen() )
            {
                result = m_inputPortOpenedIndex;
                return true;
            }
        }
        catch ( RtMidiError& )
        {
            blog::error::core( "getOpenPortIndex() but no open port" );
            return false;
        }
        return false;
    }

    bool closeInputPort() override
    {
        try
        {
            if ( m_midiIn->isPortOpen() )
                m_midiIn->closePort();
        }
        catch ( RtMidiError& error )
        {
            blog::error::core( "RtMidi could not close port {} ({})", m_inputPortOpenedIndex, error.getMessage() );
            return false;
        }
        return true;
    }

    void processMessages( const std::function< void( const app::midi::Message& ) >& processor )
    {
        if ( !m_midiIn->isPortOpen() )
            return;

        app::midi::Message msg;
        while ( m_midiMessageQueue.try_dequeue( msg ) )
        {
            processor( msg );
        }
    }


    std::unique_ptr< RtMidiIn >     m_midiIn;
    uint32_t                        m_inputPortCount = 0;
    std::vector< std::string >      m_inputPortNames;
    uint32_t                        m_inputPortOpenedIndex = 0;

    base::EventBusClient            m_eventBusClient;

    MidiMessageQueue                m_midiMessageQueue;
};

// ---------------------------------------------------------------------------------------------------------------------
Midi::Midi()
{
}

// ---------------------------------------------------------------------------------------------------------------------
Midi::~Midi()
{
}

// ---------------------------------------------------------------------------------------------------------------------
absl::Status Midi::create( const app::Core* appCore )
{
    const auto baseStatus = Module::create( appCore );
    if ( !baseStatus.ok() )
        return baseStatus;

    m_state = std::make_unique<State>( appCore->getEventBusClient() );
    return absl::OkStatus();
}

// ---------------------------------------------------------------------------------------------------------------------
void Midi::destroy()
{
    m_state.reset();

    Module::destroy();
}

// ---------------------------------------------------------------------------------------------------------------------
std::vector< app::module::MidiDevice > Midi::fetchListOfInputDevices()
{
    std::vector< app::module::MidiDevice > inputDevices; 

    auto midiIn = std::make_unique<RtMidiIn>();

    const auto inputPortCount = midiIn->getPortCount();

    inputDevices.reserve( inputPortCount );
    for ( auto pI = 0U; pI < inputPortCount; pI++ )
    {
        const auto portName = midiIn->getPortName( pI );
        inputDevices.emplace_back( portName );
    }

    return inputDevices;
}

// ---------------------------------------------------------------------------------------------------------------------
Midi::InputControl* Midi::getInputControl()
{
    if ( m_state != nullptr )
    {
        return m_state.get();
    }
    return nullptr;
}

// ---------------------------------------------------------------------------------------------------------------------
void Midi::processMessages( const std::function< void( const app::midi::Message& ) >& processor )
{
    if ( m_state != nullptr )
    {
        return m_state->processMessages( processor );
    }
}

} // namespace module
} // namespace app
