//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "beam.fx.databus.providers.h"

namespace Fx {

// ---------------------------------------------------------------------------------------------------------------------
struct DataBus
{
    static constexpr size_t cBusCount = 9;

    using DataProducer = std::function<void>( float v, float t );

    struct BusConfiguration
    {
        BusConfiguration()
            : m_providerUID( 0 )
            , m_value( 0.0f )
            , m_timeScale( 1.0f )
            , m_remapLow( 0.0f )
            , m_remapHigh( 1.0f )
            , m_indexA( -1 )
            , m_indexB( -1 )
            , m_invert( false )
        {}

        template<class Archive>
        void serialize( Archive& archive )
        {
            archive(
                CEREAL_NVP( m_providerUID ),
                CEREAL_NVP( m_value ),
                CEREAL_NVP( m_timeScale ),
                CEREAL_NVP( m_remapLow ),
                CEREAL_NVP( m_remapHigh ),
                CEREAL_NVP( m_indexA ),
                CEREAL_NVP( m_indexB ),
                CEREAL_NVP( m_invert )
            );
        }

        void imgui( const DataBus& dataBus, const Fx::Provider::AbilityFlags flags );

        uint32_t        m_providerUID;

        float           m_value;
        float           m_timeScale;
        float           m_remapLow;
        float           m_remapHigh;
        int32_t         m_indexA;
        int32_t         m_indexB;
        bool            m_invert;
    };
    using BusConfigurations = std::array< BusConfiguration, cBusCount >;

    DataBus()
        : m_busEditIndex( 0 )
        , m_busEditorOpen( false )
    {
        m_busOutputs.fill( 0.0f );
        m_busProviders.fill( nullptr );

        for ( auto bI = 0U; bI < cBusCount; bI++ )
        {
            m_busNames[bI] = new char[10];
            strcpy_s( m_busNames[bI], 10, "------" );
        }
    }

    ~DataBus()
    {
        for ( auto name : m_busNames )
            delete name;
    }

    void save( const fs::path& appStashPath );
    void load( const fs::path& appStashPath );


    void updateBus( const size_t bI, const float currentTime );
    void update();

    void imgui();


    inline bool getNameForProvider( const uint32_t uid, std::string& nameOutput ) const
    {
        const auto iter = m_providerNames.find( uid );
        if ( iter != m_providerNames.end() )
        {
            nameOutput = iter->second;
            return true;
        }
        return false;
    }

    ProviderFactory                     m_providerFactory;
    ProviderNames                       m_providerNames;

    BusConfigurations                   m_busConfigs;
    std::array< char*, cBusCount >      m_busNames;
    std::array< float, cBusCount >      m_busOutputs;
    std::array< bool,  cBusCount >      m_busOutputsUpdated;

    std::array< Provider*, cBusCount >  m_busProviders;

    int32_t                             m_busEditIndex;

    bool                                m_busEditorOpen;
};

} // namespace Fx
