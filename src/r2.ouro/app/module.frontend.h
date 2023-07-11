//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  Frontend controls initialistion of the rendering canvas and IMGUI 
//  instance, along with the update and display hooks
//

#pragma once
#include "app/module.h"
#include "app/display.scale.h"
#include "config/frontend.h"
#include "base/utils.h"
#include "base/construction.h"
#include "app/imgui.ext.h"

namespace app {
namespace module {

// ---------------------------------------------------------------------------------------------------------------------
struct Frontend final : public Module
{
    DECLARE_NO_COPY_NO_MOVE( Frontend );

    Frontend( const config::Frontend& feConfig, const char* name );
    virtual ~Frontend();

    // Module
    absl::Status create( const app::Core* appCore ) override;
    void destroy() override;
    virtual std::string getModuleName() const override { return "Frontend"; };


    // run the event loop, return if we are scheduled to quit or not
    bool appTick();

    // prep viewport, clear
    void appRenderBegin();

    // dispatch imgui
    void appRenderImguiDispatch();

    // ... and swap
    void appRenderFinalise();

    void toggleBorderless();


    // will return false from next appTick
    constexpr void requestQuit() { m_quitRequested = true; }
    ouro_nodiscard constexpr bool wasQuitRequested() const { return m_quitRequested; }



    void titleText( const char* label ) const;

    enum class FontChoice
    {
        FixedMain,      // fixed width used for the main UI
        FixedSmaller,
        MediumTitle,    // medium font used for panel titles
        LargeLogo,      // big font used for the app logo pane
    };

    inline ImFont* getFont( const FontChoice fc ) const
    {
        switch ( fc )
        {
            case FontChoice::FixedMain:     return m_fontFixed;
            case FontChoice::FixedSmaller:  return m_fixedSmaller;
            case FontChoice::MediumTitle:   return m_fontMedium;
            case FontChoice::LargeLogo:     return m_fontLogo;
            default:
                assert(0);
        }
        return nullptr;
    }

    void imguiRenderMarkdown( std::string_view markdownText ) const;

    void reloadImguiLayoutFromDefault() const;
    void resetWindowPositionAndSizeToDefault();

private:

    // for ImGui::Markdown
    static void MarkdownLinkHandler( const ImGui::MarkdownLinkCallbackData& data );
    static void MarkdownFormalCallback( const ImGui::MarkdownFormatInfo& markdownFormatInfo_, bool start_ );

    struct WindowGeometry
    {
        int32_t      m_positionX;
        int32_t      m_positionY;
        int32_t      m_width;
        int32_t      m_height;

        bool operator==( const WindowGeometry& ) const = default;
    };

    // fetch the current size and location of the app window
    ouro_nodiscard WindowGeometry getWindowGeometry() const;

    // take current state, stash into a config::Frontend and write it out to disk
    void updateAndSaveFrontendConfig();


    // push actual window attributes for borderless mode
    void applyBorderless() const;

    config::Frontend        m_feConfigCopy;
    std::string             m_appName;

    fs::path                m_imguiLayoutDefaultPath;
    char*                   m_imguiLayoutIni;

    GLFWwindow*             m_glfwWindow;
    DisplayScale            m_displayScale;
    bool                    m_isBorderless;

    WindowGeometry          m_currentWindowGeometry;
    int32_t                 m_windowGeometryChangedDelay;   // used to delay writing out changes to size/pos to gather up 
                                                            // sequential changes and avoid hammering the disk

    ImFont*                 m_fontFixed;
    ImFont*                 m_fixedSmaller;
    ImFont*                 m_fixedLarger;
    ImFont*                 m_fontMedium;
    ImFont*                 m_fontLogo;
    ImFont*                 m_fontBanner;

    ImGui::MarkdownConfig   m_markdownConfig;

    bool                    m_quitRequested;
};

} // namespace module

using FrontendModule = std::unique_ptr<module::Frontend>;

} // namespace app

