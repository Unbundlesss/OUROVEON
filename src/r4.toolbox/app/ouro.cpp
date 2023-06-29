//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"

#include "spacetime/chronicle.h"
#include "base/text.h"
#include "colour/preset.h"

#include "config/frontend.h"
#include "config/data.h"
#include "config/audio.h"
#include "config/spectrum.h"

#include "app/ouro.h"
#include "app/imgui.ext.h"
#include "app/module.audio.h"
#include "app/module.frontend.h"
#include "app/module.frontend.fonts.h"

#include "mix/common.h"
#include "mix/stem.amalgam.h"

#include "endlesss/all.h"

#include "platform_folders.h"

namespace app {


// ---------------------------------------------------------------------------------------------------------------------
// endlesss::live::RiffFetchServices
int32_t OuroApp::getSampleRate() const
{
    ABSL_ASSERT( m_mdAudio != nullptr );    // can't be fetching riffs until all the core services are running
    return m_mdAudio->getSampleRate();
}

// ---------------------------------------------------------------------------------------------------------------------
// run a loop with a dedicated configuration page for choosing output device per session; once done, we jump to the app code
//
int OuroApp::EntrypointGUI()
{
    static constexpr std::array< const char*, 4 > cSampleRateLabels { "44100", "48000", "88200", "96000" };
    static constexpr std::array< uint32_t,    4 > cSampleRateValues {  44100 ,  48000 ,  88200 ,  96000  };
    static constexpr std::array< const char*, 6 > cBufferSizeLabels {  "Auto",    "64",   "256",   "512",  "1024",  "2048" };
    static constexpr std::array< uint32_t,    6 > cBufferSizeValues {      0 ,     64 ,    256 ,    512 ,   1024 ,   2048  };
    static constexpr std::array< const char*, 6 > cVibeRenderLabels {   "512",  "1024",  "2048",  "4096" };
    static constexpr std::array< uint32_t,    6 > cVibeRenderValues {    512 ,   1024 ,   2048 ,   4096  };


    // try and fetch last audio settings from the stash; doesn't really matter if we can't, it is just saved defaults
    // for the config screen
    config::Audio audioConfig;
    const auto audioLoadResult = config::load( *this, audioConfig );
    if ( audioLoadResult != config::LoadResult::Success )
    {
        blog::cfg( "No default audio settings found, using defaults" );
    }

    // spectrum/scope config block also passed to audio engine setup; defaults is fine
    config::Spectrum audioSpectrumConfig;
    const auto spectrumLoadResult = config::load( *this, audioSpectrumConfig );
    if ( spectrumLoadResult != config::LoadResult::Success &&
         spectrumLoadResult != config::LoadResult::CannotFindConfigFile )
    {
        blog::cfg( "Spectrum config file failed to load, using defaults" );
    }

    // get default value strings for saved config values to display in the UI; these are updated if selection changes
    std::string previewSampleRate = ImGui::ValueArrayPreviewString( cSampleRateLabels, cSampleRateValues, audioConfig.sampleRate );
    std::string previewBufferSize = ImGui::ValueArrayPreviewString( cBufferSizeLabels, cBufferSizeValues, audioConfig.bufferSize );

    // stash current TZ
    const auto timezoneLocal = date::current_zone();
    const auto timezoneUTC = date::locate_zone( "Etc/UTC" );

    // add UTC/Server time on left of status bar
    const auto sbbTimeStatusLeftID = registerStatusBarBlock( app::CoreGUI::StatusBarAlignment::Left, 500.0f, [=]()
    {
        auto t  = date::make_zoned( timezoneUTC, std::chrono::system_clock::now() );
        auto tf = date::format( spacetime::defaultDisplayTimeFormatTZ, t );
        const auto serverTime = fmt::format( FMTX( "{} | {}" ), timezoneUTC->name(), tf );

        ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32_WHITE );
        ImGui::TextUnformatted( serverTime );
        ImGui::PopStyleColor();
    });
    // add local timezone time on right 
    const auto sbbTimeStatusRightID = registerStatusBarBlock( app::CoreGUI::StatusBarAlignment::Right, 500.0f, [=]()
    {
        auto t  = date::make_zoned( timezoneLocal, std::chrono::system_clock::now() );
        auto tf = date::format( spacetime::defaultDisplayTimeFormatTZ, t );
        const auto localTime = fmt::format( FMTX( "{} | {}" ), tf, timezoneLocal->name() );

        // right-align
        const auto localTimeLength = ImGui::CalcTextSize( localTime );
        ImGui::Dummy( ImVec2( 475.0f - localTimeLength.x, 0.0f ) );

        ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32_WHITE );
        ImGui::TextUnformatted( localTime );
        ImGui::PopStyleColor();
    });


    // load any saved configs
    config::endlesss::Auth endlesssAuth;
    /* const auto authLoadResult    = */ config::load( *this, endlesssAuth );

    // #HDD check and do something with config load results?


    app::AudioDeviceQuery adq;
    int32_t chosenDeviceIndex;

    // fn to take the current settings and run a query
    const auto fnUpdateDeviceQuery = [&]
    {
        chosenDeviceIndex = -1;

        adq.findSuitable( audioConfig );

        // see if the last device we had selected still exists
        if ( !audioConfig.lastDevice.empty() )
        {
            for ( int32_t dI = 0; dI < (int32_t)adq.m_deviceNames.size(); dI++ )
            {
                if ( adq.m_deviceNames[dI] == audioConfig.lastDevice )
                {
                    chosenDeviceIndex = dI;
                    break;
                }
            }
        }
    };
    // prefill a device query; this will be re-run if the settings change
    fnUpdateDeviceQuery();

    // task / state for fetching metadata 
    auto asyncFetchState = endlesss::cache::Jams::AsyncFetchState::None;
    std::string asyncState;
    tf::Taskflow taskFlow;

    // buffer for changing data storage path; #HDD TODO per platform limits for dataStoragePathBufferSize
    constexpr size_t dataStoragePathBufferSize = 255;
    char dataStoragePathBuffer[dataStoragePathBufferSize];
    memset( dataStoragePathBuffer, 0, dataStoragePathBufferSize );
    if ( m_configData == std::nullopt )
    {
        // if we are starting with no config data, put a suggestion in
        const fs::path initalPathSuggestion = fs::path( sago::getDocumentsFolder() ) / "OUROVEON";
        strcpy( dataStoragePathBuffer, initalPathSuggestion.string().c_str() );
    }
    else
    {
        // already got one!
        strcpy( dataStoragePathBuffer, m_configData->storageRoot.c_str() );
    }

    // ::filesystem variant of the buffer and computed free-space data, updated via a lambda that can be called
    // if the buffer entry changes during imgui update
    fs::path        dataStoragePath;
    std::error_code dataStorageSpaceInfoError;
    fs::space_info  dataStorageSpaceInfo;
    std::string     dataStorageSpaceText;

    const auto UpdateLocalFsRecordsForDataStoragePath = [&]()
    {
        dataStoragePath          = fs::path( dataStoragePathBuffer );
        dataStorageSpaceInfo     = std::filesystem::space( dataStoragePath, dataStorageSpaceInfoError );

        if ( dataStorageSpaceInfoError )
            dataStorageSpaceText = dataStorageSpaceInfoError.message();
        else
            dataStorageSpaceText = base::humaniseByteSize( "Free space : ", dataStorageSpaceInfo.available );

        // in the case we have an existing config and someone just changed the data path in the UI, 
        // nix the data instance so we have to go through the [Accept & Configure] flow
        if ( m_configData != std::nullopt &&
             m_configData->storageRoot != dataStoragePath.string() )
        {
            m_configData   = std::nullopt;
            m_storagePaths = std::nullopt;
        }
    };
    UpdateLocalFsRecordsForDataStoragePath();


    // used if we need to pop a modal showing some error feedback
    static constexpr auto popupErrorModalName = "Error";
    std::string popupErrorMessage;



    bool successfulBreakFromLoop = false;

    // configuration preflight
    while ( beginInterfaceLayout( CoreGUI::VF_WithStatusBar ) )
    {
        std::string popupErrorMessageToDisplay;

        ImGui::SetNextWindowContentSize( ImVec2( 700.0f, 100.0f ) );
        if ( ImGui::BeginPopupModal( popupErrorModalName, nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize ) )
        {
            ImGui::TextUnformatted( popupErrorMessage.c_str() );

            ImGui::Spacing();
            ImGui::Spacing();

            if ( ImGui::Button( " Close " ) )
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }


        std::string progressionInhibitionReason;

        const float  configWindowColumn1 = 500.0f;
        const float  configWindowColumn2 = 500.0f;
        const ImVec2 configWindowSize = ImVec2( configWindowColumn1 + configWindowColumn2, 630.0f );
        const ImVec2 viewportWorkSize = ImGui::GetMainViewport()->GetCenter();

        ImGui::SetNextWindowPos( viewportWorkSize - ( configWindowSize * 0.5f ) );
        ImGui::SetNextWindowContentSize( configWindowSize );

        ImGui::Begin( "Framework Preflight | Version " OURO_FRAMEWORK_VERSION, nullptr,
            ImGuiWindowFlags_NoResize           |
            ImGuiWindowFlags_NoSavedSettings    |
            ImGuiWindowFlags_NoCollapse );
        {
            constexpr float perColumnIndent = 5.0f;
            constexpr float perBlockIndent = 10.0f;

            // ImGuiTableFlags_SizingFixedFit #IMGUIUPGRADE
            if ( ImGui::BeginTable( "##databusknobs", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings ) )
            {
                ImGui::TableSetupColumn( "X", ImGuiTableColumnFlags_WidthFixed, configWindowColumn1 );
                ImGui::TableSetupColumn( "Y", ImGuiTableColumnFlags_WidthFixed, configWindowColumn2 );

                ImGui::TableNextColumn();
                ImGui::Indent( perColumnIndent );


                // ---------------------------------------------------------------------------------------------------------
                // hi everybody 

                {
                    ImGui::Spacing();
                    ImGui::Spacing();

                    ImGui::PushFont( m_mdFrontEnd->getFont( app::module::Frontend::FontChoice::LargeLogo ) );
                    ImGui::PushStyleColor( ImGuiCol_Text, ImGui::GetColorU32( ImGuiCol_NavHighlight ) );
                    ImGui::TextUnformatted( "OUROVEON ");
                    ImGui::PopStyleColor();
                    ImGui::PopFont();

                    ImGui::Spacing();
                    ImGui::Spacing();
                    ImGui::ColumnSeparatorBreak();
                }


                // ---------------------------------------------------------------------------------------------------------
                // where do we store our data
                // a complicated setup system for choosing a path to use for data storage and all the edge cases that includes
                {
                    std::error_code filesysError;

                    m_mdFrontEnd->titleText( "Installation" );
                    ImGui::Indent( perBlockIndent );

                    const ImVec2 pathButtonSize( configWindowColumn1 - 35.0f, 32.0f );
                    ImGui::PushItemWidth( pathButtonSize.x );
                    
                    ImGui::TextUnformatted( "Set Data Storage Root Path :" );
                    const bool textAccept = ImGui::InputText( "##DataPath", dataStoragePathBuffer, dataStoragePathBufferSize, ImGuiInputTextFlags_EnterReturnsTrue );
                    if ( textAccept || ImGui::IsItemDeactivatedAfterEdit() )
                    {
                        UpdateLocalFsRecordsForDataStoragePath();
                    }
                    const bool textFocus = ImGui::IsItemActive();

                    ImGui::Spacing();

                    // please type something
                    if ( dataStoragePath.empty() )
                    {
                        ImGui::BeginDisabledControls( true );
                        ImGui::Button( "No path entered", pathButtonSize );
                        ImGui::EndDisabledControls( true );
                    }
                    // offer to create the directory if it isn't present; requiring it to exist lets us do a better job upfront of 
                    // validating a potential storage site and showing some stats too
                    else if ( !fs::exists( dataStoragePath, filesysError ) )
                    {
                        ImGui::TextUnformatted( "This path does not exist. Would you like to create it?" );
                        ImGui::BeginDisabledControls( textFocus );
                        if ( ImGui::Button( "Create Directory", pathButtonSize ) )
                        {
                            fs::create_directories( dataStoragePath, filesysError );

                            if ( filesysError )
                            {
                                popupErrorMessageToDisplay = fmt::format( "Error occured while creating directory :\n{}", filesysError.message() );
                            }
                            else
                            {
                                // recheck the storage space, update state variables
                                UpdateLocalFsRecordsForDataStoragePath();
                            }
                        }
                        ImGui::EndDisabledControls( textFocus );
                    }
                    else
                    {
                        // show the drive space for the current path, just in case you realise you're about to obliterate
                        // your last good gigabyte with stems
                        ImGui::TextUnformatted( dataStorageSpaceText.c_str() );

                        if ( !fs::is_directory( dataStoragePath, filesysError ) )
                        {
                            ImGui::BeginDisabledControls( true );
                            ImGui::Button( "Path is not a directory", pathButtonSize );
                            ImGui::EndDisabledControls( true );
                        }
                        if ( dataStoragePath == dataStoragePath.root_path() )
                        {
                            ImGui::BeginDisabledControls( true );
                            ImGui::Button( "Cannot use a root drive path", pathButtonSize );
                            ImGui::EndDisabledControls( true );
                        }
                        else
                        {
                            const bool alreadyConfigured = ( m_configData.has_value() && m_storagePaths.has_value() );

                            ImGui::BeginDisabledControls( textFocus || alreadyConfigured );
                            if ( ImGui::Button( textFocus ? "[ Path Being Edited ]" : "Validate & Save", pathButtonSize ) )
                            {
                                config::Data newDataConfig;
                                newDataConfig.storageRoot = dataStoragePath.string();

                                m_configData = newDataConfig;

                                // save new state
                                const auto authSaveResult = config::save( *this, newDataConfig );
                                if ( authSaveResult != config::SaveResult::Success )
                                {
                                    popupErrorMessageToDisplay = fmt::format( "Unable to save storage configuration to [{}]", config::getFullPath< config::Data >( *this ).string() );
                                }

                                // temporarily create a StoragePaths instance so that we can then create the subdirectories / validate their state
                                m_storagePaths = StoragePaths( m_configData.value(), GetAppCacheName() );
                                if ( !m_storagePaths->tryToCreateAndValidate() )
                                {
                                    popupErrorMessageToDisplay = fmt::format( "Unable to create or validate storage directories (check log)" );
                                }
                                m_storagePaths = std::nullopt;
                            }
                            ImGui::EndDisabledControls( textFocus || alreadyConfigured );
                        }
                    }
                    ImGui::PopItemWidth();

                    ImGui::Spacing();
                    ImGui::Spacing();

                    if ( m_configData != std::nullopt )
                    {
                        m_storagePaths = StoragePaths( m_configData.value(), GetAppCacheName() );

                        bool cacheAppValid      = fs::exists( m_storagePaths->cacheApp );
                        bool cacheCommonValid   = fs::exists( m_storagePaths->cacheCommon );
                        bool outputAppValid     = fs::exists( m_storagePaths->outputApp );

                        const auto showValidatedStoragePath = []( bool isValid, const fs::path& path, const char* context )
                        {
                            if ( isValid )
                                ImGui::TextColored( ImGui::GetStyleColorVec4( ImGuiCol_NavHighlight ), ICON_FA_CIRCLE_CHECK " %s", context );
                            else
                                ImGui::TextDisabled( ICON_FA_CIRCLE_STOP " %s [Not Found]", context );

                            ImGui::CompactTooltip( path.string().c_str() );
                        };

                        showValidatedStoragePath( cacheCommonValid, m_storagePaths->cacheCommon, "Shared Cache" );
                        showValidatedStoragePath( cacheAppValid,    m_storagePaths->cacheApp,    "Application Cache" );
                        showValidatedStoragePath( outputAppValid,   m_storagePaths->outputApp,   "Application Output" );

                        // hold up progress until we have storage paths validated
                        if ( !cacheAppValid     ||
                             !cacheCommonValid  ||
                             !outputAppValid )
                        {
                            m_storagePaths = std::nullopt;
                        }
                    }
                    else
                    {
                        ImGui::TextDisabled( ICON_FA_CIRCLE " ..." );
                        ImGui::TextDisabled( ICON_FA_CIRCLE " ..." );
                        ImGui::TextDisabled( ICON_FA_CIRCLE " ..." );
                    }

                    ImGui::Unindent( perBlockIndent );
                    ImGui::Spacing();
                    ImGui::Spacing();
                    ImGui::ColumnSeparatorBreak();
                }

                // storage not yet configured, don't continue with the rest until this is done
                if ( !m_storagePaths.has_value() )
                {
                    ImGui::Unindent( perColumnIndent );
                    ImGui::TableNextColumn();
                    ImGui::EndTable();
                }
                else
                {
                    // ---------------------------------------------------------------------------------------------------------
                    // check in on our configured access to Endlesss' services

                    static bool endlesssWorkOffline = false;
                    bool endlesssAuthExpired = false;
                    {
                        m_mdFrontEnd->titleText( "Endlesss Accesss" );
                        ImGui::Indent( perBlockIndent );

                        // used to enable/disable buttons if the jam cache update thread is working
                        const bool jamsAreUpdating = (asyncFetchState == endlesss::cache::Jams::AsyncFetchState::Working);

                        // endlesss unix times are in nano precision
                        const auto authExpiryUnixTime = endlesssAuth.expires / 1000;

                        uint32_t expireDays, expireHours, expireMins, expireSecs;
                        endlesssAuthExpired = !spacetime::datestampUnixExpiryFromNow( authExpiryUnixTime, expireDays, expireHours, expireMins, expireSecs );

                        {
                            const bool offerOfflineMode = ( supportsOfflineEndlesssMode() && endlesssAuthExpired );
                            ImGui::BeginDisabledControls( !offerOfflineMode );
                            ImGui::Checkbox( " Enable Offline Mode", &endlesssWorkOffline );
                            ImGui::EndDisabledControls( !offerOfflineMode );

                            // give context-aware tooltip help
                            if ( !supportsOfflineEndlesssMode() )
                                ImGui::CompactTooltip( "This app does not support Offline Mode" );
                            else if ( endlesssAuthExpired )
                                ImGui::CompactTooltip( "Enable to allow booting without valid Endlesss\nauthentication, which may limit some features" );
                            else 
                                ImGui::CompactTooltip( "Offline mode only available when signed-out of Endlesss" );

                            ImGui::Spacing();
                        }

                        if ( endlesssAuthExpired )
                        {
                            ImGui::TextColored(
                                colour::shades::errors.neutral(),
                                "Authentication expired, please sign in to refresh" );
                        }
                        else
                        {
                            ImGui::BeginDisabledControls( jamsAreUpdating );
                            if ( ImGui::IconButton( ICON_FA_RIGHT_FROM_BRACKET ) )
                            {
                                // zero out the disk copy and our local cache
                                config::endlesss::Auth emptyAuth;
                                config::save( *this, emptyAuth );
                                config::load( *this, endlesssAuth );
                            }
                            ImGui::CompactTooltip( "Log out" );
                            ImGui::EndDisabledControls( jamsAreUpdating );
                            ImGui::SameLine( 0, 12.0f );

                            ImGui::TextUnformatted( "Authentication expires in" );
                            ImGui::SameLine();
                            ImGui::TextColored(
                                colour::shades::callout.neutral(),
                                "%u day(s), %u hours", expireDays, expireHours );
                        }
                        
                        if ( endlesssAuthExpired )
                        {
                            if ( !endlesssWorkOffline )
                                progressionInhibitionReason = "Endlesss log-in has expired";
                        }

                        ImGui::Spacing();

                        // allow direct login to endlesss services; this is the same path as the website and app takes
                        if ( endlesssAuthExpired )
                        {
                            static char localLoginName[64];
                            static char localLoginPass[64];

                            ImGui::TextUnformatted( "User/Pass :" );
                            ImGui::SameLine();
                            ImGui::PushItemWidth( 140.0f );
                            ImGui::InputText( "##username", localLoginName, 60 );
                            ImGui::SameLine();
                            ImGui::InputText( "##password", localLoginPass, 60, ImGuiInputTextFlags_Password );
                            ImGui::PopItemWidth();
                            ImGui::SameLine();
                            if ( ImGui::Button( "Login" ) )
                            {
                                auto dataClient = std::make_unique< httplib::SSLClient >( "api.endlesss.fm" );

                                dataClient->set_ca_cert_path( m_configEndlesssAPI.certBundleRelative.c_str() );
                                dataClient->enable_server_certificate_verification( true );

                                // post user/pass to /auth/login as a json object
                                auto authBody = fmt::format( R"({{ "username" : "{}", "password" : "{}" }})", localLoginName, localLoginPass );
                                auto res = dataClient->Post(
                                    "/auth/login",
                                    authBody,
                                    "application/json" );

                                // the expected result is a json document that config::endlesss::Auth is a subset of
                                // so we deserialize direct into that
                                const auto memoryLoadResult = config::loadFromMemory( res->body, endlesssAuth );
                                if ( memoryLoadResult != config::LoadResult::Success )
                                {
                                    blog::error::cfg( "Unable to parse Endlesss credentials from json" );
                                    blog::error::cfg( res->body );
                                }
                                else
                                {
                                    // .. if parsing was successful, save the key bits we need back to disk for future use
                                    const auto authSaveResult = config::save( *this, endlesssAuth );
                                    if ( authSaveResult != config::SaveResult::Success )
                                    {
                                        blog::error::cfg( "Unable to re-save Endlesss authentication data" );
                                    }
                                }
                            }
                        }
                        else
                        {
                            // if not yet setup, configure the network for full authenticated access
                            if ( m_networkConfiguration->hasNoAccessSet() )
                            {
                                m_networkConfiguration->initWithAuthentication( m_configEndlesssAPI, endlesssAuth );
                            }

                            // stop closing the boot window if we're running background threads or we have no jam data
                            if ( !m_jamLibrary.hasJamData() )
                                progressionInhibitionReason = "No jam metadata found";

                            ImGui::BeginDisabledControls( jamsAreUpdating );
                            {
                                if ( ImGui::IconButton( ICON_FA_ARROWS_ROTATE ) )
                                {
                                    m_jamLibrary.asyncCacheRebuild(
                                        *m_networkConfiguration,
                                        endlesssAuth.sync_options,
                                        taskFlow,
                                        [&]( const endlesss::cache::Jams::AsyncFetchState state, const std::string& status )
                                        {
                                            asyncFetchState = state;
                                            asyncState      = status;

                                            if ( state == endlesss::cache::Jams::AsyncFetchState::Success )
                                                m_jamLibrary.save( *this );
                                        });
                                    m_taskExecutor.run( taskFlow );
                                }
                                ImGui::CompactTooltip( "Sync and update your list of jams\nand current join-ins" );
                            }
                            ImGui::EndDisabledControls( jamsAreUpdating );
                            ImGui::SameLine( 0, 12.0f );
                            {
                                ImGui::TextUnformatted( "Jam Cache :" );
                                ImGui::SameLine();
                                {
                                    ImGui::PushStyleColor( ImGuiCol_Text, colour::shades::callout.neutral() );

                                    switch ( asyncFetchState )
                                    {
                                    case endlesss::cache::Jams::AsyncFetchState::Success:
                                    case endlesss::cache::Jams::AsyncFetchState::None:
                                        ImGui::TextUnformatted( m_jamLibrary.getCacheFileState().c_str() );
                                        break;

                                    case endlesss::cache::Jams::AsyncFetchState::Working:
                                        ImGui::Spinner( "##syncing", true, ImGui::GetTextLineHeight() * 0.3f, 3.0f, 0.0f, ImGui::GetColorU32( ImGuiCol_Text ) );
                                        ImGui::SameLine( 0, 14.0f );
                                        ImGui::SetCursorPosY( ImGui::GetCursorPosY() - 3.0f );
                                        ImGui::TextUnformatted( asyncState.c_str() );
                                        progressionInhibitionReason = "Busy fetching metadata";
                                        break;

                                    case endlesss::cache::Jams::AsyncFetchState::Failed:
                                        ImGui::TextUnformatted( asyncState.c_str() );
                                        break;
                                    }

                                    ImGui::PopStyleColor();
                                }
                            }
                            // configuration of jam cache sync
                            {
                                bool syncOptionsChanged = false;

                                ImGui::Spacing();
                                ImGui::Indent( perBlockIndent * 3 );
                                ImGui::BeginDisabledControls( jamsAreUpdating );
                                syncOptionsChanged |= ImGui::Checkbox( " Fetch Collectibles", &endlesssAuth.sync_options.sync_collectibles );
                                                      ImGui::CompactTooltip( "Query and store all the 'collectible' jams\nDue to API issues, this may take a few minutes" );
                                syncOptionsChanged |= ImGui::Checkbox( " Fetch Jam State", &endlesssAuth.sync_options.sync_state );
                                                      ImGui::CompactTooltip( "For every jam we know about, query basic data like riff counts\nThis can take a little while but provides the most complete\nview of the jam metadata" );
                                ImGui::EndDisabledControls( jamsAreUpdating );
                                ImGui::Unindent( perBlockIndent * 3 );

                                if ( syncOptionsChanged )
                                    config::save( *this, endlesssAuth );
                            }
                        }


                        ImGui::Unindent( perBlockIndent );
                        ImGui::Spacing();
                        ImGui::Spacing();
                        ImGui::ColumnSeparatorBreak();
                    }


                    // ---------------------------------------------------------------------------------------------------------
                    {
                        ImGui::Unindent( perColumnIndent );
                        ImGui::TableNextColumn();
                        ImGui::Indent( perColumnIndent );
                    }


                    // ---------------------------------------------------------------------------------------------------------
                    // configure how we'll be making noise

                    {
                        m_mdFrontEnd->titleText( "Audio Engine" );
                        ImGui::Indent( perBlockIndent );

                        // clamp size of drop-downs
                        ImGui::PushItemWidth( 80.0f );
                        {
                            // choose a sample rate; changing causes device options to be reconsidered
                            if ( ImGui::ValueArrayComboBox( "Sample Rate :", "##smpr", cSampleRateLabels, cSampleRateValues, audioConfig.sampleRate, previewSampleRate, 16.0f ) )
                            {
                                fnUpdateDeviceQuery();
                            }

                            // note incompatibility with Opus streaming direct-to-discord; this has a fixed constraint of running at 48k
                            if ( audioConfig.sampleRate != 48000 )
                                ImGui::TextColored( colour::shades::callout.neutral(), ICON_FAB_DISCORD " Streaming requires 48kHz" );
                            else
                                ImGui::TextUnformatted( ICON_FAB_DISCORD " Streaming compatible" );

                            ImGui::Spacing();
                            ImGui::Spacing();

                            ImGui::ValueArrayComboBox( "Buffer Size :", "##buff", cBufferSizeLabels, cBufferSizeValues, audioConfig.bufferSize, previewBufferSize, 16.0f );
                        }
                        ImGui::PopItemWidth();

                        if ( ImGui::Checkbox( "Prefer Low Latency", &audioConfig.lowLatency ) )
                            fnUpdateDeviceQuery();

                        ImGui::Spacing();
                        ImGui::Spacing();
                        ImGui::Spacing();

                        ImGui::TextUnformatted( "Output Device :" );
                        ImGui::PushItemWidth( ImGui::GetContentRegionAvail().x - 20.0f );

                        std::string deviceNameLabel;
                        if ( ImGui::BeginCombo( "##audio_devices", (chosenDeviceIndex >= 0) ? adq.m_deviceNamesUnpacked[chosenDeviceIndex] : "-select audio device-", 0 ) )
                        {
                            for ( int32_t n = 0; n < (int32_t)adq.m_deviceNamesUnpacked.size(); n++ )
                            {
                                const bool is_selected = (chosenDeviceIndex == n);

                                deviceNameLabel = fmt::format( "[L={:.2f}] {}", adq.m_deviceLatencies[n], adq.m_deviceNamesUnpacked[n] );

                                if ( ImGui::Selectable( deviceNameLabel.c_str(), is_selected ) )
                                {
                                    chosenDeviceIndex = n;
                                    audioConfig.lastDevice = adq.m_deviceNames[chosenDeviceIndex];
                                }

                                if ( is_selected )
                                    ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }
                        ImGui::PopItemWidth();

                        ImGui::Unindent( perBlockIndent );
                        ImGui::Spacing();
                        ImGui::Spacing();
                        ImGui::ColumnSeparatorBreak();

                        if ( chosenDeviceIndex < 0 )
                            progressionInhibitionReason = "No audio device selected";
                    }

                    // ---------------------------------------------------------------------------------------------------------
                    // gotta go fast
                    {
                        m_mdFrontEnd->titleText( "Performance" );
                        ImGui::Indent( perBlockIndent );

                        const auto NicerIntEditPreamble = []( const char* title, const char* tooltip )
                        {
                            ImGui::TextDisabled( "[?]" );
                            ImGui::CompactTooltip( tooltip );
                            ImGui::SameLine();
                            ImGui::TextUnformatted( title );
                            ImGui::SameLine();
                            const auto panelRegionAvailable = ImGui::GetContentRegionAvail();
                            ImGui::Dummy( ImVec2( panelRegionAvailable.x - 200.0f, 0.0f ) );
                            ImGui::SameLine();
                        };

                        ImGui::PushItemWidth( 150.0f );
                        {
                            NicerIntEditPreamble(
                                "Stem Cache Memory Target",
                                "Stems are loaded and stored in memory for re-use between riffs.\nWhen the amount of memory in use hits this value, we run a pruning process to unload the oldest ones in the cache.\nIncrease this if you have lots of RAM and want to avoid re-loading\nstems off disk during longer sessions"
                            );
                            if ( ImGui::InputInt( " Mb##stem_cache_mem", &m_configPerf.stemCacheAutoPruneAtMemoryUsageMb, 256, 512 ) )
                            {
                                m_configPerf.clampLimits();
                            }

                            NicerIntEditPreamble(
                                "Riff Live Instance Pool Size",
                                "If possible, some riffs are kept alive in memory to speed-up transitions / avoid re-loading from disk.\nThis value controls how many we aim to limit that to.\nIncrease if you got RAM to burn."
                            );
                            ImGui::InputInt( "##riff_live_inst", &m_configPerf.liveRiffInstancePoolSize, 8, 16);
                        }
                        ImGui::PopItemWidth();


                        ImGui::Unindent( perBlockIndent );
                        ImGui::Spacing();
                        ImGui::Spacing();
                        ImGui::ColumnSeparatorBreak();
                    }

                    // ---------------------------------------------------------------------------------------------------------
                    // 
                    {
                        m_mdFrontEnd->titleText( "--" );
                        ImGui::Indent( perBlockIndent );


                        ImGui::Unindent( perBlockIndent );
                        ImGui::Spacing();
                        ImGui::Spacing();
                        ImGui::ColumnSeparatorBreak();
                    }


                    ImGui::Unindent( perColumnIndent );
                    ImGui::EndTable();

                } // phase-2 configuration blocks
            } // table

            // quit | continue button block at the base of the screen
            {
                const ImVec2 buttonSize( 260.0f, 36.0f );

                // align to the bottom of the dialogue
                const auto panelRegionAvailable = ImGui::GetContentRegionAvail();
                ImGui::Dummy( ImVec2( 0.0f, panelRegionAvailable.y - 64.0f ) );

                // centered
                ImGui::Dummy( ImVec2( (panelRegionAvailable.x * 0.5f) - buttonSize.x, 0.0f ) );


                // Quit is denied only when some async work is happening so resulting data saving isn't hurt by quitting half-way through
                const bool asyncWorkIsHappening = (asyncFetchState == endlesss::cache::Jams::AsyncFetchState::Working);
                ImGui::BeginDisabledControls( asyncWorkIsHappening );
                {
                    ImGui::SameLine();
                    if ( ImGui::Button( ICON_FA_CIRCLE_XMARK " Quit", buttonSize ) )
                    {
                        m_mdFrontEnd->requestQuit();
                    }
                }
                ImGui::EndDisabledControls( asyncWorkIsHappening );

                // Continue is available if something else isn't broken or working; tell the user why we can't continue if that's the case
                const bool bootProcessUnfinished = !progressionInhibitionReason.empty();
                ImGui::BeginDisabledControls( bootProcessUnfinished );
                {
                    ImGui::SameLine();
                    if ( ImGui::Button( bootProcessUnfinished ? progressionInhibitionReason.c_str() : ICON_FA_CIRCLE_CHECK " Accept & Continue", buttonSize ) )
                    {
                        ImGui::Spacing();
                        ImGui::TextUnformatted( "Please wait, loading session..." );

                        const auto audioInitStatus = m_mdAudio->initOutput( audioConfig, audioSpectrumConfig );
                        if ( audioInitStatus.ok() )
                            successfulBreakFromLoop = true;
                    }
                }
                ImGui::EndDisabledControls( bootProcessUnfinished );
            }

        } // config window block

        ImGui::End();

        // pending error message? transfer and pop the modal open on the next cycle round
        // must be done here, after ImGui::End for annoying Imgui ordering / hierarchy reasons
        if ( !popupErrorMessageToDisplay.empty() )
        {
            popupErrorMessage = popupErrorMessageToDisplay;
            ImGui::OpenPopup( popupErrorModalName );
            popupErrorMessageToDisplay.clear();
        }

        // dispatch the UI for rendering
        finishInterfaceLayoutAndRender();

        if ( successfulBreakFromLoop )
            break;
    }

    // if we passed over Endlesss authentication, create a net config structure that only knows how to talk to public
    // stuff (so we can still pull stems from the CDN on demand, for example)
    if ( m_networkConfiguration->hasNoAccessSet() )
    {
        m_networkConfiguration->initWithoutAuthentication( m_configEndlesssAPI );
    }

    // save any config data blocks
    {
        const auto audioSaveResult = config::save( *this, audioConfig );
        if ( audioSaveResult != config::SaveResult::Success )
        {
            blog::error::cfg( "Unable to save audio configuration" );
        }

        const auto perfSaveResult = config::save( *this, m_configPerf );
        if ( perfSaveResult != config::SaveResult::Success )
        {
            blog::error::cfg( "Unable to save performance configuration" );
        }
    }

    m_taskExecutor.wait_for_all();

    if ( m_mdFrontEnd->wasQuitRequested() )
        return 0;

    // boot stem cache now we have paths & audio configured
    const auto stemCacheStatus = m_stemCache.initialise( m_storagePaths->cacheCommon, m_mdAudio->getSampleRate() );
    if ( !stemCacheStatus.ok() )
    {
        blog::error::cfg( "Unable to initialise stem cache; {}", stemCacheStatus.ToString() );
        return -1;
    }
    m_stemCacheLastPruneCheck.restart();
    m_stemCachePruneTask.emplace( [this]() { m_stemCache.lockAndPrune( false ); } );


    // unplug status bar bits
    unregisterStatusBarBlock( sbbTimeStatusLeftID );
    unregisterStatusBarBlock( sbbTimeStatusRightID );

    // events
    {
        APP_EVENT_REGISTER( ExportRiff );
        APP_EVENT_REGISTER( StemDataAmalgamGenerated );
        APP_EVENT_REGISTER_SPECIFIC( MixerRiffChange, 16 * 4096 );

        m_eventListenerRiffExport = m_appEventBus->addListener( events::ExportRiff::ID, [this]( const base::IEvent& evt ) { onEvent_ExportRiff( evt ); } );
    }

    int appResult = EntrypointOuro();

    // unhook events
    {
        checkedCoreCall( "remove listener", [this] { return m_appEventBus->removeListener( m_eventListenerRiffExport ); } );
    }

    // wrap up any dangling async work before teardown
    ensureStemCacheChecksComplete();

    return appResult;
}

// ---------------------------------------------------------------------------------------------------------------------
void OuroApp::ensureStemCacheChecksComplete()
{
    if ( m_stemCachePruneFuture.has_value() )
        m_stemCachePruneFuture->wait();
    m_stemCachePruneFuture = std::nullopt;
}

// ---------------------------------------------------------------------------------------------------------------------
void OuroApp::maintainStemCacheAsync()
{
    static constexpr auto memoryReclaimPeriod = 30 * 1000;

    if ( m_stemCacheLastPruneCheck.deltaMs().count() > memoryReclaimPeriod )
    {
        const auto stemMemory          = m_stemCache.estimateMemoryUsageBytes();
        const auto stemMemoryTriggerMb = (std::size_t)m_configPerf.stemCacheAutoPruneAtMemoryUsageMb;

        if ( stemMemory >= stemMemoryTriggerMb * 1024 * 1024 )
        {
            ensureStemCacheChecksComplete();

            // the prune is usually measured in 10s of milliseconds, but we may as well 
            // toss it into the job queue; it locks the cache to do the work, worst case very briefly delaying 
            // async background loading
            m_stemCachePruneFuture = m_taskExecutor.run( m_stemCachePruneTask );
        }
        m_stemCacheLastPruneCheck.restart();
    }
}


// ---------------------------------------------------------------------------------------------------------------------
void OuroApp::onEvent_ExportRiff( const base::IEvent& eventRef )
{
    ABSL_ASSERT( eventRef.getID() == events::ExportRiff::ID );
    
    const events::ExportRiff* exportRiffEvent = dynamic_cast<const events::ExportRiff*>( &eventRef );
    ABSL_ASSERT( exportRiffEvent != nullptr );

    endlesss::toolkit::xp::RiffExportDestination destination(
        m_storagePaths.value(),
        m_configExportOutput.spec
    );

    const auto exportedFiles = endlesss::toolkit::xp::exportRiff(
        endlesss::toolkit::xp::RiffExportMode::Stems,
        destination,
        exportRiffEvent->m_adjustments,
        exportRiffEvent->m_riff );

    for ( const auto& exported : exportedFiles )
    {
        blog::core( " => {}", exported.string() );
    }
}

} // namespace app
