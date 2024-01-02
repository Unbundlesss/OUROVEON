//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#include "pch.h"

#include "base/paging.h"
#include "base/eventbus.h"

#include "app/imgui.ext.h"
#include "app/module.frontend.fonts.h"
#include "colour/preset.h"

#include "endlesss/core.types.h"

#include "mix/common.h"

#include "ux/riff.history.h"


namespace ux {

struct HistoryRecord
{
    endlesss::types::JamCouchID      m_jamCouchID;
    endlesss::types::RiffCouchID     m_riffCouchID;
};

// ---------------------------------------------------------------------------------------------------------------------
struct RiffHistory::State
{
    State( base::EventBusClient eventBus )
        : m_eventBusClient( std::move( eventBus ) )
    {
        APP_EVENT_BIND_TO( MixerRiffChange );
    }

    ~State()
    {
        APP_EVENT_UNBIND( MixerRiffChange );
    }

    void event_MixerRiffChange( const events::MixerRiffChange* eventData );


    void imgui( app::CoreGUI& coreGUI );


    base::EventBusClient            m_eventBusClient;

    base::EventListenerID           m_eventLID_MixerRiffChange = base::EventListenerID::invalid();

};


// ---------------------------------------------------------------------------------------------------------------------
void RiffHistory::State::event_MixerRiffChange( const events::MixerRiffChange* eventData )
{
    // might be a empty riff, only track actual riffs
    if ( eventData->m_riff != nullptr )
    {
       // m_historyRecords.emplace_back( eventData->m_riff->m_riffData.jam.couchID, eventData->m_riff->m_riffData.riff.couchID );
    }
}

// ---------------------------------------------------------------------------------------------------------------------
void RiffHistory::State::imgui( app::CoreGUI& coreGUI )
{
    // management window
    if ( ImGui::Begin( ICON_FA_CLOCK_ROTATE_LEFT " Playback History###playback_history" ) )
    {

    }
    ImGui::End();
}

// ---------------------------------------------------------------------------------------------------------------------
RiffHistory::RiffHistory( base::EventBusClient eventBus )
    : m_state( std::make_unique<State>( std::move( eventBus ) ) )
{
}

// ---------------------------------------------------------------------------------------------------------------------
RiffHistory::~RiffHistory()
{
}

// ---------------------------------------------------------------------------------------------------------------------
void RiffHistory::imgui( app::CoreGUI& coreGUI )
{
    m_state->imgui( coreGUI );
}

} // namespace ux
