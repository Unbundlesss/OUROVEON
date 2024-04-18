//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#include "pch.h"
#include "ux/riff.feedshare.h"
#include "app/module.frontend.fonts.h"

#include "math/rng.h"

#include "spacetime/chronicle.h"
#include "spacetime/moment.h"

#include "app/imgui.ext.h"

#include "xp/open.url.h"

#include "endlesss/core.types.h"


using namespace std::chrono_literals;

namespace ux {

// ---------------------------------------------------------------------------------------------------------------------
struct RiffFeedShareState
{
    constexpr static std::size_t cMaximumShareNameLength = 20; // no idea what the real limit is

    RiffFeedShareState() = delete;
    RiffFeedShareState( const endlesss::types::RiffIdentity& identity )
        : m_riffIdentity( identity )
    {
        // set a default share name JIC, something semi random in case we are just hammering out quick shares
        math::RNG32 rng;
        strcpy( m_shareName, fmt::format( FMTX("ouro-share-{}{}"), identity.getRiffID().value().substr(0,2), rng.genInt32(1000, 9999) ).c_str() );
    }

    ~RiffFeedShareState()
    {}

    enum class State
    {
        Intro,
        Working,
        Results
    };

    enum ShareMode : int32_t
    {
        ToFeed,
        ToClubs,
    };


    void imgui(
        const endlesss::api::MyClubs::ChannelsList& clubsChans,
        const endlesss::api::NetConfiguration::Shared& netCfg,
        tf::Executor& taskExecutor );


    static inline int32_t           m_shareMode = ToFeed;       // static across modals
    State                           m_state = State::Intro;

    std::future< absl::Status >     m_workFutureStatus;
    absl::Status                    m_workResult;

    // ToFeed
    std::string                     m_shareUUIDResult;  // on successful share, filled with UUID
    endlesss::types::RiffIdentity   m_riffIdentity;
    char                            m_shareName[cMaximumShareNameLength];
    bool                            m_sharePrivate = true;

    // ToClubs
    static inline std::size_t       m_clubsChannelIndex = 0;    // static across modals; the club channels list never changes size anyway
};


// ---------------------------------------------------------------------------------------------------------------------
void RiffFeedShareState::imgui(
    const endlesss::api::MyClubs::ChannelsList& clubsChans,
    const endlesss::api::NetConfiguration::Shared& netCfg,
    tf::Executor& taskExecutor )
{
    const ImVec2 buttonSize( 240.0f, 32.0f );

    // JIC, reset this index if its beyond the incoming list size
    if ( m_clubsChannelIndex >= clubsChans.size() )
        m_clubsChannelIndex = 0;

    const auto ImGuiShareModeOptions = [&]( std::string_view description )
        {
            ImGui::Spacing();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted( "Share riff to" );
            ImGui::SameLine();
            ImGui::RadioButton( "Feed ", &m_shareMode, (int32_t)ToFeed );
            ImGui::SameLine();
            ImGui::RadioButton( "Clubs", &m_shareMode, (int32_t)ToClubs );

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::TextColored( colour::shades::toast.light(), "%s", description.data() );
            ImGui::SeparatorBreak();
            ImGui::Spacing();
            ImGui::Spacing();
        };

    // common "working" panel, just show a spinner while we wait, checking on the future status
    const auto ImGuiHandleStandardWorking = [this]()
        {
            {
                const float spinnerSize = 28.0f;
                const ImVec2 regionAvailableHalf = ImGui::GetContentRegionAvail() * 0.5f;

                ImGui::Dummy( { 0, (regionAvailableHalf.y * 0.35f) - (spinnerSize * 0.5f) } );
                ImGui::Dummy( { regionAvailableHalf.x - spinnerSize, 0 } );
                ImGui::SameLine( 0, 0 );
                ImGui::Spinner( "##thinking", true, spinnerSize, 6.0f, 0.0f, colour::shades::white.lightU32() );
                ImGui::Spacing();
                ImGui::CenteredText( "Requesting ..." );
            }

            ABSL_ASSERT( m_workFutureStatus.valid() );
            if ( m_workFutureStatus.wait_for( 14ms ) == std::future_status::ready )
            {
                m_workResult = m_workFutureStatus.get();
                m_state = State::Results;
            }
        };

    // standard display for the results view; offer links to load the result in a browser
    const auto ImGuiHandleStandardResult = [this]( std::string_view webUrl )
        {
            if ( m_workResult.ok() )
            {
                ImGui::TextColored( colour::shades::green.light(), ICON_FA_CIRCLE_CHECK " Riff Shared" );
                ImGui::SeparatorBreak();
                ImGui::Spacing();

                if ( !webUrl.empty() )
                {
                    if ( ImGui::Button( " " ICON_FA_LINK " Open in Web Browser " ) )
                    {
                        xpOpenURL( webUrl.data() );
                    }
                    ImGui::SameLine();
                    if ( ImGui::Button( " " ICON_FA_CLIPBOARD " Copy Link " ) )
                    {
                        ImGui::SetClipboardText( webUrl.data() );
                    }
                    ImGui::CompactTooltip( webUrl );
                }
            }
            else
            {
                ImGui::TextColored( colour::shades::errors.light(), ICON_FA_CIRCLE_EXCLAMATION " Sharing Failed" );
                ImGui::SeparatorBreak();
                ImGui::Spacing();

                ImGui::TextWrapped( "%s", m_workResult.ToString().c_str() );
            }
        };

    if ( m_shareMode == ToFeed )
    {
        switch ( m_state )
        {
            case State::Intro:
            {
                ImGuiShareModeOptions( "Share the selected riff to your Endlesss feed" );

                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted( "Name : " );
                ImGui::SameLine();
                ImGui::InputText( "###name-of-share", m_shareName, cMaximumShareNameLength );
                ImGui::Spacing();
                {
                    ImGui::Scoped::Enabled se( false ); // for now, force this to private only
                    ImGui::Checkbox( "Private Feed", &m_sharePrivate );
                }
                ImGui::SeparatorBreak();
                ImGui::Spacing();

                ImGui::Scoped::Disabled sd( strlen(m_shareName) == 0 );
                if ( ImGui::Button( "Share Riff", buttonSize ) )
                {
                    m_state = State::Working;
                    m_workFutureStatus = taskExecutor.async( [&]()
                        {
                            // let the 'working' status be on screen briefly so it's clear what's happening
                            std::this_thread::sleep_for( 1s );

                            endlesss::api::push::ShareRiffOnFeed shareRiffRequest;
                            shareRiffRequest.m_jamCouchID   = m_riffIdentity.getJamID();
                            shareRiffRequest.m_riffCouchID  = m_riffIdentity.getRiffID();
                            shareRiffRequest.m_private      = m_sharePrivate;
                            shareRiffRequest.m_shareName    = std::string( m_shareName );

                            return shareRiffRequest.action( *netCfg, m_shareUUIDResult );
                        });
                }
            }
            break;

            case State::Working:
            {
                ImGuiHandleStandardWorking();
            }
            break;

            case State::Results:
            {
                // navigate to the Endlesss riff web player
                const auto webPlayerURL = fmt::format( FMTX( "https://endlesss.fm/{}/?rifffId={}" ), netCfg->auth().user_id, m_shareUUIDResult );
                ImGuiHandleStandardResult( webPlayerURL );
            }
            break;
        }
    }
    else if ( m_shareMode == ToClubs )
    {
        switch ( m_state )
        {
            case State::Intro:
            {
                ImGuiShareModeOptions( "Share the selected riff to a chosen Clubs channel" );

                ImGui::SetNextItemWidth( -FLT_MIN );
                if ( ImGui::BeginCombo( "##club_channel", clubsChans[m_clubsChannelIndex].m_name.c_str() ) )
                {
                    for ( std::size_t optI = 0; optI < clubsChans.size(); optI++ )
                    {
                        const bool selected = optI == m_clubsChannelIndex;
                        if ( ImGui::Selectable( clubsChans[optI].m_name.c_str(), selected ) )
                        {
                            m_clubsChannelIndex = optI;
                        }
                        if ( selected )
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::Spacing();

                if ( ImGui::Button( "Share Riff", buttonSize ) )
                {
                    m_state = State::Working;
                    m_workFutureStatus = taskExecutor.async( [&]()
                        {
                            // let the 'working' status be on screen briefly so it's clear what's happening
                            std::this_thread::sleep_for( 1s );

                            endlesss::api::push::RiffCopy riffCopyRequest;
                            riffCopyRequest.m_jamFullID_CopyTo   = clubsChans[m_clubsChannelIndex].m_listenId;
                            riffCopyRequest.m_jamCouchID = m_riffIdentity.getJamID();
                            riffCopyRequest.m_riffCouchID = m_riffIdentity.getRiffID();

                            return riffCopyRequest.action( *netCfg, m_shareUUIDResult );
                        });
                }
            }
            break;

            case State::Working:
            {
                ImGuiHandleStandardWorking();
            }
            break;

            case State::Results:
            {
                // offer to open the club in question directly, assuming you're signed in
                const auto clubURL = fmt::format( FMTX( "https://endlesss.fm/clubs/?club={}" ), clubsChans[m_clubsChannelIndex].m_owningClubId );
                ImGuiHandleStandardResult( clubURL );
            }
            break;
        }
    }

    if ( ImGui::BottomRightAlignedButton( "Close", buttonSize ) )
    {
        taskExecutor.wait_for_all();
        ImGui::CloseCurrentPopup();
    }
}

// ---------------------------------------------------------------------------------------------------------------------
std::shared_ptr< RiffFeedShareState > createModelRiffFeedShareState( const endlesss::types::RiffIdentity& identity )
{
    return std::make_shared< RiffFeedShareState >( identity );
}

// ---------------------------------------------------------------------------------------------------------------------
void modalRiffFeedShare(
    const char* title,
    RiffFeedShareState& riffFeedShareState,
    const endlesss::api::MyClubs::ChannelsList& clubsChans,
    const endlesss::api::NetConfiguration::Shared& netCfg,
    tf::Executor& taskExecutor )
{
    const ImVec2 configWindowSize = ImVec2( 500.0f, 240.0f );
    ImGui::SetNextWindowContentSize( configWindowSize );

    ImGui::PushStyleColor( ImGuiCol_PopupBg, ImGui::GetStyleColorVec4( ImGuiCol_ChildBg ) );

    if ( ImGui::BeginPopupModal( title, nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize ) )
    {
        riffFeedShareState.imgui( clubsChans, netCfg, taskExecutor );

        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
}

} // namespace ux
