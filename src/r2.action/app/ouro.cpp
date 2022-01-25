//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"

#include "base/spacetime.h"

#include "config/frontend.h"
#include "config/data.h"
#include "config/audio.h"

#include "app/ouro.h"
#include "app/module.audio.h"
#include "app/module.frontend.h"
#include "app/module.frontend.fonts.h"

#include "endlesss/all.h"

#include "gl/shader.h"

#include "platform_folders.h"


namespace app {

// ---------------------------------------------------------------------------------------------------------------------
// run a loop with a dedicated configuration page for choosing output device per session; once done, we jump to the app code
//
int OuroApp::EntrypointGUI()
{
    const std::array< uint32_t, 3 > cSampleRates { 44100, 48000, 88200 };

    // try and fetch last audio settings from the stash; doesn't really matter if we can't, it is just saved defaults
    // for the config screen
    config::Audio audioConfig;
    const auto audioLoadResult = config::load( *this, audioConfig );
    if ( audioLoadResult != config::LoadResult::Success )
    {
        blog::cfg( "No default audio settings found, using defaults" );
    }


    // load any saved configs
    config::endlesss::Auth endlesssAuth;
    const auto authLoadResult    = config::load( *this, endlesssAuth );
    const auto discordLoadResult = config::load( *this, m_configDiscord );


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

    // buffer for changing data storage path; #HDD TODO per platform limits
    constexpr size_t dataStoragePathBufferSize = 255;
    char dataStoragePathBuffer[dataStoragePathBufferSize];
    memset( dataStoragePathBuffer, 0, dataStoragePathBufferSize );
    if ( m_configData == std::nullopt )
    {
        // if we are starting with no config data, put a suggestion in
        const fs::path initalPathSuggestion = fs::path( sago::getDocumentsFolder() ) / "OUROVEON";
        strcpy_s( dataStoragePathBuffer, dataStoragePathBufferSize, initalPathSuggestion.string().c_str() );
    }
    else
    {
        // already got one!
        strcpy_s( dataStoragePathBuffer, dataStoragePathBufferSize, m_configData->storageRoot.c_str() );
    }

    // ::filesystem variant of the buffer and computed free-space data, udpated via a lambda that can be called
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


    // stash current TZ
    const auto timezoneLocal = date::current_zone();
    const auto timezoneUTC   = date::locate_zone( "Etc/UTC" );


    bool successfulBreakFromLoop = false;

    // configuration preflight
    while ( MainLoopBegin(false, false) )
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
        const ImVec2 configWindowSize = ImVec2( configWindowColumn1 + configWindowColumn2, 600.0f );
        const ImVec2 viewportWorkSize = ImGui::GetMainViewport()->GetCenter();

        ImGui::SetNextWindowPos( viewportWorkSize - ( configWindowSize * 0.5f ) );
        ImGui::SetNextWindowContentSize( configWindowSize );

        ImGui::Begin( "Framework Preflight", nullptr,
            ImGuiWindowFlags_NoResize           |
            ImGuiWindowFlags_NoSavedSettings    |
            ImGuiWindowFlags_NoCollapse );
        {
            constexpr float perColumnIndent = 5.0f;
            constexpr float perBlockIndent = 10.0f;

            if ( ImGui::BeginTable( "##databusknobs", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ColumnsWidthFixed | ImGuiTableFlags_NoSavedSettings ) )
            {
                ImGui::TableSetupColumn( "X", ImGuiTableColumnFlags_WidthFixed, configWindowColumn1 );
                ImGui::TableSetupColumn( "Y", ImGuiTableColumnFlags_WidthFixed, configWindowColumn2 );

                ImGui::TableNextColumn();
                ImGui::Indent( perColumnIndent );


                // ---------------------------------------------------------------------------------------------------------
                // hi everybody 

                {
                    ImGui::PushFont( m_mdFrontEnd->getFont( app::module::Frontend::FontChoice::LargeLogo ) );
                    ImGui::TextUnformatted( "OUROVEON ");
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
                                if ( !tryCreateAndValidateStoragePaths( m_storagePaths.value() ) )
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
                                ImGui::TextColored( ImGui::GetStyleColorVec4( ImGuiCol_NavHighlight ), ICON_FA_CHECK_CIRCLE " %s", context );
                            else
                                ImGui::TextDisabled( ICON_FA_STOP_CIRCLE " %s [Not Found]", context );

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
                    // where are you and what time is it?

                    {
                        m_mdFrontEnd->titleText( "Spacetime" );
                        ImGui::Indent( perBlockIndent );
                        {
                            auto t = date::make_zoned( timezoneUTC, std::chrono::system_clock::now() );
                            auto tf = date::format( base::spacetime::defaultDisplayTimeFormatTZ, t );
                            ImGui::Text( "SERVER | %-24s %s", timezoneUTC->name().c_str(), tf.c_str() );
                        }
                        {
                            auto t = date::make_zoned( timezoneLocal, std::chrono::system_clock::now() );
                            auto tf = date::format( base::spacetime::defaultDisplayTimeFormatTZ, t );
                            ImGui::Text( " LOCAL | %-24s %s", timezoneLocal->name().c_str(), tf.c_str() );
                        }

                        ImGui::Unindent( perBlockIndent );
                        ImGui::Spacing();
                        ImGui::Spacing();
                        ImGui::ColumnSeparatorBreak();
                    }


                    ImGui::Unindent( perColumnIndent );
                    ImGui::TableNextColumn();
                    ImGui::Indent( perColumnIndent );

                    // ---------------------------------------------------------------------------------------------------------
                    // configure how we'll be making noise

                    {
                        m_mdFrontEnd->titleText( "Audio Engine" );
                        ImGui::Indent( perBlockIndent );

                        // choose a sample rate; changing causes device options to be reconsidered
                        {
                            ImGui::TextUnformatted( "Desired Sample Rate :" );

                            std::string sampleRateLabel;
                            for ( const auto sampleRateOption : cSampleRates )
                            {
                                sampleRateLabel = fmt::format( "{}  ", sampleRateOption );
                                if ( ImGui::RadioButton( sampleRateLabel.c_str(), audioConfig.sampleRate == sampleRateOption ) )
                                {
                                    audioConfig.sampleRate = sampleRateOption;
                                    fnUpdateDeviceQuery();
                                } 
                                ImGui::SameLine();
                            }
                            ImGui::TextUnformatted( "" );
                        }
                        ImGui::Spacing();
                        {
                            if ( ImGui::Checkbox( "Prefer Low Latency", &audioConfig.lowLatency ) )
                                fnUpdateDeviceQuery();
                        }
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
                    // i love to chat and friends

                    {
                        m_mdFrontEnd->titleText( "Discord" );
                        ImGui::Indent( perBlockIndent );

                        ImGui::PushItemWidth( configWindowColumn1 * 0.75f );
                        {
                            ImGui::InputText( " Bot Token", &m_configDiscord.botToken, ImGuiInputTextFlags_Password );
                            ImGui::CompactTooltip( "from https://discord.com/developers/applications/<bot_id>/bot" );

                            // enable Developer Mode in your account settings, then you get Copy ID on right-click menus
                            ImGui::InputText( " Guild ID", &m_configDiscord.guildSID, ImGuiInputTextFlags_CharsDecimal );
                            ImGui::CompactTooltip( "Snowflake ID of the guild to connect with" );

                            ImGui::Spacing();
                            ImGui::Spacing();

                            if ( m_configDiscord.botToken.empty() ||
                                 m_configDiscord.guildSID.empty() )
                            {
                                ImGui::TextDisabled( "No credentials supplied" );
                            }
                            else
                            {
                                if ( audioConfig.sampleRate != 48000 )
                                    ImGui::TextColored( ImGui::GetErrorTextColour(), ICON_FA_EXCLAMATION_TRIANGLE " Discord audio streaming requires 48khz output" );
                                else
                                    ImGui::TextUnformatted( ICON_FA_CHECK_CIRCLE " Discord compatible audio output" );
                            }
                        }
                        ImGui::PopItemWidth();


                        ImGui::Unindent( perBlockIndent );
                        ImGui::Spacing();
                        ImGui::Spacing();
                        ImGui::ColumnSeparatorBreak();
                    }

                    // ---------------------------------------------------------------------------------------------------------
                    // check in on our configured access to Endlesss' services

                    bool endlesssAuthExpired = false;
                    {
                        m_mdFrontEnd->titleText( "Endlesss Accesss" );
                        ImGui::Indent( perBlockIndent );

                        const auto authExpiryUnixTime = endlesssAuth.expires / 1000;

                        uint32_t expireDays, expireHours, expireMins, expireSecs;
                        endlesssAuthExpired = !base::spacetime::datestampUnixExpiryFromNow( authExpiryUnixTime, expireDays, expireHours, expireMins, expireSecs );

                        if ( endlesssAuthExpired )
                        {
                            ImGui::TextColored(
                                ImVec4( 1.0f, 0.2f, 0.2f, 1.0f ),
                                "Authentication expired, please sign in to refresh" );
                        }
                        else
                        {
                            if ( ImGui::IconButton( ICON_FA_SIGN_OUT_ALT ) )
                            {
                                // zero out the disk copy and our local cache
                                config::endlesss::Auth emptyAuth;
                                config::save( *this, emptyAuth );
                                config::load( *this, endlesssAuth );
                            }
                            ImGui::CompactTooltip( "Log out" );
                            ImGui::SameLine( 0, 12.0f );

                            ImGui::TextUnformatted( "Authentication expires in" );
                            ImGui::SameLine();
                            ImGui::TextColored(
                                ImGui::GetStyleColorVec4( ImGuiCol_SliderGrabActive ),
                                "%u day(s), %u hours", expireDays, expireHours );
                        }

                        if ( endlesssAuthExpired )
                            progressionInhibitionReason = "Endlesss log-in has expired";

                        ImGui::Spacing();

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

                                dataClient->set_ca_cert_path( m_configEndlessAPI.certBundleRelative.c_str() );
                                dataClient->enable_server_certificate_verification( true );

                                auto authBody = fmt::format( R"({{ "username" : "{}", "password" : "{}" }})", localLoginName, localLoginPass );

                                // post the query, we expect a single document stream back
                                auto res = dataClient->Post(
                                    "/auth/login",
                                    authBody,
                                    "application/json" );

                                const auto memoryLoadResult = config::loadFromMemory( res->body, endlesssAuth );
                                if ( memoryLoadResult != config::LoadResult::Success )
                                {
                                    blog::error::cfg( "Unable to parse Endlesss credentials from json" );
                                    blog::error::cfg( res->body );
                                }
                                else
                                {
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
                            if ( !m_apiNetworkConfiguration.has_value() )
                            {
                                m_apiNetworkConfiguration = endlesss::api::NetConfiguration( m_configEndlessAPI, endlesssAuth, m_sharedDataPath );
                            }

                            const bool jamsAreUpdating = (asyncFetchState == endlesss::cache::Jams::AsyncFetchState::Working);

                            // stop closing the boot window if we're running background threads or we have no jam data
                            if ( !m_jamLibrary.hasJamData() )
                                progressionInhibitionReason = "No jam metadata found";

                            ImGui::BeginDisabledControls( jamsAreUpdating || endlesssAuthExpired );
                            {
                                if ( ImGui::IconButton( ICON_FA_SYNC ) )
                                {
                                    m_jamLibrary.asyncCacheRebuild( m_apiNetworkConfiguration.value(), taskFlow, [&]( const endlesss::cache::Jams::AsyncFetchState state, const std::string& status )
                                        {
                                            asyncFetchState = state;
                                            asyncState      = status;

                                            if ( state == endlesss::cache::Jams::AsyncFetchState::Success )
                                                m_jamLibrary.save( m_sharedConfigPath );
                                        });
                                    m_taskExecutor.run( taskFlow );
                                }
                                ImGui::CompactTooltip( "Sync and update your list of jams\nand current publics" );
                                ImGui::SameLine( 0, 12.0f );
                            }
                            ImGui::EndDisabledControls( jamsAreUpdating || endlesssAuthExpired );

                            {
                                switch ( asyncFetchState )
                                {
                                case endlesss::cache::Jams::AsyncFetchState::Success:
                                case endlesss::cache::Jams::AsyncFetchState::None:
                                    ImGui::TextUnformatted( m_jamLibrary.getCacheFileState().c_str() );
                                    break;

                                case endlesss::cache::Jams::AsyncFetchState::Working:
                                    ImGui::Spinner( "##syncing", true, ImGui::GetTextLineHeight() * 0.4f, 3.0f, ImGui::GetColorU32( ImGuiCol_Text ) );
                                    ImGui::SameLine( 0, 8.0f );
                                    ImGui::TextUnformatted( asyncState.c_str() );
                                    progressionInhibitionReason = "Busy fetching metadata";
                                    break;

                                case endlesss::cache::Jams::AsyncFetchState::Failed:
                                    ImGui::TextUnformatted( asyncState.c_str() );
                                    break;
                                }
                            }
                        }

                        ImGui::Unindent( perBlockIndent );
                        ImGui::Spacing();
                        ImGui::Spacing();
                    }

                    ImGui::Unindent( perColumnIndent );
                    ImGui::EndTable();
                } // phase-2 configuration blocks
            } // table


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
                    if ( ImGui::Button( ICON_FA_TIMES_CIRCLE " Quit", buttonSize ) )
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
                    if ( ImGui::Button( bootProcessUnfinished ? progressionInhibitionReason.c_str() : ICON_FA_CHECK_CIRCLE " Accept & Continue", buttonSize ) )
                    {
                        ImGui::Spacing();
                        ImGui::TextUnformatted( "Please wait, loading session..." );

                        if ( m_mdAudio->initOutput( audioConfig ) )
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


        MainLoopEnd( nullptr, nullptr );

        if ( successfulBreakFromLoop )
            break;
    }

    // save any config data blocks
    {
        const auto audioSaveResult = config::save( *this, audioConfig );
        if ( audioSaveResult != config::SaveResult::Success )
        {
            blog::error::cfg( "Unable to save audio configuration" );
        }

        const auto discordSaveResult = config::save( *this, m_configDiscord );
        if ( discordSaveResult != config::SaveResult::Success )
        {
            blog::error::cfg( "Unable to save discord configuration" );
        }
    }

    if ( m_mdFrontEnd->wasQuitRequested() )
        return 0;

    return EntrypointOuro();
}


// ---------------------------------------------------------------------------------------------------------------------
// standard modal UI to paw through the jams available to our account, picking one if you please
//
void OuroApp::modalJamBrowser(
    const char* title,
    const endlesss::cache::Jams& jamLibrary,
    const std::function<const endlesss::types::JamCouchID()>& getCurrentSelection,
    const std::function<void( const endlesss::types::JamCouchID& )>& changeSelection )
{
    const ImVec2 configWindowSize = ImVec2( 800.0f, 420.0f ); // blaze it
    ImGui::SetNextWindowContentSize( configWindowSize );

    ImGui::PushStyleColor( ImGuiCol_PopupBg, ImGui::GetStyleColorVec4( ImGuiCol_ChildBg ) );

    if ( ImGui::BeginPopupModal( title, nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize ) )
    {
        static ImGuiTextFilter jamNameFilter;
        static int32_t jamSortOption = endlesss::cache::Jams::eIterateSortDefault;

        bool shouldClosePopup = false;

        endlesss::types::JamCouchID selectedDbID;
        if ( getCurrentSelection )
            selectedDbID = getCurrentSelection();

        // wrap up the search function for filtering between public/private jams
        bool iterationPublic = true;
        const auto iterationFn = [&]( const endlesss::cache::Jams::Data& jamData ) 
        {
            if ( jamData.m_isPublic == iterationPublic )
            {
                if ( !jamNameFilter.PassFilter( jamData.m_displayName.c_str() ) )
                    return;

                bool isCurrentSelection = ( !selectedDbID.empty() && selectedDbID == jamData.m_jamCID );

                ImGui::TableNextColumn();
                if ( ImGui::ClickableText( jamData.m_displayName.c_str() ) )
                {
                    if ( changeSelection )
                        changeSelection( jamData.m_jamCID );

                    shouldClosePopup = true;
                }
            }
        };

        jamNameFilter.Draw( "##NameFilter", 200.0f );

        ImGui::SameLine( 0, 2.0f );
        if ( ImGui::Button( ICON_FA_TIMES_CIRCLE ) )
            jamNameFilter.Clear();

        ImGui::SameLine();
        ImGui::TextUnformatted( "Name Filter" );

        ImGui::SameLine( 0, 220.0f );
        ImGui::TextUnformatted( "Sort By " );

        ImGui::SameLine();
        ImGui::RadioButton( "Join Time ", &jamSortOption, endlesss::cache::Jams::eIterateSortDefault );
        ImGui::SameLine();
        ImGui::RadioButton( "Name", &jamSortOption, endlesss::cache::Jams::eIterateSortByName );

        const auto panelRegionAvailable = ImGui::GetContentRegionAvail();

        // one half table for publics ...
        ImGui::BeginChild( "##publics", ImVec2( 400.0f, panelRegionAvailable.y - 30.0f ) );
        if ( ImGui::BeginTable( "##publicJams", 1, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg ) )
        {
            ImGui::TableSetupColumn( "Public" );
            ImGui::TableHeadersRow();

            jamLibrary.iterateJams( iterationFn, jamSortOption );

            ImGui::EndTable();
        }
        ImGui::EndChild();
        
        // other half table for privates ...
        ImGui::SameLine(0.0f, 2.0f);

        ImGui::BeginChild( "##privates", ImVec2( 400.0f, panelRegionAvailable.y - 30.0f ) );
        if ( ImGui::BeginTable( "##privateJams", 1, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg ) )
        {
            ImGui::TableSetupColumn( "Private" );
            ImGui::TableHeadersRow();

            iterationPublic = false;
            jamLibrary.iterateJams( iterationFn, jamSortOption );

            ImGui::EndTable();
        }
        ImGui::EndChild();

        if ( ImGui::Button( " Close " ) || shouldClosePopup )
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
}

// ---------------------------------------------------------------------------------------------------------------------
void OuroApp::ImGui_AppHeader()
{
    ImGui::PushFont( m_mdFrontEnd->getFont( app::module::Frontend::FontChoice::LargeLogo ) );
    ImGui::TextUnformatted( GetAppName() );
    ImGui::PopFont();
}

// ---------------------------------------------------------------------------------------------------------------------
void OuroApp::ImGui_DiskRecorder( rec::IRecordable& recordable )
{
    const ImVec2 commonButtonSize = ImVec2( 26.0f, ImGui::GetFrameHeight() );

    static constexpr std::array< const char*, 5 > animProgress{
        "[_.~\"~] ",
        "[~_.~\"] ",
        "[\"~_.~] ",
        "[~\"~_.] ",
        "[.~\"~_] "
    };

    ImGui::PushID( recordable.getRecorderName() );

    const bool recodingInProgress = recordable.isRecording();

    // recorder may be in a state of flux; if so, display what it's telling us
    if ( recodingInProgress &&
         recordable.getFluxState() != nullptr )
    {
        {
            ImGui::Scoped::ToggleButton toggled( true, true );
            if ( ImGui::Button( ICON_FA_HOURGLASS_HALF, commonButtonSize ) )
            {
                recordable.stopRecording();
            }
        }

        ImGui::SameLine( 0.0f, 4.0f );
        ImGui::PushItemWidth( ImGui::GetContentRegionAvail().x );
        ImGui::TextUnformatted( recordable.getFluxState() );
        ImGui::PopItemWidth();
    }
    // otherwise it will be recording or not, so handle those default states
    else
    {
        if ( !recodingInProgress )
        {
            bool beginRecording = false;

            beginRecording |= ImGui::Button( ICON_FA_HDD, commonButtonSize );

            ImGui::SameLine( 0.0f, 4.0f );
            beginRecording |= ImGui::Button( recordable.getRecorderName(), ImVec2( ImGui::GetContentRegionAvail().x, 0.0f ) );

            if ( beginRecording )
            {
                const auto timestampPrefix = base::spacetime::createPrefixTimestampForFile();
                recordable.beginRecording( m_storagePaths->outputApp.string(), timestampPrefix );
            }
        }
        else
        {
            static int32_t animCycle = 0;
            animCycle++;

            const uint64_t bytesRecorded = recordable.getRecordingDataUsage();
            const auto humanisedBytes = base::humaniseByteSize( animProgress[(animCycle / 8) % 5], bytesRecorded );

            {
                ImGui::Scoped::ToggleButton toggled( true, true );
                if ( ImGui::Button( ICON_FA_HDD, commonButtonSize ) )
                {
                    recordable.stopRecording();
                }
            }
            ImGui::SameLine( 0.0f, 4.0f );
            ImGui::PushItemWidth( ImGui::GetContentRegionAvail().x );
            ImGui::TextUnformatted( humanisedBytes.c_str() );
            ImGui::PopItemWidth();
        }
    }

    ImGui::PopID();
}

} // namespace app