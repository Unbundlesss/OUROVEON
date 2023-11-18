//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#include "pch.h"
#include "ux/cache.migrate.h"
#include "app/imgui.ext.h"

#include "filesys/fsutil.h"
#include "xp/open.url.h"

#include "endlesss/cache.stems.h"
#include "endlesss/toolkit.warehouse.h"

namespace ux {

// ---------------------------------------------------------------------------------------------------------------------
struct CacheMigrationState
{
    using recursive_iterator = fs::recursive_directory_iterator;

    CacheMigrationState( const fs::path& cacheCommonRootPath )
        : m_cacheRoot( cacheCommonRootPath )
        , m_rootPathVersion1( cacheCommonRootPath / endlesss::cache::Stems::getCachePathRoot( endlesss::cache::Stems::CacheVersion::Version1 ) )
        , m_rootPathVersion2( cacheCommonRootPath / endlesss::cache::Stems::getCachePathRoot( endlesss::cache::Stems::CacheVersion::Version2 ) )
    {
    }

    fs::path            m_cacheRoot;
    fs::path            m_rootPathVersion1;
    fs::path            m_rootPathVersion2;
    recursive_iterator  m_iterator;

    std::vector< fs::path >         m_resolverOriginalFiles;
    endlesss::types::StemCouchIDs   m_resolverInputs;
    endlesss::types::JamCouchIDs    m_resolverOutputs;

    bool                m_running = false;
    uint32_t            m_filesExamined = 0;
    uint32_t            m_filesMigrated = 0;
};

// ---------------------------------------------------------------------------------------------------------------------
std::shared_ptr< CacheMigrationState > createCacheMigrationState( const fs::path& cacheCommonRootPath )
{
    return std::make_shared< CacheMigrationState >( cacheCommonRootPath );
}

// ---------------------------------------------------------------------------------------------------------------------
void modalCacheMigration( const char* title, endlesss::toolkit::Warehouse& warehouse, CacheMigrationState& state )
{
    const ImVec2 buttonSize( 240.0f, 32.0f );

    const ImVec2 configWindowSize( 600.0f, 320.0f );
    ImGui::SetNextWindowContentSize( configWindowSize );

    if ( ImGui::BeginPopupModal( title, nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize ) )
    {
        if ( !fs::exists( state.m_rootPathVersion1 ) )
        {
            ImGui::TextWrapped( "This tool is for migrating older OUROVEON stem caches but you do not appear to have one in your installation" );
        }
        else
        {
            ImGui::TextWrapped( "This is a tool to migrate your older OUROVEON stem cache to the latest format. This process involves identifying & copying the old stem files; it may take a moment to complete, depending on the size of your cache." );
            ImGui::Spacing();
            ImGui::TextWrapped( "Click [Run Migration] and wait for it to complete. You only need do this process once." );
            ImGui::Spacing();
            ImGui::SeparatorBreak();

            {
                ImGui::Scoped::ToggleButton tbl( state.m_running, true );
                if ( ImGui::Button( "Run Migration", buttonSize ) )
                {
                    state.m_running = !state.m_running;
                    state.m_filesExamined = 0;
                    state.m_iterator = CacheMigrationState::recursive_iterator( state.m_rootPathVersion1, std::filesystem::directory_options::skip_permission_denied );
                }
            }
            ImGui::Text( "Stems Examined : %u", state.m_filesExamined );
            ImGui::Text( "Stems Migrated : %u", state.m_filesMigrated );

            ImGui::Spacing();
            ImGui::SeparatorBreak();
            ImGui::TextWrapped( "Once complete, you can delete the old cache folder : " );
            if ( ImGui::Button( "Open..." ) )
            {
                xpOpenFolder( state.m_cacheRoot.string().c_str() );
            }
            ImGui::SameLine( 0, 12.0f );
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored( colour::shades::callout.neutral(), "%s", state.m_rootPathVersion1.string().c_str() );


            if ( state.m_running )
            {
                state.m_resolverOriginalFiles.clear();
                state.m_resolverInputs.clear();
                state.m_resolverOutputs.clear();

                int32_t runs = 8;
                try
                {
                    std::error_code osError;
                    auto i = fs::begin( state.m_iterator );
                    for ( ; i != fs::end( state.m_iterator ); i = i.increment( osError ) )
                    {
                        if ( osError )
                            break;

                        if ( i->is_directory() )
                            continue;

                        if ( runs <= 0 )
                            break;
                        runs--;

                        const std::string filename = i->path().stem().string();
                        if ( filename.size() > 10 &&
                            filename[0] == 's' &&
                            filename[1] == 't' &&
                            filename[2] == 'e' &&
                            filename[3] == 'm' )
                        {
                            const auto stemID = filename.substr( 5 );
                            state.m_resolverInputs.emplace_back( stemID );
                            state.m_resolverOriginalFiles.emplace_back( i->path() );

                            state.m_filesExamined++;
                        }
                    }
                    bool bHitEnd = i == fs::end( state.m_iterator );
                    if ( bHitEnd )
                        state.m_running = false;
                }
                catch ( std::exception& cEx )
                {
                    blog::error::cache( FMTX( "stopping cache walk on exception : {}" ), cEx.what() );
                    state.m_running = false;
                }

                if ( state.m_resolverInputs.empty() )
                {
                    state.m_running = false;
                }

                // ask warehouse about where we should be relocating these
                warehouse.batchFindJamIDForStem( state.m_resolverInputs, state.m_resolverOutputs );
                ABSL_ASSERT( state.m_resolverInputs.size() == state.m_resolverOutputs.size() );

                for ( auto idx = 0U; idx < state.m_resolverInputs.size(); idx++ )
                {
                    if ( state.m_resolverOutputs[idx].empty() )
                        continue;

                    const auto stemID = state.m_resolverInputs[idx];

                    const auto copyToPath = endlesss::cache::Stems::getCachePathForStemData(
                        state.m_rootPathVersion2,
                        state.m_resolverOutputs[idx],
                        stemID );
                    const auto copyToFile = copyToPath / stemID.value();

                    if ( !fs::exists( copyToFile ) )
                    {
                        //                         blog::cache( FMTX( "{} = {} >> {}" ),
                        //                             state.m_resolverInputs[idx],
                        //                             state.m_resolverOutputs[idx],
                        //                             copyToFile.string() );

                        absl::Status directoryBuildStatus = filesys::ensureDirectoryExists( copyToPath );
                        if ( directoryBuildStatus.ok() )
                        {
                            try
                            {
                                fs::copy_file( state.m_resolverOriginalFiles[idx], copyToFile );
                            }
                            catch ( fs::filesystem_error& fsE )
                            {
                                blog::error::cache( FMTX( "error while copying [{}] : {}" ), copyToFile.string(), fsE.what() );
                            }
                        }

                        state.m_filesMigrated++;
                    }
                }
            }
        }

        const auto panelRegionAvail = ImGui::GetContentRegionAvail();
        {
            const float alignButtonsToBase = panelRegionAvail.y - (buttonSize.y + 6.0f);
            ImGui::Dummy( ImVec2( 0, alignButtonsToBase ) );
        }

        {
            ImGui::Scoped::Disabled sd( state.m_running );
            if ( ImGui::Button( "Close", buttonSize ) )
            {
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndPopup();
    }
}

} // namespace ux
