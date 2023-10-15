//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#include "pch.h"

#include "app/module.frontend.h"
#include "app/module.frontend.fonts.h"
#include "net/bond.riffpush.h"

namespace net {
namespace bond {

void modalRiffPushClientConnection(
    const char* title,
    const app::FrontendModule& frontend,
    net::bond::RiffPushClient& client )
{
    const ImVec2 configWindowSize = ImVec2( 450.0f, 260.0f );
    ImGui::SetNextWindowContentSize( configWindowSize );

    static absl::Status connectionAttemptStatus = absl::OkStatus();

    // TODO stick these into a config somewhere or whatever
    static std::string serverAddress    = "localhost";
    static std::string serverPort       = "9002";

    static constexpr float indentSize = 20.0f;

    if ( ImGui::BeginPopupModal( title, nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize ) )
    {
        ImGui::Indent( 8.0f );

        {
            ImGui::PushFont( frontend->getFont( app::module::Frontend::FontChoice::MediumTitle ) );
            {
                ImGui::Scoped::FloatTextRight floatRight( "BOND  " );
                ImGui::TextUnformatted( "BOND" );
            }
            ImGui::PopFont();
        }

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::TextUnformatted( ICON_FA_GLOBE " Server" );
        ImGui::PushItemWidth( 150.0f );
        ImGui::Indent( indentSize );
        ImGui::InputText( " Address", &serverAddress );
        ImGui::InputText( " Port", &serverPort, ImGuiInputTextFlags_CharsDecimal );
        ImGui::Unindent( indentSize );
        ImGui::PopItemWidth();
        ImGui::SeparatorBreak();

        ImGui::TextUnformatted( ICON_FA_CIRCLE_NODES " Connection" );
        ImGui::Indent( indentSize );

        const ImVec2 buttonSize( 200.0f, 0.0f );
        switch ( client.getState() )
        {
            case net::bond::Disconnected:
            {
                if ( ImGui::Button( "Connect", buttonSize ) )
                {
                    connectionAttemptStatus = client.connect( fmt::format( FMTX( "ws://{}:{}" ), serverAddress, serverPort ) );
                }
            }
            break;

            case net::bond::InFlux:
            {
                if ( ImGui::Button( "Abort", buttonSize ) )
                {
                    connectionAttemptStatus = client.disconnect();
                }
                ImGui::SameLine();

                ImGui::Spinner( "##working", true, ImGui::GetTextLineHeight() * 0.4f, 3.0f, 1.5f, ImGui::GetColorU32( ImGuiCol_Text ) );
                ImGui::SameLine();
                ImGui::TextUnformatted( " Working ..." );
            }
            break;

            case net::bond::Connected:
            {
                if ( ImGui::Button( "Disconnect", buttonSize ) )
                {
                    connectionAttemptStatus = client.disconnect();
                }
            }
            break;
        }

        ImGui::Unindent( indentSize );
        ImGui::SeparatorBreak();
        ImGui::Spacing();
        ImGui::Spacing();

        if ( !connectionAttemptStatus.ok() )
        {
            ImGui::TextUnformatted( connectionAttemptStatus.ToString() );
        }
        else
        {
            if ( client.getState() == net::bond::Connected )
            {
                ImGui::CenteredColouredText( ImGui::GetStyleColorVec4( ImGuiCol_NavHighlight ), "Connection Established" );
            }
            else
            {
                ImGui::TextUnformatted( " " );
            }
        }

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::SeparatorBreak();

        {
            ImGui::Dummy( ImVec2( ( ImGui::GetContentRegionAvail().x * 0.5f ) - ( buttonSize.x * 0.5f ), 0.0f ) );
            ImGui::SameLine();
            if ( ImGui::Button( "Close", buttonSize ) )
                ImGui::CloseCurrentPopup();
        }

        ImGui::Unindent( 8.0f );
        ImGui::EndPopup();
    }
}

} // namespace bond
} // namespace net

