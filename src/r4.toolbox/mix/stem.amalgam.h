//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//

#pragma once

#include "base/operations.h"
#include "base/eventbus.h"

#include "endlesss/toolkit.exchange.h"


namespace mix {

// ---------------------------------------------------------------------------------------------------------------------
struct StemDataAmalgam
{
    constexpr void reset()
    {
        m_beat.fill( false );
        m_energy.fill( 0.0f );
    }

    std::array< bool, 8 >   m_beat;
    std::array< float, 8 >  m_energy;
};

// ---------------------------------------------------------------------------------------------------------------------
struct StemDataProcessor
{
    StemDataProcessor()
    {
        reset();
    }

    // add or remove listener for the stem amalgam message to update current state
    absl::Status connect( base::EventBusPtr appEventBus );
    absl::Status disconnect( base::EventBusPtr appEventBus );

    constexpr void reset()
    {
        m_stemAmalgamBeats.fill( 0 );
        m_stemAmalgamEnergy.fill( 0 );
        m_stemAmalgamConsensus = 0.0f;
    }

    // tick value decays
    constexpr void update( const float deltaTime, const float timeToDecayInSec )
    {
        const float decayValue = ( 1.0f / timeToDecayInSec ) * deltaTime;

        for ( std::size_t i = 0; i < 8; i++ )
        {
            m_stemAmalgamBeats[i] = std::max( 0.0f, m_stemAmalgamBeats[i] - decayValue );
        }
        m_stemAmalgamConsensus = std::max( 0.0f, m_stemAmalgamConsensus - decayValue );
    }

    // blit the current state into the given exchange data block
    constexpr void copyToExchangeData( endlesss::toolkit::Exchange& exchangeData )
    {
        for ( auto stemI = 0U; stemI < 8; stemI++ )
        {
            exchangeData.m_stemPulse[stemI] = m_stemAmalgamBeats[stemI];
            exchangeData.m_stemEnergy[stemI] = m_stemAmalgamEnergy[stemI];
        }
        exchangeData.m_consensusBeat = m_stemAmalgamConsensus;
    }

    // accessors for the current state
    ouro_nodiscard constexpr const std::array< float, 8 >& getBeats() const { return m_stemAmalgamBeats; }
    ouro_nodiscard constexpr const std::array< float, 8 >& getEnergy() const { return m_stemAmalgamEnergy; }
    ouro_nodiscard constexpr float getConsensus() const { return m_stemAmalgamConsensus; }

protected:

    base::EventListenerID                   m_eventListenerStemDataAmalgam = base::EventListenerID::invalid();

    std::array< float, 8 >                  m_stemAmalgamBeats;
    std::array< float, 8 >                  m_stemAmalgamEnergy;
    float                                   m_stemAmalgamConsensus;

    void handleNewStemAmalgam( const base::IEvent& eventPtr );
};

} // namespace mix

// ---------------------------------------------------------------------------------------------------------------------
CREATE_EVENT_BEGIN( StemDataAmalgamGenerated )

    StemDataAmalgamGenerated( const mix::StemDataAmalgam& amalgam )
        : m_stemDataAmalgam( amalgam )
    {}

    mix::StemDataAmalgam        m_stemDataAmalgam;

CREATE_EVENT_END()
