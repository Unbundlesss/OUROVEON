//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"

#include "beam.fx.vst.h"
#include "beam.fx.databus.h"

#include "app/module.frontend.h"
#include "app/module.frontend.fonts.h"
#include "app/module.audio.h"

#include "effect/vst2/host.h"


namespace Fx {

static fs::path getSessionPath( const fs::path& appStashPath )
{
    return fs::absolute( appStashPath / "vst.session.json" );
}

static fs::path getParamsPath( const fs::path& appStashPath )
{
    return fs::absolute( appStashPath / "vst.params.json" );
}

// ---------------------------------------------------------------------------------------------------------------------
void VstStack::load( const fs::path& appStashPath, app::AudioModule& audioEngine, const app::AudioPlaybackTimeInfo* vstTimePtr )
{
    fs::path sessionPath = getSessionPath( appStashPath );

    Session sessionData;
    if ( fs::exists( sessionPath ) )
    {
        try
        {
            std::ifstream is( sessionPath );
            cereal::JSONInputArchive archive( is );

            // fetch last VSTs in use
            sessionData.serialize( archive );

            const auto vstCount = sessionData.vstPaths.size();
            m_instances.reserve( vstCount );
            m_parameters.reserve( vstCount );

            // instantiate the VSTs in order
            for ( auto vI = 0U; vI < vstCount; vI++ )
            {
                // fetch the original ID so that any parameter maps will match correctly; then also update the tracking variable so it starts beyond any we load
                uint64_t preservedID = sessionData.vstIDs[vI];
                m_incrementalLoadId = std::max( m_incrementalLoadId, preservedID ) + 1;

                // load VSTs and block until they have fully booted, as we need them alive to deserialize into
                auto* vstInst = addVST( audioEngine, vstTimePtr, sessionData.vstPaths[vI].c_str(), preservedID, true );

                blog::cfg( "Loading VST session [{}] ...", vstInst->getProductName() );

                // restore settings if they were stashed
                if ( !sessionData.vstStateData[vI].empty() )
                {
                    vstInst->deserialize( sessionData.vstStateData[vI] );
                }

                vstInst->requestActivationChange( sessionData.vstActive[vI] );
            }
        }
        catch ( cereal::Exception& cEx )
        {
            blog::error::cfg( "{}", cEx.what() );
        }
    }

    m_parameters.clear();
    fs::path paramsPath = getParamsPath( appStashPath );
    if ( fs::exists( paramsPath ) )
    {
        try
        {
            std::ifstream is( paramsPath );
            cereal::JSONInputArchive archive( is );

            archive(
                CEREAL_NVP( m_parameters )
            );
        }
        catch ( cereal::Exception& cEx )
        {
            blog::error::cfg( "{}", cEx.what() );
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
void VstStack::clear()
{
    for ( auto iPair : m_instances )
        delete iPair.second;
    m_instances.clear();
    m_parameters.clear();
}

// ---------------------------------------------------------------------------------------------------------------------
vst::Instance* VstStack::addVST( app::AudioModule& audioEngine, const app::AudioPlaybackTimeInfo* vstTimePtr, const char* vstFilename, const uint64_t vstLoadID, const bool haltUntilLoaded )
{
    const auto audioSampleRate = (float)audioEngine->getSampleRate();
    const auto audioBufferSize = audioEngine->getMaximumBufferSize();

    vst::Instance* vstInst = new vst::Instance(
        vstFilename,
        audioSampleRate,
        audioBufferSize,
        vstTimePtr );

    // disable by default
    vstInst->requestActivationChange( false );

    // loading is done on a thread, along with all the initial setup; we shaln't be waiting for it
    vstInst->beginLoadAsync();

    // on the occasion this function needs to ensure the VST is fully booted before it returns, we wait patiently here
    if ( haltUntilLoaded )
    {
        constexpr auto vstThreadBootWait = std::chrono::milliseconds( 100 );

        int32_t retries = 100;   // #hdd data drive this timeout
        while ( !vstInst->loaded() && retries > 0 )
        {
            std::this_thread::sleep_for( vstThreadBootWait );
            retries--;
        }

        if ( retries <= 0 )
        {
            blog::error::vst( "WARNING - VST thread may not have booted correctly [{}] for [{}], aborting", vstInst->getUniqueID(), vstFilename );
            return nullptr;
        }
    }

    {
        // add to the audio engine fx pile
        audioEngine->effectAppend( vstInst );

        // assign a unique loading order ID
        vstInst->setUserData( vstLoadID );

        // log in our local list
        m_order.push_back( vstLoadID );
        m_instances.emplace( vstLoadID, vstInst );
        m_parameters.emplace( vstLoadID, ParameterSet{} );
    }

    return vstInst;
}

// ---------------------------------------------------------------------------------------------------------------------
void VstStack::chooseNewVST( app::AudioModule& audioEngine, const app::AudioPlaybackTimeInfo* vstTimePtr, const app::module::Frontend& appFrontend )
{
    std::string chosenFile;
    if ( appFrontend.showFilePicker( "VST 2.x Plugin (*.dll)\0*.dll\0Any File\0*.*\0", chosenFile ) )
    {
        addVST( audioEngine, vstTimePtr, chosenFile.c_str(), m_incrementalLoadId++ );
    }
}

// ---------------------------------------------------------------------------------------------------------------------
bool VstStack::removeVST( app::AudioModule& audioEngine, const uint64_t loadID )
{
    if ( m_instances.find( loadID ) == m_instances.end() )
        return false;

    // unplug all VSTs, we'll then re-install in the revised order
    audioEngine->effectClearAll();

    // purge the VST from our lists
    vst::Instance* instance = m_instances[loadID];
    m_instances.erase( loadID );
    m_parameters.erase( loadID );
    m_order.erase( std::remove( m_order.begin(), m_order.end(), loadID ), m_order.end() );

    // unload it
    delete instance;

    // .. rebuild the audio engine VST pile
    for ( auto vstID : m_order )
    {
        auto vstInst = m_instances[vstID];
        audioEngine->effectAppend( vstInst );
    }
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
void VstStack::saveSession( const fs::path& appStashPath )
{
    fs::path sessionPath = getSessionPath( appStashPath );

    Session sessionData;
    for ( auto vstID : m_order )
    {
        auto vstInst = m_instances[vstID];

        blog::cfg( "Saving VST session [{}] ...", vstInst->getProductName() );

        sessionData.vstIDs.emplace_back( vstID );
        sessionData.vstPaths.emplace_back( vstInst->getPath() );
        sessionData.vstStateData.emplace_back( vstInst->serialize() );
        sessionData.vstActive.emplace_back( vstInst->isActive() );
    }
    {
        try
        {
            std::ofstream is( sessionPath );
            cereal::JSONOutputArchive archive( is );

            sessionData.serialize( archive );
        }
        catch ( cereal::Exception& cEx )
        {
            blog::error::cfg( "VstStack::saveSession() failed; {}", cEx.what() );
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
void VstStack::saveParameters( const fs::path& appStashPath )
{
    fs::path parameterPath = getParamsPath( appStashPath );
    {
        try
        {
            std::ofstream is( parameterPath );
            cereal::JSONOutputArchive archive( is );

            archive(
                CEREAL_NVP( m_parameters )
            );
        }
        catch ( cereal::Exception& cEx )
        {
            blog::error::cfg( "VstStack::saveParameters() failed; {}", cEx.what() );
        }
    }
}


// ---------------------------------------------------------------------------------------------------------------------
void VstStack::imgui(
    app::AudioModule&                   audioEngine,
    const app::AudioPlaybackTimeInfo*   vstTimePtr,
    const app::module::Frontend&        appFrontend,
    const DataBus&                      dataBus )
{
    // set buttons to match checkbox size and unify, ignoring button icon contents for a neater look
    const ImVec2 commonSquareSize = ImVec2( ImGui::GetFrameHeight(), ImGui::GetFrameHeight() );

    ImGui::Begin( "Effects" );

    if ( ImGui::BeginTable( "##vstx", 2, ImGuiTableFlags_Borders ) )
    {
        ImGui::TableSetupColumn( "VST Stack", ImGuiTableColumnFlags_WidthAutoResize );
        ImGui::TableSetupColumn( "Parameter Maps", ImGuiTableColumnFlags_None );
        ImGui::TableHeadersRow();
        ImGui::TableNextColumn();

        ImGui::Spacing();
        
        // button that will toggle a full bypass of all installed VSTs
        const bool isVSTBypassEnabled = audioEngine->isEffectBypassEnabled();
        {
            const char* bypassIcon = isVSTBypassEnabled ? ICON_FA_VOLUME_MUTE : ICON_FA_VOLUME_DOWN;

            ImGui::Scoped::ToggleButton bypassOn( isVSTBypassEnabled );
            if ( ImGui::Button( bypassIcon, commonSquareSize ) )
                audioEngine->toggleEffectBypass();

            ImGui::SameLine( 0.0f, 4.0f );
        }

        const float vstLeftPanelWidth = 300.0f;
        const float appendButtonWidth = vstLeftPanelWidth - (4.0f + commonSquareSize.x);
        if ( ImGui::Button( ICON_FA_PLUS_SQUARE " Append New...", ImVec2( appendButtonWidth, 0.0f) ) )
        {
            chooseNewVST( audioEngine, vstTimePtr, appFrontend );
        }
        ImGui::Spacing();

        static uint64_t selectedVST = -1;
        uint64_t vstToDelete = -1;

        {
            ImGui::BeginDisabledControls( isVSTBypassEnabled );

            for ( auto vstIndex : m_order )
            {
                vst::Instance* vsti = m_instances[vstIndex];

                const auto vstUID = vsti->getUserData();
                ImGui::PushID( (void*)vstUID );

                // VSTs are loaded asynchronously on their own thread; if they are not available, show in-progress placeholder
                if ( !vsti->loaded() )
                {
                    ImGui::BeginDisabledControls( true );
                    ImGui::Button( "", commonSquareSize ); ImGui::SameLine( 0.0f, 4.0f );
                    ImGui::Button( "", commonSquareSize ); ImGui::SameLine( 0.0f, 4.0f );
                    ImGui::Button( "", commonSquareSize ); ImGui::SameLine();
                    ImGui::EndDisabledControls( true );

                    ImGui::TextUnformatted( ICON_FA_HOURGLASS_START );
                }
                // VST is loaded, show proper controls
                else
                {
                    {
                        bool isVstOn = vsti->isActive();
                        if ( ImGui::Checkbox( "", &isVstOn ) )
                        {
                            vsti->requestActivationChange( isVstOn );
                        }

                        ImGui::SameLine( 0.0f, 4.0f );
                    }
                    {
                        ImGui::Scoped::ToggleButton selEdit( selectedVST == vstIndex );
                        if ( ImGui::Button( ICON_FA_LIST, commonSquareSize ) )
                        {
                            // clean up previously selected VST pre-switch
                            if ( selectedVST != -1 )
                                m_instances[selectedVST]->requestAutomationHook( nullptr );

                            selectedVST = vstIndex;
                        }
                        ImGui::SameLine( 0.0f, 4.0f );
                    }
                    if ( vsti->canChangeEditorState() )
                    {
                        const bool vstEditorOpen = vsti->editorIsOpen();

                        ImGui::Scoped::ToggleButton selEdit( vstEditorOpen );
                        if ( vstEditorOpen && ImGui::Button( ICON_FA_COG, commonSquareSize ) )
                        {
                            vsti->closeEditor();
                        }
                        else if ( !vstEditorOpen && ImGui::Button( ICON_FA_COG, commonSquareSize ) )
                        {
                            vsti->openEditor();
                        }
                    }
                    else
                    {
                        // VST is working on opening/closing UI or something that is inhibiting changing that state
                        ImGui::TextUnformatted( ICON_FA_FAN );
                    }

                    ImGui::SameLine();

                    ImGui::TextUnformatted( vsti->getProductName().c_str() );
                    ImGui::SameLine();
                    auto cursorX = ImGui::GetCursorPosX();

                    ImGui::SameLine( 0.0f, vstLeftPanelWidth - cursorX - 8.0f );
                    if ( ImGui::Button( ICON_FA_BACKSPACE ) )
                    {
                        vstToDelete = vstUID;
                    }
                }

                ImGui::PopID();
            }

            ImGui::EndDisabledControls( isVSTBypassEnabled );
        }

        // act upon a requested delete
        if ( vstToDelete != -1 )
        {
            // ensure all automation hooks removed before deleting
            for ( auto vstPair : m_instances )
                vstPair.second->requestAutomationHook( nullptr );
            
            if ( selectedVST == vstToDelete )
                selectedVST = -1;

            removeVST( audioEngine, vstToDelete );
        }

        ImGui::Spacing();
        ImGui::TableNextColumn();

        if ( selectedVST != -1 )
        {
            vst::Instance*       vsti = m_instances[ selectedVST ];
            ParameterSet&    paramSet = m_parameters[ selectedVST ];

            ImGui::Spacing();
            auto thisColumnSize = ImGui::MeasureSpace( ImVec2( -1.0f, 0.0f ) );
            thisColumnSize.x += ImGui::GetCursorPosX();
            if ( ImGui::Button( ICON_FA_SLIDERS_H " New Binding...", ImVec2(-1.0f, 0.0f) ) )
            {
                vsti->requestAutomationHook( nullptr ); // clear hook, as the lambda points to potentially old binding data
                paramSet.bindings.emplace_back("Unnamed");
            }
            ImGui::Spacing();

            std::string bindingToDelete;    // if set, we remove the named binding after this loop

            for ( auto& binding : paramSet.bindings )
            {
                static char bindingName[40];
                strcpy_s( bindingName, binding.name.c_str() );
                ImGui::PushID( &binding );

                ImGui::PushItemWidth( 100.0f );
                if ( ImGui::InputText( "##bind_name", bindingName, 40, ImGuiInputTextFlags_EnterReturnsTrue ) )
                {
                    binding.name = bindingName;
                }
                ImGui::PopItemWidth();

                ImGui::SameLine();
                ImGui::TextUnformatted( ICON_FA_LINK );

                ImGui::PushItemWidth( 80.0f );
                ImGui::SameLine();

                if ( ImGui::Combo( "##bus_idx", &binding.bus, dataBus.m_busNames.data(), DataBus::cBusCount ) )
                {
                }

                ImGui::PopItemWidth();

                ImGui::SameLine();
                if ( vsti->hasAutomationHook() )
                {
                    ImGui::Scoped::ToggleButton active( true, true );
                    if ( ImGui::Button( ICON_FA_CIRCLE_NOTCH ) )
                        vsti->requestAutomationHook( nullptr );
                }
                else
                {
                    // register to get a callback when a parameter changes, so the user can pick one easily off a UI
                    if ( ImGui::Button( ICON_FA_PLUS ) )
                    {
                        // open the editor for poking at if it isn't already shown
                        if ( !vsti->editorIsOpen() &&
                              vsti->canChangeEditorState() )
                              vsti->openEditor();

                        // set callback hook that adds a new param by index
                        vsti->requestAutomationHook( [&]( const int32_t paramIdx, const char* parameterName, const float curValue )
                            {
                                // ignore duplicates
                                if ( binding.parameterIndexRegistered( paramIdx ) )
                                    return;

                                binding.parameters.emplace_back( parameterName, paramIdx, curValue, curValue );
                            });
                    }
                }

                // right-align
                {
                    ImGui::SameLine();
                    auto cursorX = ImGui::GetCursorPosX();
                    ImGui::SameLine( 0.0f, thisColumnSize.x - cursorX - 18.0f );
                }
                if ( ImGui::Button( ICON_FA_BACKSPACE ) )
                {
                    bindingToDelete = binding.name;
                }

                int32_t paramToDelete = -1;

                ImGui::Indent();
                for ( auto& params : binding.parameters )
                {
                    ImGui::PushID( params.pidx );
                    ImGui::Text( "%24s", params.name.c_str() );

                    ImGui::SameLine();

                    ImGui::PushItemWidth( 140.0f );
                    ImGui::DragFloat2( "##param_range", &params.low, 0.01f, 0.0f, 1.0f );
                    ImGui::PopItemWidth();

                    // right-align
                    {
                        ImGui::SameLine();
                        auto cursorX = ImGui::GetCursorPosX();
                        ImGui::SameLine( 0.0f, thisColumnSize.x - cursorX - 18.0f );
                    }
                    if ( ImGui::Button( ICON_FA_BACKSPACE ) )
                    {
                        paramToDelete = params.pidx;
                    }

                    ImGui::PopID();
                }
                ImGui::Unindent();

                // remove a nominated parameter
                if ( paramToDelete != -1 )
                    binding.deleteParameterByIndex( paramToDelete );

                ImGui::PopID();
            }

            // remove a nominated binding if someone pressed delete
            if ( bindingToDelete.length() > 0 )
            {
                paramSet.deleteBindingByName( bindingToDelete );
            }

            ImGui::Spacing();
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------------------------------------------------
void VstStack::ParameterSet::syncToDataBus( const DataBus& bus, vst::Instance* vsti ) const
{
    for ( const auto& pb : bindings )
    {
        assert( pb.bus >= 0 && pb.bus < DataBus::cBusCount );

        const float value = bus.m_busOutputs[pb.bus];

        for ( const auto& param : pb.parameters )
        {
            const float scaledValue = base::lerp( param.low, param.high, value );
            vsti->setParameter( param.pidx, scaledValue );
        }
    }
}

} // namespace Fx