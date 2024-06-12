//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#include "pch.h"
#include "ux/jams.importer.h"

#include "base/fio.h"
#include "base/text.h"
#include "base/text.transform.h"

#include "app/ouro.h"
#include "app/module.frontend.h"
#include "app/module.frontend.fonts.h"

#include "data/yamlutil.h"
#include "data/uuid.h"

#include "spacetime/chronicle.h"
#include "spacetime/moment.h"

#include "app/imgui.ext.h"

#include "xp/open.url.h"

#include "endlesss/cache.stems.h"
#include "endlesss/live.stem.h"
#include "endlesss/toolkit.warehouse.h"


using namespace std::chrono_literals;

namespace ux {

// ---------------------------------------------------------------------------------------------------------------------
struct JamImporterState
{
    JamImporterState() = delete;
    JamImporterState( const fs::path& importPath, base::EventBusClient eventBus )
        : m_eventBusClient( std::move( eventBus ) )
        , m_importPath( importPath )
    {
        APP_EVENT_BIND_TO( OperationComplete );
    }

    ~JamImporterState()
    {
        APP_EVENT_UNBIND( OperationComplete );
    }


    void imgui( app::OuroApp& ouroApplication );

    void refreshAvailableJams( endlesss::toolkit::Warehouse* warehousePtr );

    void launchImportTasks( app::OuroApp& ouroApplication );

    // watch for incoming completion events for 
    void event_OperationComplete( const events::OperationComplete* eventData )
    {
        const auto variant = base::Operations::variantFromID( eventData->m_id );
        if ( variant == endlesss::toolkit::Warehouse::OV_ImportAction )
        {
            for ( auto& item : m_importablesList )
            {
                if ( item->m_importOperationYAML == eventData->m_id )
                    item->m_importOperationYAML = base::OperationID::invalid();
                if ( item->m_importOperationTAR == eventData->m_id )
                    item->m_importOperationTAR = base::OperationID::invalid();
            }
        }
    }



    // captures a bundle of data for importing a jam, both the TAR stems and YAML metadata
    struct ImportableJamBundle
    {
        fs::path                    m_fileTAR;
        fs::path                    m_fileYAML;
        std::string                 m_jamNameFromYAML;          // parsed public jam name from YAML
        std::string                 m_jamNameToSortWith;        // lowercased m_jamNameFromYAML for alphabetical sorting 
        endlesss::types::JamCouchID m_jamCouchID;

        base::OperationID           m_importOperationYAML = base::OperationID::invalid();
        base::OperationID           m_importOperationTAR = base::OperationID::invalid();

        bool                        m_import = true;
        bool                        m_bYamlParseOk = false;
    };
    using ImportableJamBundlePtr    = std::shared_ptr< ImportableJamBundle >;
    using ImportableJamMap          = absl::flat_hash_map< std::string, ImportableJamBundlePtr >;
    using ImportableJamList         = std::vector< ImportableJamBundlePtr >;

    std::atomic_bool        m_importablesRefreshing = false;
    std::atomic_bool        m_importIsProcessing = false;
    ImportableJamList       m_importablesList;

    base::EventBusClient    m_eventBusClient;
    base::EventListenerID   m_eventLID_OperationComplete = base::EventListenerID::invalid();

    fs::path                m_importPath;
};

// ---------------------------------------------------------------------------------------------------------------------
void JamImporterState::refreshAvailableJams( endlesss::toolkit::Warehouse* warehousePtr )
{
    static constexpr std::string_view cOrxPre  = "orx.";
    static constexpr std::string_view cYamlExt = ".yaml";
    static constexpr std::string_view cTarExt  = ".tar";

    // local map of importable data we will populate and then eventually sort into a vector for display
    ImportableJamMap  importablesMap;

    auto fileIterator = fs::directory_iterator( m_importPath, std::filesystem::directory_options::skip_permission_denied );

    std::error_code osError;
    for ( auto fIt = fs::begin( fileIterator ); fIt != fs::end( fileIterator ); fIt = fIt.increment( osError ) )
    {
        if ( osError )
        {
            break;
        }

        // regular files for regular people!
        if ( !fIt->is_regular_file() )
            continue;

        const auto& entryFullFilename   = fIt->path();
        const auto entryExtensionLower  = base::StrToLwrExt( entryFullFilename.extension().string() );
        const auto entryStem            = base::StrToLwrExt( entryFullFilename.stem().string() );

        // does this start with "orx.", first sign we have something useful
        if ( entryStem.rfind( cOrxPre, 0 ) == 0 )
        {
            // an extension we expect?
            if ( entryExtensionLower == cYamlExt ||
                 entryExtensionLower == cTarExt )
            {
                // create or find an existing import bundle based on the filename stem, eg. `orx.after_easter__.band9b6acbcd75`
                ImportableJamBundlePtr importBundle;
                if ( importablesMap.contains( entryStem ) )
                {
                    importBundle = importablesMap[entryStem];
                }
                else
                {
                    importBundle = std::make_shared<ImportableJamBundle>();
                    importablesMap.emplace( entryStem, importBundle );
                }

                if ( entryExtensionLower == cYamlExt )
                {
                    ABSL_ASSERT( importBundle->m_fileYAML.empty() );
                    importBundle->m_fileYAML = entryFullFilename;

                    // we now check the YAML is approximately what we want to deal with ahead of time
                    // and fetch the readible jam name + jam couch ID out as part of that process
                    {
                        ryml::Tree yamlTree;
                        ryml::Parser yamlParser;

                        // load the text from disk or fail out immediately
                        auto loadStatus = base::readTextFile( importBundle->m_fileYAML );
                        if ( loadStatus.ok() )
                        {
                            std::string parseName = importBundle->m_fileYAML.filename().string();

                            // map it
                            auto nameView = ryml::csubstr( std::data( parseName ), std::size( parseName ) );
                            auto bufferView = ryml::substr( std::data( loadStatus.value() ), std::size( loadStatus.value() ) );

                            // parse it
                            yamlParser.parse_in_place( nameView, bufferView, &yamlTree );

                            // try to read out our principal keys
                            const auto jamNameReadStatus = data::parseYamlValue<std::string>( yamlTree, "jam_name" );
                            if ( !jamNameReadStatus.ok() )
                            {
                                blog::error::app( FMTX( "[JamImporter] unable to parse jam_name from source YAML `{}`" ), entryFullFilename.filename().string() );
                                importBundle->m_bYamlParseOk = false;
                                importBundle->m_import = false;
                            }
                            else
                            {
                                // got the jam name, couch ID next
                                importBundle->m_jamNameFromYAML = jamNameReadStatus.value();
                                importBundle->m_jamNameToSortWith = base::StrToLwrExt( importBundle->m_jamNameFromYAML );

                                const auto jamBandReadStatus = data::parseYamlValue<std::string>( yamlTree, "jam_couch_id" );
                                if ( !jamBandReadStatus.ok() )
                                {
                                    blog::error::app( FMTX( "[JamImporter] unable to parse jam_couch_id from source YAML `{}`" ), entryFullFilename.filename().string() );
                                    importBundle->m_bYamlParseOk = false;
                                    importBundle->m_import = false;
                                }
                                else
                                {
                                    // got what we wanted, consider this import valid for now
                                    importBundle->m_jamCouchID = endlesss::types::JamCouchID{ jamBandReadStatus.value() };
                                    importBundle->m_bYamlParseOk = true;

                                    blog::app( FMTX( "[JamImporter] successful YAML header check for `{}`" ), entryFullFilename.filename().string() );

                                    // if there are any references to that new jam ID already in the database, mark it as not to import
                                    // by default, just in case the user doesn't want to re-do it
                                    if ( warehousePtr->anyReferencesToJamFound( importBundle->m_jamCouchID ) )
                                    {
                                        importBundle->m_import = false;
                                    }
                                }
                            }
                        }
                        else
                        {
                            blog::error::app( FMTX( "[JamImporter] failed to load YAML text from `{}`" ), entryFullFilename.filename().string() );
                            importBundle->m_bYamlParseOk = false;
                            importBundle->m_import = false;
                        }
                    }

                }
                if ( entryExtensionLower == cTarExt )
                {
                    ABSL_ASSERT( importBundle->m_fileTAR.empty() );
                    importBundle->m_fileTAR = entryFullFilename;
                }
            }
            else
            {
                blog::app( FMTX( "[JamImporter] ignoring unrecognised orx extension `{}` from `{}`" ), entryExtensionLower, entryFullFilename.filename().string() );
            }
        }
        else
        {
            blog::app( FMTX( "[JamImporter] ignoring non orx file `{}`" ), entryFullFilename.filename().string() );
        }
    }

    m_importablesList.clear();
    m_importablesList.reserve( importablesMap.size() );

    // extract the objects from the hashmap, tun into a sorted list to display to the user
    for ( const auto& items : importablesMap )
    {
        m_importablesList.emplace_back( items.second );
    }
    std::sort( m_importablesList.begin(), m_importablesList.end(),
        [&]( const ImportableJamBundlePtr& lhs, const ImportableJamBundlePtr& rhs ) -> bool
        {
            return lhs->m_jamNameToSortWith < rhs->m_jamNameToSortWith;
        } );

    // async task done hooray
    m_importablesRefreshing = false;
}

// ---------------------------------------------------------------------------------------------------------------------
void JamImporterState::launchImportTasks( app::OuroApp& ouroApplication )
{
    m_importIsProcessing = true;

    const fs::path outputStemPath = ouroApplication.getStemCache().getCacheRootPath();
    blog::app( FMTX( "[JamImporter] targeting stem cache path `{}`" ), outputStemPath.string() );

    tf::Taskflow taskflow;

    for ( auto& item : m_importablesList )
    {
        if ( item->m_import && 
             item->m_bYamlParseOk &&
             item->m_fileTAR.empty() == false &&
             item->m_fileYAML.empty() == false )
        {
            // copy in the name in case it wasn't in our BNS etc
            ouroApplication.emplaceJamNameResolutionIntoQueue(
                item->m_jamCouchID,
                item->m_jamNameFromYAML );

            // kick the async tasks off
            item->m_importOperationYAML = ouroApplication.getWarehouseInstance()->requestJamDataImport( item->m_fileYAML );
            item->m_importOperationTAR  = ouroApplication.enqueueJamStemArchiveImportAsync( item->m_fileTAR, taskflow );

            std::this_thread::yield();

            // mark this done for import
            item->m_import = false;
        }
    }

    // launch task schedule, let it ride in the background
    ouroApplication.getTaskExecutor().run( std::move( taskflow ) );
}


// ---------------------------------------------------------------------------------------------------------------------
void JamImporterState::imgui( app::OuroApp& ouroApplication )
{
    const bool bShouldDisableUIWhileAsyncRunning = m_importIsProcessing || m_importablesRefreshing;

    const ImVec2 buttonSize( 240.0f, 32.0f );
    {
        ImGui::Scoped::Disabled sd( bShouldDisableUIWhileAsyncRunning );

        if ( ImGui::Button( ICON_FA_FOLDER_OPEN " Open Import Folder...  " ) )
        {
            xpOpenFolder( m_importPath.string().c_str() );
        }
        ImGui::SameLine();
        if ( ImGui::Button( ICON_FA_ARROWS_SPIN " Refresh Available Jams " ) )
        {
            m_importablesRefreshing = true;
            m_importablesList.clear();

            ouroApplication.getTaskExecutor().silent_async( [this, theWarehouse = ouroApplication.getWarehouseInstance() ]() { this->refreshAvailableJams( theWarehouse ); } );
        }
    }
    // show progress on the background inspection process if running
    if ( m_importablesRefreshing )
    {
        ImGui::SameLine(0, 20.0f);
        ImGui::Spinner( "##syncing", true, ImGui::GetTextLineHeight() * 0.4f, 3.0f, 1.5f, ImGui::GetColorU32( ImGuiCol_Text ) );
        ImGui::SameLine();
        ImGui::TextUnformatted( " Analysing data, please wait ..." );
    }

    bool anyProcessingHappening = false;
    uint32_t potentialExportsCount = 0;
    {
        ImGui::Scoped::Disabled sd( bShouldDisableUIWhileAsyncRunning );

        const ImVec2 tableViewDimensions = ImVec2(
            ImGui::GetContentRegionAvail().x,
            ImGui::GetContentRegionAvail().y - 140.0f );

        ImGui::Spacing();
        ImGui::Spacing();
        if ( ImGui::BeginTable( "##import_table", 5,
            ImGuiTableFlags_ScrollY |
            ImGuiTableFlags_Borders |
            ImGuiTableFlags_RowBg   |
            ImGuiTableFlags_NoSavedSettings, tableViewDimensions ) )
        {
            ImGui::TableSetupScrollFreeze( 0, 1 );  // top row always visible
            ImGui::TableSetupColumn( "Import",      ImGuiTableColumnFlags_WidthFixed,    64.0f );
            ImGui::TableSetupColumn( "Jam Name",    ImGuiTableColumnFlags_WidthStretch,   1.0f );
            ImGui::TableSetupColumn( "##Y",         ImGuiTableColumnFlags_WidthFixed,    32.0f );
            ImGui::TableSetupColumn( "##R",         ImGuiTableColumnFlags_WidthFixed,    32.0f );
            ImGui::TableSetupColumn( "Internal ID", ImGuiTableColumnFlags_WidthFixed,   140.0f );
            ImGui::TableHeadersRow();

            for ( size_t jamIdx = 0; jamIdx < m_importablesList.size(); jamIdx++ )
            {
                const bool bHasValidYaml    = m_importablesList[jamIdx]->m_bYamlParseOk;
                const bool bHasFileTAR      = m_importablesList[jamIdx]->m_fileTAR.empty() == false;
                const bool bHasFileYAML     = m_importablesList[jamIdx]->m_fileYAML.empty() == false;
                const bool bHasBothFiles    = bHasFileTAR && bHasFileYAML;

                ImGui::PushID( (int32_t)jamIdx );
                ImGui::TableNextColumn();

                {
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted( "  " );
                    ImGui::SameLine();

                    if ( bHasValidYaml && bHasBothFiles )
                    {
                        ImGui::Checkbox( "##import", &m_importablesList[jamIdx]->m_import );
                    }
                    else
                    {
                        ImGui::TextColored( colour::shades::callout.neutral(), ICON_FA_CIRCLE_EXCLAMATION );
                    }
                    ImGui::TableNextColumn();
                }
                {
                    ImGui::AlignTextToFramePadding();
                    if ( !bHasFileYAML )
                    {
                        ImGui::TextColored( colour::shades::errors.neutral(), "No matching .YAML file found" );
                        ImGui::CompactTooltip( m_importablesList[jamIdx]->m_fileTAR.string().c_str() );
                    }
                    else if ( !bHasValidYaml )
                    {
                        ImGui::TextColored( colour::shades::errors.neutral(), "YAML file failed to parse" );
                        ImGui::CompactTooltip( m_importablesList[jamIdx]->m_fileYAML.string().c_str() );
                    }
                    else if ( !bHasFileTAR )
                    {
                        ImGui::TextColored( colour::shades::errors.neutral(), "No matching .TAR file found" );
                        ImGui::CompactTooltip( m_importablesList[jamIdx]->m_fileYAML.string().c_str() );
                    }
                    else
                    {
                        ImGui::TextColored( colour::shades::sea_green.neutral(), 
                            m_importablesList[jamIdx]->m_jamNameFromYAML.c_str() );

                        if ( m_importablesList[jamIdx]->m_import )
                            potentialExportsCount++;
                    }
                    ImGui::TableNextColumn();
                }
                {
                    if ( m_importablesList[jamIdx]->m_importOperationYAML.isValid() )
                    {
                        ImGui::Spinner( "##yaml_working", true, ImGui::GetTextLineHeight() * 0.4f, 3.0f, 1.5f, ImGui::GetColorU32( ImGuiCol_Text ) );
                        anyProcessingHappening = true;
                    }

                    ImGui::TableNextColumn();

                    if ( m_importablesList[jamIdx]->m_importOperationTAR.isValid() )
                    {
                        ImGui::Spinner( "##tar_working", true, ImGui::GetTextLineHeight() * 0.4f, 3.0f, 1.5f, ImGui::GetColorU32( ImGuiCol_Text ) );
                        anyProcessingHappening = true;
                    }

                    ImGui::TableNextColumn();
                }
                {
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted( m_importablesList[jamIdx]->m_jamCouchID.c_str() );
                }

                ImGui::PopID();
            }

            ImGui::EndTable();
        }
        {
            if ( ImGui::Button( " Select All  " ) )
            {
                for ( auto& item : m_importablesList )
                    item->m_import = true;
            }
            ImGui::SameLine(0, 16.0f);
            if ( ImGui::Button( " Select None " ) )
            {
                for ( auto& item : m_importablesList )
                    item->m_import = false;
            }
        }
    }
    {
        ImGui::Scoped::Disabled sd( bShouldDisableUIWhileAsyncRunning );

        {
            ImGui::Scoped::Enabled se( potentialExportsCount > 0 );
            {
                ImGui::Scoped::ColourButton cb( colour::shades::sea_green, colour::shades::black, potentialExportsCount > 0 );
                if ( ImGui::Button( " Begin Import ", buttonSize ) )
                {
                    launchImportTasks( ouroApplication );
                }
            }
            ImGui::TextUnformatted( "NOTE: importing cannot be cancelled once begun, to ensure database consistency" );
        }

        if ( ImGui::BottomRightAlignedButton( "Close", buttonSize ) )
        {
            ouroApplication.getTaskExecutor().wait_for_all();
            ImGui::CloseCurrentPopup();
        }
    }
    {
        m_importIsProcessing = anyProcessingHappening;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
std::shared_ptr< JamImporterState > createJamImporterState( const fs::path& importPath, base::EventBusClient eventBus )
{
    return std::make_shared< JamImporterState >( importPath, std::move( eventBus ) );
}

// ---------------------------------------------------------------------------------------------------------------------
void modalJamImporterState(
    const char* title,
    JamImporterState& jamImporterState,
    app::OuroApp& ouroApplication )
{
    const ImVec2 configWindowSize = ImVec2( 830.0f, 600.0f );
    ImGui::SetNextWindowContentSize( configWindowSize );

    ImGui::PushStyleColor( ImGuiCol_PopupBg, ImGui::GetStyleColorVec4( ImGuiCol_ChildBg ) );

    if ( ImGui::BeginPopupModal( title, nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize ) )
    {
        jamImporterState.imgui( ouroApplication );

        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
}

} // namespace ux
