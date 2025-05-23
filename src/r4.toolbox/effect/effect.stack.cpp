//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#include "pch.h"

#if OURO_FEATURE_NST24

#include "effect/effect.stack.h"
#include "data/databus.h"

#include "app/module.frontend.h"
#include "app/module.frontend.fonts.h"
#include "app/module.audio.h"
#include "app/core.h"

#include "effect/vst2/host.h"
#include "platform_folders.h"

namespace effect {

static fs::path getSessionPath( const fs::path& appStashPath, const std::string& stackName )
{
    return fs::absolute( appStashPath / fmt::format( "effects.{}.session.json", stackName ) );
}

static fs::path getParamsPath( const fs::path& appStashPath, const std::string& stackName )
{
    return fs::absolute( appStashPath / fmt::format( "effects.{}.params.json", stackName ) );
}

// ---------------------------------------------------------------------------------------------------------------------
void EffectStack::load( const fs::path& appStashPath )
{
    fs::path sessionPath = getSessionPath( appStashPath, m_stackName );

    m_lastBrowsedPath = ".";
#if OURO_PLATFORM_WIN
    m_lastBrowsedPath = sago::GetWindowsProgramFiles();
#endif

    Session sessionData;
    if ( fs::exists( sessionPath ) )
    {
        try
        {
            std::ifstream is( sessionPath );
            cereal::JSONInputArchive archive( is );

            // fetch last plugins in use
            sessionData.serialize( archive );

            const auto pluginCount = sessionData.vstPaths.size();
            m_instances.reserve( pluginCount );
            m_parameters.reserve( pluginCount );

            // instantiate the plugins in order
            for ( auto vI = 0U; vI < pluginCount; vI++ )
            {
                // fetch the original ID so that any parameter maps will match correctly; then also update the tracking variable so it starts beyond any we load
                int64_t preservedID = sessionData.vstIDs[vI];
                m_incrementalLoadId = std::max( m_incrementalLoadId, preservedID ) + 1;

                // load VSTs and block until they have fully booted, as we need them alive to deserialize into
                auto* nstInst = addNST( sessionData.vstPaths[vI].c_str(), preservedID, true );
                if ( nstInst != nullptr )
                {
                    blog::cfg( "Loading plugin session [{}] ...", nstInst->getProductName() );

                    // restore settings if they were stashed
                    if ( !sessionData.vstStateData[vI].empty() )
                    {
                        nstInst->deserialize( sessionData.vstStateData[vI] );
                    }

                    nstInst->requestActivationChange( sessionData.vstActive[vI] );
                }
            }

            if ( !sessionData.lastBrowsedPath.empty() )
                m_lastBrowsedPath = sessionData.lastBrowsedPath;
        }
        catch ( cereal::Exception& cEx )
        {
            blog::error::cfg( "{}", cEx.what() );
        }
    }

    m_parameters.clear();
    fs::path paramsPath = getParamsPath( appStashPath, m_stackName );
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
void EffectStack::clear()
{
    for ( auto iPair : m_instances )
        delete iPair.second;
    m_instances.clear();
    m_parameters.clear();
}

// ---------------------------------------------------------------------------------------------------------------------
nst::Instance* EffectStack::addNST( const char* vstFilename, const int64_t vstLoadID, const bool haltUntilLoaded )
{
    const auto audioSampleRate = (float)m_effectContainer->getEffectSampleRate();
    const auto audioBufferSize = m_effectContainer->getEffectMaximumBufferSize();

    nst::Instance* nstInst = new nst::Instance(
        vstFilename,
        audioSampleRate,
        audioBufferSize,
        m_audioTimingInfoPtr );

    // disable by default
    nstInst->requestActivationChange( false );

    // loading is done on a thread, along with all the initial setup; we shaln't be waiting for it
    nstInst->beginLoadAsync();

    // on the occasion this function needs to ensure the VST is fully booted before it returns, we wait patiently here
    if ( haltUntilLoaded )
    {
        constexpr auto nstThreadBootWait = std::chrono::milliseconds( 100 );

        int32_t retries = 100;   // #hdd data drive this timeout
        while ( !nstInst->loaded() && retries > 0 )
        {
            std::this_thread::sleep_for( nstThreadBootWait );
            retries--;

            // bail on load failure
            if ( nstInst->failedToLoad() )
                retries = 0;
        }

        if ( retries <= 0 )
        {
            blog::error::plug( "WARNING - plugin thread may not have booted correctly [{}] for [{}], aborting", nstInst->getUniqueID(), vstFilename );

            delete nstInst;
            return nullptr;
        }
    }

    {
        // add to the audio engine fx pile
        m_effectContainer->effectAppend( nstInst );

        // assign a unique loading order ID
        nstInst->setUserData( vstLoadID );

        // log in our local list
        m_order.push_back( vstLoadID );
        m_instances.emplace( vstLoadID, nstInst );
        m_parameters.emplace( vstLoadID, ParameterSet{} );
    }

    return nstInst;
}

// ---------------------------------------------------------------------------------------------------------------------
void EffectStack::chooseNewVST( app::CoreGUI& coreGUI )
{
    auto fileDialog = std::make_unique< ImGuiFileDialog >();

    fileDialog->OpenDialog(
        "FxFileDlg",
        "Choose VST 2.x Plugin",
        ".dll",
        m_lastBrowsedPath.c_str(),
        1,
        nullptr,
        ImGuiFileDialogFlags_Modal );

    std::ignore = coreGUI.activateFileDialog( std::move(fileDialog), [this]( ImGuiFileDialog& dlg )
    {
        addNST( dlg.GetFilePathName().c_str(), m_incrementalLoadId++ );

        m_lastBrowsedPath = dlg.GetCurrentPath();
    });
}

// ---------------------------------------------------------------------------------------------------------------------
bool EffectStack::removeVST( const int64_t loadID )
{
    if ( m_instances.find( loadID ) == m_instances.end() )
        return false;

    // unplug all VSTs, we'll then re-install in the revised order
    m_effectContainer->effectClearAll();

    // purge the VST from our lists
    nst::Instance* instance = m_instances[loadID];
    m_instances.erase( loadID );
    m_parameters.erase( loadID );
    m_order.erase( std::remove( m_order.begin(), m_order.end(), loadID ), m_order.end() );

    // unload it
    delete instance;

    // .. rebuild the audio engine VST pile
    for ( auto vstID : m_order )
    {
        auto nstInst = m_instances[vstID];
        m_effectContainer->effectAppend( nstInst );
    }
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
void EffectStack::saveSession( const fs::path& appStashPath )
{
    fs::path sessionPath = getSessionPath( appStashPath, m_stackName );

    Session sessionData;
    for ( auto vstID : m_order )
    {
        auto nstInst = m_instances[vstID];

        blog::cfg( "Saving VST session [{}] ...", nstInst->getProductName() );

        sessionData.vstIDs.emplace_back( vstID );
        sessionData.vstPaths.emplace_back( nstInst->getPath() );
        sessionData.vstStateData.emplace_back( nstInst->serialize() );
        sessionData.vstActive.emplace_back( nstInst->isActive() );
    }
    sessionData.lastBrowsedPath = m_lastBrowsedPath;

    {
        try
        {
            std::ofstream is( sessionPath );
            cereal::JSONOutputArchive archive( is );

            sessionData.serialize( archive );
        }
        catch ( cereal::Exception& cEx )
        {
            blog::error::cfg( "EffectStack::saveSession() failed; {}", cEx.what() );
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
void EffectStack::saveParameters( const fs::path& appStashPath )
{
    fs::path parameterPath = getParamsPath( appStashPath, m_stackName );
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
            blog::error::cfg( "EffectStack::saveParameters() failed; {}", cEx.what() );
        }
    }
}


// ---------------------------------------------------------------------------------------------------------------------
void EffectStack::imgui(
    app::CoreGUI&           coreGUI,
    const data::DataBus*    dataBus )
{
    // set buttons to match checkbox size and unify, ignoring button icon contents for a neater look
    const ImVec2 commonSquareSize = ImVec2( ImGui::GetFrameHeight(), ImGui::GetFrameHeight() );

    // only add parameter mapping controls if a dataBus was sent through
    const bool addDataBusUI  = (dataBus != nullptr);
    const int32_t numColumns = addDataBusUI ? 2 : 1;

    if ( ImGui::BeginTable( "##vstx", numColumns, ImGuiTableFlags_Borders ) )
    {
        // only bother adding headers if we are using more than just the list of effects
        if ( addDataBusUI )
        {
            ImGui::TableSetupColumn( "Effect Chain", ImGuiTableColumnFlags_WidthFixed ); // #IMGUIUPGRADE
            ImGui::TableSetupColumn( "Parameter Maps", ImGuiTableColumnFlags_None );
            ImGui::TableHeadersRow();
        }
        ImGui::TableNextColumn();

        ImGui::Spacing();
        
        // button that will toggle a full bypass of all installed VSTs
        const bool isVSTBypassEnabled = m_effectContainer->isEffectBypassEnabled();
        {
            const char* bypassIcon = isVSTBypassEnabled ? ICON_FA_BAN : ICON_FA_POWER_OFF;

            ImGui::Scoped::ToggleButton bypassOn( isVSTBypassEnabled );
            if ( ImGui::Button( bypassIcon, commonSquareSize ) )
                m_effectContainer->toggleEffectBypass();

            ImGui::SameLine( 0.0f, 4.0f );
        }

        const float vstLeftPanelWidth = 300.0f;
        const float appendButtonWidth = vstLeftPanelWidth - (4.0f + commonSquareSize.x);
        {
            ImGui::Scoped::ButtonTextAlignLeft leftAlign;
            if ( ImGui::Button( ICON_FA_SQUARE_PLUS " Append VST...", ImVec2( appendButtonWidth, 0.0f ) ) )
            {
                chooseNewVST( coreGUI );
            }
        }
        ImGui::Spacing();

        static int64_t selectedVST = -1;
        int64_t vstToDelete = -1;
        int64_t vstOrderIndexToSwapUpwards = -1;

        {
            ImGui::BeginDisabledControls( isVSTBypassEnabled );

            const bool offerSwapButton = ( ImGui::GetMergedModFlags() & ImGuiModFlags_Alt );

            for ( auto orderIndex = 0; orderIndex < m_order.size(); orderIndex ++ )
            {
                const auto vstIndex = m_order[orderIndex];
                nst::Instance* vsti = m_instances[vstIndex];

                const auto vstUID = vsti->getUserData();
                ImGui::PushID( (void*)vstUID );

                const bool vstFailedToLoad          = vsti->failedToLoad();
                const bool vstLoadingInProgress     = !vsti->loaded();

                bool addDeletionButton = false;

                // VSTs are loaded asynchronously on their own thread; if they are not available, show in-progress placeholder
                if ( vstLoadingInProgress || 
                     vstFailedToLoad )
                {
                    ImGui::BeginDisabledControls( true );
                    ImGui::Button( "", commonSquareSize );
                    ImGui::SameLine( 0.0f, 4.0f );

                    // parameter map button hidden without a databus to use
                    if ( dataBus != nullptr )
                    {
                        ImGui::Button( "", commonSquareSize ); 
                        ImGui::SameLine( 0.0f, 4.0f );
                    }

                    ImGui::Button( "", commonSquareSize );
                    ImGui::SameLine();
                    ImGui::EndDisabledControls( true );

                    if ( vstFailedToLoad )
                    {
                        ImGui::TextUnformatted( ICON_FA_HEART_CRACK " Failed To Load" );
                        addDeletionButton = true; // load failed, only option to delete now
                    }
                    else
                        ImGui::TextUnformatted( ICON_FA_HOURGLASS_START );
                }
                // VST is loaded, show proper controls
                else
                {
                    // enable/disable button
                    {
                        bool isVstOn = vsti->isActive();
                        if ( ImGui::Checkbox( "", &isVstOn ) )
                        {
                            vsti->requestActivationChange( isVstOn );
                        }

                        ImGui::SameLine( 0.0f, 4.0f );
                    }
                    // only show parameter map selection if we are offering that UX
                    if ( dataBus != nullptr )
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

                    if ( offerSwapButton )
                    {
                        if ( orderIndex > 0 )
                        {
                            if ( ImGui::Button( ICON_FA_ANGLE_UP, commonSquareSize ) )
                            {
                                vstOrderIndexToSwapUpwards = orderIndex;
                            }
                        }
                        else
                        {
                            ImGui::Dummy( commonSquareSize );
                        }
                    }
                    else
                    {
                        // button to open editor
                        if ( vsti->canChangeEditorState() )
                        {
                            const bool vstEditorOpen = vsti->editorIsOpen();

                            ImGui::Scoped::ToggleButton selEdit( vstEditorOpen );
                            if ( vstEditorOpen && ImGui::Button( ICON_FA_GEAR, commonSquareSize ) )
                            {
                                vsti->closeEditor();
                            }
                            else if ( !vstEditorOpen && ImGui::Button( ICON_FA_GEAR, commonSquareSize ) )
                            {
                                vsti->openEditor();
                            }
                        }
                        else
                        {
                            // VST is working on opening/closing UI or something that is inhibiting changing that state
                            ImGui::BeginDisabledControls( false );
                            ImGui::Button( ICON_FA_FAN, commonSquareSize );
                            ImGui::EndDisabledControls( false );
                        }
                    }

                    ImGui::SameLine();
                    ImGui::TextUnformatted( vsti->getProductName() );

                    addDeletionButton = true;
                }

                if ( addDeletionButton )
                {
                    ImGui::SameLine();
                    auto cursorX = ImGui::GetCursorPosX();

                    ImGui::SameLine( 0.0f, vstLeftPanelWidth - cursorX - 8.0f );
                    if ( ImGui::Button( ICON_FA_DELETE_LEFT ) )
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

            removeVST( vstToDelete );
        }
        if ( vstOrderIndexToSwapUpwards != -1 )
        {
            std::swap( m_order[vstOrderIndexToSwapUpwards], m_order[vstOrderIndexToSwapUpwards - 1] );

            // .. rebuild the audio engine VST pile
            m_effectContainer->effectClearAll();
            for ( auto vstID : m_order )
            {
                auto nstInst = m_instances[vstID];
                m_effectContainer->effectAppend( nstInst );
            }
        }

        ImGui::Spacing();

        if ( dataBus != nullptr )
        {
            ImGui::TableNextColumn();

            if ( selectedVST != -1 )
            {
                nst::Instance* vsti = m_instances[selectedVST];
                ParameterSet& paramSet = m_parameters[selectedVST];

                ImGui::Spacing();
                auto thisColumnSize = ImGui::MeasureSpace( ImVec2( -1.0f, 0.0f ) );
                thisColumnSize.x += ImGui::GetCursorPosX();
                if ( ImGui::Button( ICON_FA_SLIDERS " New Binding...", ImVec2( -1.0f, 0.0f ) ) )
                {
                    vsti->requestAutomationHook( nullptr ); // clear hook, as the lambda points to potentially old binding data
                    paramSet.bindings.emplace_back( "Unnamed" );
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

                    if ( ImGui::Combo( "##bus_idx", &binding.bus, dataBus->m_busNames.data(), data::DataBus::cBusCount ) )
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
                                } );
                        }
                    }

                    // right-align
                    {
                        ImGui::SameLine();
                        auto cursorX = ImGui::GetCursorPosX();
                        ImGui::SameLine( 0.0f, thisColumnSize.x - cursorX - 18.0f );
                    }
                    if ( ImGui::Button( ICON_FA_DELETE_LEFT ) )
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
                        if ( ImGui::Button( ICON_FA_DELETE_LEFT ) )
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
        }

        ImGui::EndTable();
    }
}

// ---------------------------------------------------------------------------------------------------------------------
void EffectStack::ParameterSet::syncToDataBus( const data::DataBus& bus, nst::Instance* vsti ) const
{
    for ( const auto& pb : bindings )
    {
        ABSL_ASSERT( pb.bus >= 0 && pb.bus < data::DataBus::cBusCount );

        const float value = bus.m_busOutputs[pb.bus];

        for ( const auto& param : pb.parameters )
        {
            const float scaledValue = base::lerp( param.low, param.high, value );
            vsti->setParameter( param.pidx, scaledValue );
        }
    }
}

} // namespace effect

#endif // OURO_FEATURE_NST24
