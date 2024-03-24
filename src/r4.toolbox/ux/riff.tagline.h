//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#pragma once

#include "app/core.h"
#include "base/utils.h"
#include "endlesss/core.types.h"

namespace ux {

struct TagLineToolProvider
{
    virtual ~TagLineToolProvider() {}

    enum ToolID : uint8_t
    {
        RiffExport,         // emit export-to-disk events
        NavigateTo,         // emit navigate-to-riff events
        ShareToFeed,        // emit share-to-feed events

        BuiltItToolIdTop    // end ID for built-in tool IDs
    };

    virtual uint8_t getToolCount() const
    {
        return BuiltItToolIdTop;
    }

    virtual bool isToolEnabled( const ToolID id, const endlesss::live::Riff* currentRiffPtr ) const;

    virtual const char* getToolIcon( const ToolID id, std::string& tooltip ) const
    {
        switch ( id )
        {
            case RiffExport:    tooltip = "Export this riff to disk";               return ICON_FA_FLOPPY_DISK;
            case NavigateTo:    tooltip = "Try to navigate to this riff";           return ICON_FA_GRIP;
            case ShareToFeed:   tooltip = "Share this riff to your Endlesss feed";  return ICON_FA_RSS;

            default:
                ABSL_ASSERT( 0 );
                return "X";
                break;
        }
    }

    virtual bool checkToolKeyboardShortcut( const ToolID id ) const
    {
        if ( id == RiffExport )
            return ImGui::Shortcut( ImGuiModFlags_Ctrl, ImGuiKey_E, false );

        return false;
    }

    virtual void handleToolExecution( const ToolID id, base::EventBusClient& eventBusClient, endlesss::live::RiffPtr& currentRiffPtr );
};

struct TagLine
{
    TagLine( base::EventBusClient eventBus );
    ~TagLine();

    void imgui(
        endlesss::live::RiffPtr& currentRiffPtr,                        // required; the current riff. can be null
        const endlesss::toolkit::Warehouse* warehouseAccess = nullptr,  // optional; if !null, offer tagging options
        TagLineToolProvider* toolProvider = nullptr                     // optional; for changing how tools are enabled, handled
    );

private:

    struct State;
    std::unique_ptr< State >    m_state;

};

} // namespace ux
