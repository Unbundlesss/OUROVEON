//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  Frontend controls initialistion of the rendering canvas and IMGUI 
//  instance, along with the update and display hooks
//

#include "pch.h"
#include "app/core.h"
#include "app/module.frontend.h"
#include "app/module.frontend.fonts.h"

#include "config/frontend.h"

// required for file picker
#include <commdlg.h>

// bits from imgui internals
#include "imgui_internal.h"

namespace app {
namespace module {

// ---------------------------------------------------------------------------------------------------------------------
void ApplyOuroveonImGuiStyle()
{
    ImGuiStyle& style                     = ImGui::GetStyle();
    style.WindowRounding                  = 1.0f;
    style.ChildRounding                   = 1.0f;
    style.FrameRounding                   = 4.0f;
    style.GrabRounding                    = 2.0f;
    style.PopupRounding                   = 1.0f;
    style.ScrollbarRounding               = 10.0f;
    style.TabRounding                     = 2.0f;

    ImVec4* colors                        = style.Colors;
    colors[ImGuiCol_Text]                  = ImVec4( 0.97f, 0.96f, 0.93f, 0.99f );
    colors[ImGuiCol_TextDisabled]          = ImVec4( 0.54f, 0.53f, 0.53f, 1.00f );
    colors[ImGuiCol_WindowBg]              = ImVec4( 0.08f, 0.09f, 0.10f, 1.00f );
    colors[ImGuiCol_ChildBg]               = ImVec4( 0.08f, 0.09f, 0.10f, 1.00f );
    colors[ImGuiCol_PopupBg]               = ImVec4( 0.02f, 0.02f, 0.02f, 1.00f );
    colors[ImGuiCol_Border]                = ImVec4( 0.08f, 0.09f, 0.10f, 1.00f );
    colors[ImGuiCol_BorderShadow]          = ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
    colors[ImGuiCol_FrameBg]               = ImVec4( 0.12f, 0.14f, 0.18f, 1.00f );
    colors[ImGuiCol_FrameBgHovered]        = ImVec4( 0.14f, 0.17f, 0.27f, 1.00f );
    colors[ImGuiCol_FrameBgActive]         = ImVec4( 0.16f, 0.21f, 0.32f, 1.00f );
    colors[ImGuiCol_TitleBg]               = ImVec4( 0.11f, 0.22f, 0.46f, 1.00f );
    colors[ImGuiCol_TitleBgActive]         = ImVec4( 0.13f, 0.23f, 0.45f, 1.00f );
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4( 0.12f, 0.22f, 0.43f, 1.00f );
    colors[ImGuiCol_MenuBarBg]             = ImVec4( 0.10f, 0.14f, 0.21f, 1.00f );
    colors[ImGuiCol_ScrollbarBg]           = ImVec4( 0.11f, 0.13f, 0.18f, 0.71f );
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4( 0.40f, 0.46f, 0.53f, 0.39f );
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4( 0.40f, 0.46f, 0.53f, 0.63f );
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4( 0.97f, 0.48f, 0.02f, 1.00f );
    colors[ImGuiCol_CheckMark]             = ImVec4( 0.97f, 0.48f, 0.02f, 1.00f );
    colors[ImGuiCol_SliderGrab]            = ImVec4( 0.40f, 0.46f, 0.53f, 1.00f );
    colors[ImGuiCol_SliderGrabActive]      = ImVec4( 0.97f, 0.48f, 0.02f, 1.00f );
    colors[ImGuiCol_Button]                = ImVec4( 0.35f, 0.44f, 0.55f, 0.31f );
    colors[ImGuiCol_ButtonHovered]         = ImVec4( 0.47f, 0.56f, 0.66f, 0.45f );
    colors[ImGuiCol_ButtonActive]          = ImVec4( 0.97f, 0.48f, 0.02f, 0.86f );
    colors[ImGuiCol_Header]                = ImVec4( 0.49f, 0.06f, 0.02f, 0.94f );
    colors[ImGuiCol_HeaderHovered]         = ImVec4( 0.50f, 0.07f, 0.03f, 1.00f );
    colors[ImGuiCol_HeaderActive]          = ImVec4( 0.53f, 0.07f, 0.03f, 1.00f );
    colors[ImGuiCol_Separator]             = ImVec4( 0.04f, 0.04f, 0.07f, 0.93f );
    colors[ImGuiCol_SeparatorHovered]      = ImVec4( 0.39f, 0.59f, 0.85f, 0.58f );
    colors[ImGuiCol_SeparatorActive]       = ImVec4( 0.39f, 0.59f, 0.85f, 1.00f );
    colors[ImGuiCol_ResizeGrip]            = ImVec4( 0.25f, 0.44f, 0.82f, 1.00f );
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4( 0.33f, 0.51f, 0.88f, 1.00f );
    colors[ImGuiCol_ResizeGripActive]      = ImVec4( 0.23f, 0.60f, 0.88f, 1.00f );
    colors[ImGuiCol_Tab]                   = ImVec4( 0.08f, 0.19f, 0.42f, 1.00f );
    colors[ImGuiCol_TabHovered]            = ImVec4( 0.15f, 0.27f, 0.53f, 1.00f );
    colors[ImGuiCol_TabActive]             = ImVec4( 0.09f, 0.29f, 0.72f, 1.00f );
    colors[ImGuiCol_TabUnfocused]          = ImVec4( 0.17f, 0.26f, 0.45f, 1.00f );
    colors[ImGuiCol_TabUnfocusedActive]    = ImVec4( 0.09f, 0.19f, 0.40f, 1.00f );
    colors[ImGuiCol_DockingPreview]        = ImVec4( 0.14f, 0.51f, 0.85f, 0.80f );
    colors[ImGuiCol_DockingEmptyBg]        = ImVec4( 0.04f, 0.04f, 0.05f, 1.00f );
    colors[ImGuiCol_PlotLines]             = ImVec4( 0.61f, 0.61f, 0.61f, 1.00f );
    colors[ImGuiCol_PlotLinesHovered]      = ImVec4( 0.56f, 0.16f, 0.04f, 1.00f );
    colors[ImGuiCol_PlotHistogram]         = ImVec4( 0.00f, 0.66f, 0.18f, 1.00f );
    colors[ImGuiCol_PlotHistogramHovered]  = ImVec4( 0.63f, 0.73f, 0.01f, 1.00f );
    colors[ImGuiCol_TableHeaderBg]         = ImVec4( 0.21f, 0.40f, 0.84f, 0.10f );
    colors[ImGuiCol_TableBorderStrong]     = ImVec4( 0.00f, 0.00f, 0.00f, 0.82f );
    colors[ImGuiCol_TableBorderLight]      = ImVec4( 0.00f, 0.00f, 0.00f, 0.50f );
    colors[ImGuiCol_TableRowBg]            = ImVec4( 0.61f, 0.74f, 1.00f, 0.04f );
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4( 0.61f, 0.74f, 1.00f, 0.07f );
    colors[ImGuiCol_TextSelectedBg]        = ImVec4( 0.21f, 0.31f, 0.48f, 0.70f );
    colors[ImGuiCol_DragDropTarget]        = ImVec4( 1.00f, 1.00f, 0.00f, 0.90f );
    colors[ImGuiCol_NavHighlight]          = ImVec4( 0.91f, 0.44f, 0.04f, 1.00f );
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4( 1.00f, 1.00f, 1.00f, 0.70f );
    colors[ImGuiCol_NavWindowingDimBg]     = ImVec4( 0.80f, 0.80f, 0.80f, 0.20f );
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4( 0.80f, 0.80f, 0.80f, 0.35f );
}

static constexpr char const* cImGuiFontFixed    = "../../shared/fonts/FiraCode-Regular.ttf";
static constexpr char const* cImGuiFontAwesome  = "../../shared/fonts/" FONT_ICON_FILE_NAME_FAS;
static constexpr char const* cImGuiFontTitle    = "../../shared/fonts/stentiga.ttf";
static constexpr char const* cImGuiFontMedium   = "../../shared/fonts/Oswald-Light.ttf";
static constexpr char const* cImGuiFontBanner   = "../../shared/fonts/JosefinSans-Regular.ttf";


// ---------------------------------------------------------------------------------------------------------------------
Frontend::Frontend( const config::Frontend& feConfig, const char* name )
    : m_feConfigCopy( feConfig )
    , m_appName( name )
    , m_SDLWindow( nullptr )
    , m_hwnd( nullptr )
    , m_largestTextureDimension( 0 )
    , m_fontFixed( nullptr )
    , m_fontMedium( nullptr )
    , m_fontLogo( nullptr )
    , m_quitRequested( false )
{
}

// ---------------------------------------------------------------------------------------------------------------------
Frontend::~Frontend()
{
    if ( m_hwnd != nullptr )
        destroy();
}

// ---------------------------------------------------------------------------------------------------------------------
bool Frontend::create( const app::Core& appCore )
{
    // preflight checks on the font data
    if ( !fs::exists( cImGuiFontFixed )   ||
         !fs::exists( cImGuiFontAwesome ) ||
         !fs::exists( cImGuiFontTitle )   ||
         !fs::exists( cImGuiFontMedium )
        )
    {
        blog::error::core( "Unable to find required font files, [{}] for example", cImGuiFontFixed );
        return false;
    }

    // begin to configure SDL 
    blog::core( "loading SDL ..." );
    if ( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER ) != 0 )
    {
        blog::error::core( "InitialiseSDL failed, error: %s", SDL_GetError() );
        return false;
    }

    // .. and then GL
    const char* glsl_version = "#version 400";
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, 0 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 4 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 0 );

    SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
    SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 24 );
    SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, 8 );

    {
        SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 1 );
        SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, 2 );
    }

    blog::core( "creating main window" );
    m_SDLWindow = SDL_CreateWindow(
        fmt::format( "OUROVEON {} // ishani.org 2022", m_appName ).c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        m_feConfigCopy.appWidth,
        m_feConfigCopy.appHeight,
        (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI)
    );
    m_isBorderless = true;

    blog::core( "creating main GL context" );
    m_GLContext = SDL_GL_CreateContext( m_SDLWindow );
    SDL_GL_MakeCurrent( m_SDLWindow, m_GLContext );
    SDL_GL_SetSwapInterval( 1 ); // +vsync

    blog::core( "loading GLAD ..." );
    int gladErr = gladLoadGL();
    if ( gladErr == 0 )
    {
        blog::error::core( "OpenGL loader failed, {}", gladErr );
        return false;
    }

    blog::core( "bound OpenGL {}.{} | GLSL {}", GLVersion.major, GLVersion.minor, glGetString( GL_SHADING_LANGUAGE_VERSION ) );

    glGetIntegerv( GL_MAX_TEXTURE_SIZE, (GLint*)&m_largestTextureDimension );
    blog::core( "GL_MAX_TEXTURE_SIZE = {}", m_largestTextureDimension );

    {
        blog::core( "initialising ImGui {}", IMGUI_VERSION );

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImPlot::CreateContext();
        imnodes::Initialize();

        // install styles
        ImGui::StyleColorsDark();
        ApplyOuroveonImGuiStyle();

        // configure rendering with SDL/GL
        ImGui_ImplSDL2_InitForOpenGL( m_SDLWindow, m_GLContext );
        ImGui_ImplOpenGL3_Init( glsl_version );


        ImGuiIO& io = ImGui::GetIO();

        // configure where imgui will stash layout persistence
        {
            static constexpr size_t layoutPathMax = 256;

            m_imguiLayoutIni = new char[layoutPathMax];
            const auto layoutStore = ( appCore.getAppConfigPath() / "layout.current.ini" ).string();
            strcpy_s( m_imguiLayoutIni, layoutPathMax, layoutStore.c_str() );

            // if the current layout ini doesn't exist, manually pull the default one
            if ( !fs::exists( layoutStore ) )
            {
                reloadImguiLayoutFromDefault();
            }

            io.IniFilename                        = m_imguiLayoutIni;
        }
        // populate ImGui font selection
        {
            io.ConfigFlags                       |= ImGuiConfigFlags_DockingEnable;
            io.ConfigWindowsMoveFromTitleBarOnly  = true;
            io.ConfigDockingWithShift             = true;

            m_fontFixed = io.Fonts->AddFontFromFileTTF( cImGuiFontFixed, 16.0f );

            // embed FontAwesome glyphs into default font so we can use them without switching
            {
                static const ImWchar faIconRange[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
                ImFontConfig faIconConfig;
                faIconConfig.MergeMode          = true;
                faIconConfig.PixelSnapH         = true;
                faIconConfig.GlyphOffset        = ImVec2( 0.0f, 2.0f );
                faIconConfig.GlyphExtraSpacing  = ImVec2( 1.0f, 0.0f );
                io.Fonts->AddFontFromFileTTF( cImGuiFontAwesome, 15.0f, &faIconConfig, faIconRange );
            }
            m_fixedSmaller = io.Fonts->AddFontDefault();
            m_fixedLarger  = io.Fonts->AddFontFromFileTTF( cImGuiFontFixed, 24.0f );
            m_fontLogo     = io.Fonts->AddFontFromFileTTF( cImGuiFontTitle, 50.0f );
            m_fontMedium   = io.Fonts->AddFontFromFileTTF( cImGuiFontMedium, 50.0f );
            m_fontBanner   = io.Fonts->AddFontFromFileTTF( cImGuiFontBanner, 40.0f );

            ImGuiFreeType::BuildFontAtlas( io.Fonts, ImGuiFreeType::LightHinting );
        }
    }

    // snag the windows HWND for our main window
    SDL_SysWMinfo wmInfo;
    SDL_VERSION( &wmInfo.version );
    SDL_GetWindowWMInfo( m_SDLWindow, &wmInfo );
    m_hwnd = wmInfo.info.win.window;

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
void Frontend::destroy()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();

    imnodes::Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext( m_GLContext );
    SDL_DestroyWindow( m_SDLWindow );
    SDL_Quit();

    m_SDLWindow = nullptr;
    m_hwnd = nullptr;
}

// ---------------------------------------------------------------------------------------------------------------------
bool Frontend::appTick()
{
    bool shouldQuit = m_quitRequested;

    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
    // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
    SDL_Event event;
    while ( SDL_PollEvent( &event ) )
    {
        ImGui_ImplSDL2_ProcessEvent( &event );

        if ( event.type == SDL_QUIT )
            shouldQuit = true;

        if ( event.type             == SDL_WINDOWEVENT &&
             event.window.event     == SDL_WINDOWEVENT_CLOSE &&
             event.window.windowID  == SDL_GetWindowID( m_SDLWindow ) )
            shouldQuit = true;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame( m_SDLWindow );
    ImGui::NewFrame();

    ImGui::Scoped::TickPulses();

    return shouldQuit;
}

// ---------------------------------------------------------------------------------------------------------------------
void Frontend::appRenderBegin()
{
    ImGuiIO& io = ImGui::GetIO();

    ImGui::Render();

    glViewport( 0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y );

    glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
    glClear( GL_COLOR_BUFFER_BIT );
}

// ---------------------------------------------------------------------------------------------------------------------
void Frontend::appRenderImguiDispatch()
{
    ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );
}

// ---------------------------------------------------------------------------------------------------------------------
void Frontend::appRenderFinalise()
{
    SDL_GL_SwapWindow( m_SDLWindow );
}

// ---------------------------------------------------------------------------------------------------------------------
void Frontend::toggleBorderless()
{
    m_isBorderless = !m_isBorderless;
    SDL_SetWindowBordered( m_SDLWindow, m_isBorderless ? SDL_FALSE : SDL_TRUE );
    SDL_SetWindowResizable( m_SDLWindow, m_isBorderless ? SDL_FALSE : SDL_TRUE );
}

// ---------------------------------------------------------------------------------------------------------------------
bool Frontend::showFilePicker( const char* spec, std::string& fileResult ) const
{
    char filename[MAX_PATH];

    OPENFILENAMEA ofn;
    ZeroMemory( &filename, sizeof( filename ) );
    ZeroMemory( &ofn, sizeof( ofn ) );
    ofn.lStructSize = sizeof( ofn );
    ofn.hwndOwner   = getHWND();
    ofn.lpstrFilter = spec;
    ofn.lpstrFile   = filename;
    ofn.nMaxFile    = MAX_PATH;
    ofn.lpstrTitle  = "Select file";
    ofn.Flags       = OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST;

    if ( GetOpenFileNameA( &ofn ) )
    {
        fileResult = filename;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------------------------------------------------
void Frontend::titleText( const char* label )
{
    ImGui::PushFont( getFont( app::module::Frontend::FontChoice::MediumTitle ) );
    ImGui::TextUnformatted( label );
    ImGui::PopFont();
    ImGui::Spacing();
}

void Frontend::reloadImguiLayoutFromDefault()
{
    ImGui::LoadIniSettingsFromDisk( "..\\layout.default.ini" );
    ImGui::MarkIniSettingsDirty();
}

} // namespace module
} // namespace app
