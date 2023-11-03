//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#include "pch.h"
#include "ux/jam.validate.h"

#include "spacetime/chronicle.h"
#include "spacetime/moment.h"

#include "app/imgui.ext.h"

#include "endlesss/cache.stems.h"
#include "endlesss/live.stem.h"
#include "endlesss/toolkit.warehouse.h"

using namespace std::chrono_literals;

namespace ux {

// ---------------------------------------------------------------------------------------------------------------------
struct JamValidateState
{
    JamValidateState() = delete;
    JamValidateState( const endlesss::types::JamCouchID& jamID )
        : m_jamCouchID( jamID )
    {}

    ~JamValidateState()
    {}

    enum class State
    {
        FetchID,
        Intro,
        Kick,
        Wait,
        Abandoned,
        Complete
    };

    void imgui(
        endlesss::toolkit::Warehouse& warehouse,
        const endlesss::api::NetConfiguration::Shared& netCfg,
        tf::Executor& taskExecutor );

    void imguiStats( bool isWorking )
    {
        ImGui::TextColored( colour::shades::toast.neutral(), "Jam ID : %s", m_jamExtendedID.c_str() );
        if ( isWorking )
        {
            ImGui::SameLine( 0, 0 );
            const float currentViewWidth = ImGui::GetContentRegionAvail().x;
            const float spinnerSize = ImGui::GetTextLineHeight() * 0.3f;
            ImGui::SameLine( 0, currentViewWidth - ( spinnerSize * 4.0f ) );
            ImGui::Spinner( "##thinking", true, spinnerSize, 3.0f, 0.0f, colour::shades::callout.lightU32() );
        }

        ImGui::Spacing();
        ImGui::SeparatorBreak();
        ImGui::TextColored( colour::shades::toast.light(), "[ %5i ] riffs examined", m_riffsExamined.load() );
        ImGui::TextColored( colour::shades::callout.light(), "[ %5i ] stems patched with new data", m_stemsPatched.load() );
        ImGui::TextColored( colour::shades::errors.light(), "[ %5i ] network interruptions", m_networkRetries.load() );
        ImGui::Spacing();
    }

    State                           m_state = State::FetchID;

    endlesss::types::JamCouchID     m_jamCouchID;
    std::string                     m_jamExtendedID;
    std::string                     m_resolverErrors;

    uint32_t                        m_workIndex = 0;
    uint32_t                        m_workRetries = 0;
    std::future< absl::Status >     m_workFutureStatus;

    std::atomic_uint32_t            m_riffsExamined = 0;
    std::atomic_uint32_t            m_stemsPatched = 0;
    std::atomic_uint32_t            m_networkRetries = 0;
};


// ---------------------------------------------------------------------------------------------------------------------
void JamValidateState::imgui(
    endlesss::toolkit::Warehouse& warehouse,
    const endlesss::api::NetConfiguration::Shared& netCfg,
    tf::Executor& taskExecutor )
{
    const ImVec2 buttonSize( 240.0f, 32.0f );

    switch ( m_state )
    {
        case State::FetchID:
        {
            endlesss::api::BandPermalinkMeta bandPermalink;
            if ( bandPermalink.fetch( *netCfg, m_jamCouchID ) )
            {
                if ( bandPermalink.errors.empty() )
                {
                    if ( bandPermalink.data.extractLongJamIDFromPath( m_jamExtendedID ) )
                    {
                        m_resolverErrors.clear();

                    }
                    else
                    {
                        m_resolverErrors = fmt::format( FMTX( "Could not find extended ID; {}" ), bandPermalink.data.path );
                    }
                }
                else
                {
                    m_resolverErrors = fmt::format( FMTX( "Failed to fetch link data; {}" ), bandPermalink.errors[0] );
                }
            }
            else
            {
                m_resolverErrors = "BandPermalinkMeta network request failure";
            }

            m_state = State::Intro;
        }
        break;

        case State::Intro:
        {
            ImGui::TextWrapped( "This tool will step through this jam and re-sync all riff data against the public Endlesss API, correcting any gaps or errors as we go\n" );
            ImGui::Spacing();
            ImGui::TextColored( colour::shades::errors.light(), "%s", m_resolverErrors.c_str() );
            ImGui::SeparatorBreak();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted( "Extended Jam ID : " );
            ImGui::SameLine();
            ImGui::InputText( "###extended-id", &m_jamExtendedID, ImGuiInputTextFlags_ReadOnly );
            ImGui::SeparatorBreak();
            ImGui::Spacing();

            ImGui::Scoped::Enabled se( m_resolverErrors.empty() );
            if ( ImGui::Button( "Begin Analysis", buttonSize ) )
            {
                m_state = State::Kick;
            }
        }
        break;

        case State::Kick:
        {
            imguiStats( true );

            m_workFutureStatus = taskExecutor.async( [&]()
                {
                    // give servers more of a break if we get more initial failures
                    const auto extraTimePadding = ((m_workRetries + 1) * 2) * 1s;
                    std::this_thread::sleep_for( extraTimePadding + 500ms );

                    endlesss::api::RiffStructureValidation riffStructure;
                    if ( riffStructure.fetch( *netCfg, m_jamExtendedID, m_workIndex, 100 ) )
                    {
                        if ( riffStructure.data.rifffs.empty() )
                        {
                            return absl::UnavailableError( "finished" );
                        }

                        for ( const auto& riff : riffStructure.data.rifffs )
                        {
                            endlesss::types::RiffCouchID riffCouchID( riff._id );

                            endlesss::types::RiffComplete riffFromDatabase;
                            if ( warehouse.fetchSingleRiffByID( riffCouchID, riffFromDatabase ) )
                            {
                                for ( int32_t stemIndex = 0; stemIndex < 8; stemIndex++ )
                                {
                                    const auto& stemEnabledOnServer = riff.state.playback[stemIndex].slot.current.on;

                                    const endlesss::types::Stem& databaseStem = riffFromDatabase.stems[stemIndex];
                                    const std::string& serverStemID = riff.state.playback[stemIndex].slot.current.currentLoop;

                                    if ( stemEnabledOnServer )
                                    {
                                        if ( databaseStem.couchID.empty() &&
                                            !serverStemID.empty() )
                                        {
                                            blog::app( FMTX( "[R:{}] found empty stem in database vs. populated on server : [{}]" ), riffCouchID, serverStemID );

                                            warehouse.patchRiffStemRecord( m_jamCouchID, riffCouchID, stemIndex, endlesss::types::StemCouchID( serverStemID ) );

                                            m_stemsPatched++;
                                        }
                                    }
                                }
                            }
                        }

                        m_riffsExamined += static_cast<uint32_t>( riffStructure.data.rifffs.size() );

                        return absl::OkStatus();
                    }
                    return absl::AbortedError( "Failed to retrieve riff structure for iteration" );
                });

            m_state = State::Wait;
        }
        break;

        case State::Wait:
        {
            imguiStats( true );

            ABSL_ASSERT( m_workFutureStatus.valid() );
            if ( m_workFutureStatus.wait_for( 14ms ) == std::future_status::ready )
            {
                const auto workStatus = m_workFutureStatus.get();

                if ( workStatus.ok() )
                {
                    m_workRetries = 0;
                    m_workIndex ++;
                    m_state = State::Kick;
                }
                else if ( absl::IsAborted( workStatus ) )
                {
                    if ( m_workRetries >= 5 )
                    {
                        m_state = State::Abandoned;
                    }
                    // try again!
                    else
                    {
                        m_networkRetries++;
                        m_workRetries++;
                        m_state = State::Kick;
                    }
                }
                else if ( absl::IsUnavailable( workStatus ) )
                {
                    m_state = State::Complete;
                }
                else
                {
                    ABSL_ASSERT( 0 );
                }
            }
        }
        break;

        case State::Abandoned:
        {
            imguiStats( false );

            ImGui::TextWrapped( "Process aborted" );
            ImGui::TextColored( colour::shades::errors.light(), "Failed during network fetch" );
        }
        break;

        case State::Complete:
        {
            imguiStats( false );

            ImGui::TextWrapped( "Process complete" );
        }
        break;
    }


    if ( ImGui::BottomRightAlignedButton( "Close", buttonSize ) )
    {
        taskExecutor.wait_for_all();
        ImGui::CloseCurrentPopup();
    }
}

// ---------------------------------------------------------------------------------------------------------------------
std::shared_ptr< JamValidateState > createModelJamValidateState( const endlesss::types::JamCouchID& jamID )
{
    return std::make_shared< JamValidateState >( jamID );
}

// ---------------------------------------------------------------------------------------------------------------------
void modalJamValidate(
    const char* title,
    JamValidateState& jamValidateState,
    endlesss::toolkit::Warehouse& warehouse,
    const endlesss::api::NetConfiguration::Shared& netCfg,
    tf::Executor& taskExecutor )
{
    const ImVec2 configWindowSize = ImVec2( 830.0f, 250.0f );
    ImGui::SetNextWindowContentSize( configWindowSize );

    ImGui::PushStyleColor( ImGuiCol_PopupBg, ImGui::GetStyleColorVec4( ImGuiCol_ChildBg ) );

    if ( ImGui::BeginPopupModal( title, nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize ) )
    {
        jamValidateState.imgui( warehouse, netCfg, taskExecutor );

        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
}

} // namespace ux
