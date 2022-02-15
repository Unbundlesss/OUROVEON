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

namespace app {
namespace module {

// ---------------------------------------------------------------------------------------------------------------------
struct Midi : public Module
{
    Midi();
    ~Midi();

    // Module
    bool create( const app::Core& appCore ) override;
    void destroy() override;

    struct InputControl
    {
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
