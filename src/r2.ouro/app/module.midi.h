//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once
#include "app/module.h"
#include "app/module.midi.msg.h"

#include "base/id.hash.h"
#include "base/text.h"

namespace app {
namespace module {

// ---------------------------------------------------------------------------------------------------------------------
struct _midi_device_id {};
using MidiDeviceID = base::id::HashWrapper<_midi_device_id>;

// represent a MIDI device by name; this is then translated to a low-level device index at the point of Open'ing -- 
// which can fail if the device already got unplugged
struct MidiDevice
{
    MidiDevice() = delete;
    MidiDevice( const std::string& name )
        : m_name( name )
        , m_uid( base::HashString64( name ) )
    {}

    constexpr bool operator == ( const MidiDevice& rhs ) const { return rhs.m_uid == m_uid; }
    constexpr bool operator != ( const MidiDevice& rhs ) const { return rhs.m_uid != m_uid; }

    ouro_nodiscard constexpr const std::string& getName() const { return m_name; }
    ouro_nodiscard constexpr const MidiDeviceID& getUID() const { return m_uid; }

private:
    std::string     m_name;
    MidiDeviceID    m_uid;
};

// ---------------------------------------------------------------------------------------------------------------------
struct Midi : public Module
{
    DECLARE_NO_COPY_NO_MOVE( Midi );

    Midi();
    ~Midi();

    // Module
    absl::Status create( const app::Core* appCore ) override;
    void destroy() override;
    virtual std::string getModuleName() const override { return "Midi"; };

    // create a temporary MidiIn interface and assemble a list of usable input devices
    static std::vector< MidiDevice > fetchListOfInputDevices();


    struct InputControl
    {
        virtual ~InputControl() {}
        virtual bool getInputPorts( std::vector<std::string>& names ) = 0;
        virtual bool openInputPort( uint32_t index ) = 0;
        virtual bool getOpenPortIndex( uint32_t& result ) = 0;
        virtual bool closeInputPort() = 0;
    };
    // request api for controlling midi input; or nullptr if this MIDI boot process failed
    InputControl* getInputControl();


    // client code should call this from the main thread (or a single, consistent thread at least) to 
    // iterate and drain the current queue of decoded MIDI messages received from the back-end
    //
    // if MIDI isn't booted or there's just nothing to do, this call does nothing
    void processMessages( const std::function< void( const app::midi::Message& ) >& processor );

protected:

    struct State;
    std::unique_ptr< State >    m_state;
};

using MidiModule = std::unique_ptr<module::Midi>;

} // namespace module
} // namespace app

// ---------------------------------------------------------------------------------------------------------------------
CREATE_EVENT_BEGIN( MidiEvent )

    MidiEvent( const app::midi::Message& msg, const app::module::MidiDeviceID& deviceUID )
        : m_msg( msg )
        , m_deviceUID( deviceUID )
    {}

    app::midi::Message          m_msg;
    app::module::MidiDeviceID   m_deviceUID;

CREATE_EVENT_END()
