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
#include "config/frontend.h"
#include "base/utils.h"
#include "app/imgui.ext.h"

namespace app {
namespace module {

// ---------------------------------------------------------------------------------------------------------------------
struct Frontend : public Module
{
    Frontend( const config::Frontend& feConfig, const char* name );
    virtual ~Frontend();

    // Module
    bool create( const app::Core& appCore ) override;
    void destroy() override;


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
    finline void requestQuit() { m_quitRequested = true; }
    finline bool wasQuitRequested() const { return m_quitRequested; }

    // as we do initialisation in the constructor, one must check afterwards that all is well
    finline bool wasBootSuccessful() const  { return m_GlfwWindow != nullptr; }

    finline int32_t getLargestTextureDim() const{ return m_largestTextureDimension; }


    // open a platform file picker, returns true on success
    bool showFilePicker( const char* spec, std::string& fileResult ) const;

    void titleText( const char* label );

    enum class FontChoice
    {
        FixedMain,      // fixed width used for the main UI
        FixedSmaller,
        FixedLarger,
        MediumTitle,    // medium font used for panel titles
        LargeLogo,      // big font used for the app logo pane
        Banner          // really big
    };

    finline ImFont* getFont( const FontChoice fc ) const
    {
        switch ( fc )
        {
            case FontChoice::FixedMain:     return m_fontFixed;
            case FontChoice::FixedSmaller:  return m_fixedSmaller;
            case FontChoice::FixedLarger:   return m_fixedLarger;
            case FontChoice::MediumTitle:   return m_fontMedium;
            case FontChoice::LargeLogo:     return m_fontLogo;
            case FontChoice::Banner:        return m_fontBanner;
            default:
                assert(0);
        }
        return nullptr;
    }

    static void reloadImguiLayoutFromDefault();

private:

    // push actual window attributes for borderless mode
    void applyBorderless();

    config::Frontend    m_feConfigCopy;
    std::string         m_appName;

    char*               m_imguiLayoutIni;

    GLFWwindow*         m_GlfwWindow;
    int32_t             m_largestTextureDimension;
    bool                m_isBorderless;

    ImFont*             m_fontFixed;
    ImFont*             m_fixedSmaller;
    ImFont*             m_fixedLarger;
    ImFont*             m_fontMedium;
    ImFont*             m_fontLogo;
    ImFont*             m_fontBanner;

    bool                m_quitRequested;
};

} // namespace module

using FrontendModule = std::unique_ptr<module::Frontend>;

} // namespace app

