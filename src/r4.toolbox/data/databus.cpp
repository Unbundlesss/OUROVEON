//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"

#include "data/databus.h"
#include "app/module.frontend.h"

namespace data {

static fs::path getConfigPath( const fs::path& appStashPath )
{
    return fs::absolute( appStashPath / "databus.config.json" );
}

void DataBus::BusConfiguration::imgui( const DataBus& dataBus, const data::Provider::AbilityFlags flags )
{
    const auto& busNames = dataBus.m_busNames;

    if ( flags & data::Provider::kUsesValue )
        ImGui::DragFloat( "Value", &m_value, 0.05f, 0.0f, 1.0f, "%.2f" );
    
    if ( flags & data::Provider::kUsesTime )
        ImGui::DragFloat( "Time Scale", &m_timeScale, 0.1f, 0.0f, 10.0f, "%.2f" );

    if ( flags & data::Provider::kUsesBus1 )
        ImGui::Combo( "##bus_A", &m_indexA, busNames.data(), cBusCount );

    if ( flags & data::Provider::kUsesBus2 )
        ImGui::Combo( "##bus_B", &m_indexB, busNames.data(), cBusCount );

    if ( flags & data::Provider::kUsesRemapping )
    {
        ImGui::DragFloatRange2( "Remap", &m_remapLow, &m_remapHigh, 0.1f, 0.0f, 1.0f, "%.2f" );
        ImGui::Checkbox( "Invert", &m_invert );
    }
}

void DataBus::save( const fs::path& appStashPath )
{
    fs::path configPath = getConfigPath( appStashPath );
    {
        try
        {
            std::ofstream is( configPath );
            cereal::JSONOutputArchive archive( is );

            // cereal doesn't deal in raw pointers
            std::array< std::string, cBusCount > busNames;
            for ( auto i = 0; i < cBusCount; i++ )
                busNames[i] = m_busNames[i];

            archive(
                CEREAL_NVP( busNames ),
                CEREAL_NVP( m_busConfigs )
            );
        }
        catch ( cereal::Exception& cEx )
        {
            blog::error::cfg( "DataBus::save() failed; {}", cEx.what() );
        }
    }
}

void DataBus::load( const fs::path& appStashPath )
{
    fs::path configPath = getConfigPath( appStashPath );

    if ( fs::exists( configPath ) )
    {
        try
        {
            std::ifstream is( configPath );
            cereal::JSONInputArchive archive( is );

            std::array< std::string, cBusCount > busNames;

            archive(
                CEREAL_NVP( busNames ),
                CEREAL_NVP( m_busConfigs )
            );

            // build providers instances
            for ( int32_t bus = 0; bus < cBusCount; bus++ )
            {
                strncpy( m_busNames[bus], busNames[bus].c_str(), 6 );

                if ( m_busProviders[bus] != nullptr )
                    delete m_busProviders[bus];
                m_busProviders[bus] = nullptr;

                const auto newProviderUID = m_busConfigs[bus].m_providerUID;
                if ( newProviderUID != 0 )
                {
                    const auto factoryIter = m_providerFactory.find( newProviderUID );
                    if ( factoryIter != m_providerFactory.end() )
                        m_busProviders[bus] = factoryIter->second();
                }
            }
        }
        catch ( cereal::Exception& cEx )
        {
            blog::error::cfg( "DataBus::load() failed; {}", cEx.what() );
        }
    }
}

void DataBus::updateBus( const size_t bI, const float currentTime )
{
    if ( m_busOutputsUpdated[bI] )
        return;

    const BusConfiguration& bc = m_busConfigs[bI];

    if ( m_busProviders[bI] == nullptr )
    {
        m_busOutputs[bI] = bc.m_value;
    }
    else
    {
        Provider::Input input;
        input.m_value   = bc.m_value;
        input.m_time    = currentTime * bc.m_timeScale;

        if ( bc.m_indexA >= 0 )
        {
            updateBus( bc.m_indexA, currentTime );
            input.m_bus1 = m_busOutputs[bc.m_indexA];
        }
        if ( bc.m_indexB >= 0 )
        {
            updateBus( bc.m_indexB, currentTime );
            input.m_bus2 = m_busOutputs[bc.m_indexB];
        }


        float generatedValue = m_busProviders[bI]->generate( input );
        float remappedValue = std::clamp( (generatedValue - bc.m_remapLow) / (bc.m_remapHigh - bc.m_remapLow), 0.0f, 1.0f );

        if ( bc.m_invert )
            remappedValue = 1.0f - remappedValue;

        m_busOutputs[bI] = remappedValue;
    }

    m_busOutputsUpdated[bI] = true;
}

void DataBus::update()
{
    const float currentTime = (float)ImGui::GetTime();

     m_busOutputsUpdated.fill( false );

     for ( auto bI = 0U; bI < cBusCount; bI++ )
     {
         updateBus( bI, currentTime );
     }
}

void DataBus::imgui()
{
    ImGui::Begin( "Data Bus" );

    const float controlKnobRadius = 30.0f;
    const auto contentRegionAvailable = ImGui::GetContentRegionAvail();
    const int32_t controlKnobColumns = (int32_t)std::floor( contentRegionAvailable.x / ((controlKnobRadius * 2.0f) + 25.0f) );

    if ( controlKnobColumns <= 0 )
    {
        ImGui::End();
        return;
    }

    const float sizeOfEditorBlock = m_busEditorOpen ? 175.0f : 28.0f;

    if ( ImGui::BeginChild( "##knob_bank", ImVec2( 0, contentRegionAvailable.y - sizeOfEditorBlock ), false, ImGuiWindowFlags_AlwaysAutoResize ) )
    {
        if ( ImGui::BeginTable( "##databusknobs", controlKnobColumns, ImGuiTableFlags_None ) )
        {
            for ( int32_t bus = 0; bus < cBusCount; bus++ )
            {
                ImGui::PushID( bus );
                ImGui::TableNextColumn();

                const float tableCellWidth = ImGui::GetContentRegionAvail().x + 6.0f;
                ImGui::Dummy( ImVec2( (tableCellWidth * 0.5f) - 30.0f, 0.0f ) ); ImGui::SameLine( 0, 0 );

                if ( m_busProviders[bus] == nullptr )
                    ImGui::KnobFloat( "##knob", 30.0f, &m_busConfigs[bus].m_value, 0.0f, 1.0f, 100.0f, 0.0f );
                else
                    ImGui::KnobFloat( "##knob", 30.0f, &m_busOutputs[bus], 0.0f, 1.0f, -1.0f, 0.0f );

                ImGui::RadioButton( m_busNames[bus], &m_busEditIndex, bus );
                ImGui::PopID();
            }

            ImGui::EndTable();
        }
    }
    ImGui::EndChild();

    if ( ImGui::BeginChild( "##ctrl_editor", ImVec2( 0, sizeOfEditorBlock - 5.0f ), false, ImGuiWindowFlags_AlwaysAutoResize ) )
    {
        ImGui::SetNextItemOpen( m_busEditorOpen );
        if ( ImGui::CollapsingHeader( "Edit" ) )
        {
//            ImGui::PushItemWidth( 100.0f );

            if ( ImGui::InputText( "Name", m_busNames[m_busEditIndex], 7, ImGuiInputTextFlags_EnterReturnsTrue ) )
            {
                //strupr( m_busNames[m_busEditIndex], 10 );
            }

            const auto currentProviderUID = m_busConfigs[m_busEditIndex].m_providerUID;

            std::string providerName = "Manual";
            getNameForProvider( currentProviderUID, providerName );

            if ( ImGui::BeginCombo( "##provider", providerName.c_str(), 0) )
            {
                for ( const auto& providers : m_providerNames )
                {
                    const auto newProviderUID = providers.first;
                    const bool isSelected     = ( currentProviderUID == newProviderUID );

                    if ( ImGui::Selectable( providers.second.c_str(), isSelected ) )
                    {
                        if ( m_busProviders[m_busEditIndex] != nullptr )
                            delete m_busProviders[m_busEditIndex];
                        
                        const auto factoryIter = m_providerFactory.find( newProviderUID );

                        m_busProviders[m_busEditIndex] = factoryIter->second();
                        m_busConfigs[m_busEditIndex].m_providerUID = newProviderUID;
                    }

                    if ( isSelected )
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

//            ImGui::PopItemWidth();

            data::Provider::AbilityFlags abilityFlags = data::Provider::kNothingSpecial;
            if ( m_busProviders[m_busEditIndex] != nullptr )
                abilityFlags = m_busProviders[m_busEditIndex]->flags();

            m_busConfigs[m_busEditIndex].imgui( *this, abilityFlags );
            m_busEditorOpen = true;
        }
        else
        {
            m_busEditorOpen = false;
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

} // namespace data
