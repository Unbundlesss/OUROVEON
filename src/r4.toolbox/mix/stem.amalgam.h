//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
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
        m_wave.fill( 0.0f );
        m_beat.fill( 0.0f );
        m_low.fill( 0.0f );
        m_high.fill( 0.0f );
    }

    std::array< float, 8 >  m_wave;
    std::array< float, 8 >  m_beat;
    std::array< float, 8 >  m_low;
    std::array< float, 8 >  m_high;
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
        m_stemAmalgam.reset();
        m_stemAmalgamConsensus = 0.0f;
    }

    // tick value decays
    constexpr void update( const float deltaTime, const float timeToDecayInSec )
    {
        const float decayValue = (1.0f / timeToDecayInSec) * deltaTime;

        m_stemAmalgamConsensus = std::max( 0.0f, m_stemAmalgamConsensus - decayValue );
    }

    // blit the current state into the given exchange data block
    constexpr void copyToExchangeData( endlesss::toolkit::Exchange& exchangeData )
    {
        for ( auto stemI = 0U; stemI < 8; stemI++ )
        {
            exchangeData.m_stemBeat[stemI]   = m_stemAmalgam.m_beat[stemI];
            exchangeData.m_stemWave[stemI]   = m_stemAmalgam.m_wave[stemI];
            exchangeData.m_stemWaveLF[stemI] = m_stemAmalgam.m_low[stemI];
            exchangeData.m_stemWaveHF[stemI] = m_stemAmalgam.m_high[stemI];
        }
        exchangeData.m_consensusBeat = m_stemAmalgamConsensus;
    }

    // accessors for the current state
    ouro_nodiscard constexpr const std::array< float, 8 >& getWave() const { return m_stemAmalgam.m_wave; }
    ouro_nodiscard constexpr const std::array< float, 8 >& getBeat() const { return m_stemAmalgam.m_beat; }
    ouro_nodiscard constexpr float getConsensus() const { return m_stemAmalgamConsensus; }

protected:

    base::EventListenerID                   m_eventListenerStemDataAmalgam = base::EventListenerID::invalid();

    StemDataAmalgam                         m_stemAmalgam;
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
