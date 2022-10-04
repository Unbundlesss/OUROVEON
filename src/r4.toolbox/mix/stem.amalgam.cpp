//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//

#include "pch.h"

#include "mix/stem.amalgam.h"

namespace stdp = std::placeholders;

namespace mix {

// ---------------------------------------------------------------------------------------------------------------------
absl::Status StemDataProcessor::connect( base::EventBusPtr appEventBus )
{
    if ( m_eventListenerStemDataAmalgam != base::EventListenerID::invalid() )
        return absl::UnknownError( "already connected to stem data event bus" );

    m_eventListenerStemDataAmalgam = appEventBus->addListener(
        events::StemDataAmalgamGenerated::ID, 
        std::bind( &StemDataProcessor::handleNewStemAmalgam, this, stdp::_1 ) );

    return absl::OkStatus();
}

// ---------------------------------------------------------------------------------------------------------------------
absl::Status StemDataProcessor::disconnect( base::EventBusPtr appEventBus )
{
    if ( m_eventListenerStemDataAmalgam == base::EventListenerID::invalid() )
        return absl::UnknownError( "not connected to stem data event bus" );

    const auto status = appEventBus->removeListener( m_eventListenerStemDataAmalgam );
    if ( !status.ok() )
        return status;

    m_eventListenerStemDataAmalgam = base::EventListenerID::invalid();

    return absl::OkStatus();
}

// ---------------------------------------------------------------------------------------------------------------------
void StemDataProcessor::handleNewStemAmalgam( const base::IEvent& eventPtr )
{
    ABSL_ASSERT( eventPtr.getID() == events::StemDataAmalgamGenerated::ID );

    const events::StemDataAmalgamGenerated* stemDataEvent = dynamic_cast<const events::StemDataAmalgamGenerated*>(&eventPtr);
    ABSL_ASSERT( stemDataEvent != nullptr );

    int32_t simultaneousBeats = 0;
    for ( auto stemI = 0U; stemI < 8; stemI++ )
    {
        if ( stemDataEvent->m_stemDataAmalgam.m_beat[stemI] )
        {
            m_stemAmalgamBeats[stemI] = 1.0f;
            simultaneousBeats++;
        }

        m_stemAmalgamEnergy[stemI] = stemDataEvent->m_stemDataAmalgam.m_energy[stemI];
    }
    if ( simultaneousBeats >= 3 )
        m_stemAmalgamConsensus = 1.0f;
}

} // namespace mix

