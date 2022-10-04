//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//

#include "pch.h"

#include "vx/vibes.h"
#include "vx/vibes.blueprint.h"
#include "vx/vibes.fbo.h"
#include "vx/vibes.driver.h"
#include "vx/vibes.shader.h"

#include "app/module.frontend.fonts.h"
#include "app/imgui.ext.h"
#include "app/core.h"

#include "math/rng.h"
#include "base/hashing.h"
#include "base/text.h"

#include "spacetime/moment.h"
#include "filesys/preprocessing.h"

#include "endlesss/toolkit.exchange.h"

#include "gfx/gl/shader.h"
#include "gfx/gl/macro.h"

#include "simplecpp.h"


// ---------------------------------------------------------------------------------------------------------------------
#define _VB_VIEW(_action)           \
      _action(Shader)               \
      _action(Global)
REFLECT_ENUM( VibesControl, uint32_t, _VB_VIEW );

std::string generateVibesControlTitle( const VibesControl::Enum _vwv )
{
#define _ACTIVE_ICON(_ty)             _vwv == VibesControl::_ty ? ICON_FC_FILLED_SQUARE : ICON_FC_HOLLOW_SQUARE,
#define _ICON_PRINT(_ty)             "{}"

    return fmt::format( FMTX( "Vibes Control [" _VB_VIEW( _ICON_PRINT ) "]###vibes_control" ),
        _VB_VIEW( _ACTIVE_ICON )
        "" );

#undef _ICON_PRINT
#undef _ACTIVE_ICON
}

#undef _VB_VIEW
// ---------------------------------------------------------------------------------------------------------------------

namespace vx {

// ---------------------------------------------------------------------------------------------------------------------
struct VibePlan
{
    using UniquePtr = std::shared_ptr< VibePlan >;

    struct Operation
    {
        struct Toggle
        {
            std::string                     m_name;
            std::string                     m_active;
            std::vector< std::string >      m_opts;
        };
        struct Dial
        {
            std::string                     m_name;
            float                           m_current = 0;
            float                           m_default = 0;
            float                           m_min = 0;
            float                           m_max = 0;
        };

        using UniquePtr = std::shared_ptr< Operation >;

        std::string                         m_name;

        fs::path                            m_vertexShaderFile;
        fs::path                            m_fragmentShaderFile;

        VibeFBO::SharedPtr                  m_fboInputA;
        VibeFBO::SharedPtr                  m_fboInputB;
        VibeFBO::SharedPtr                  m_fboOutput;

        VibeShader::ArrayBufferHandles      m_arrayBufferHandles;
        VibeShader::PtrOrStatus             m_shader;

        std::vector< Toggle >               m_toggles;
        std::vector< Dial >                 m_dials;


        inline bool compileShader( const filesys::Preprocessing& ppState )
        {
            filesys::Preprocessing shaderPrepro;
            shaderPrepro.state() = ppState.state();

            for ( const auto& tog : m_toggles )
            {
                if ( !tog.m_active.empty() )
                    shaderPrepro.state().defines.push_back( tog.m_active );
            }

            auto shaderLoadStatus = gl::Shader::loadFromDisk( m_name, shaderPrepro, m_vertexShaderFile, m_fragmentShaderFile );
            if ( !shaderLoadStatus.ok() )
            {
                m_shader = shaderLoadStatus.status();
                return false;
            }

            VibeShader::UniquePtr shaderResult = std::make_unique< VibeShader >( m_name, shaderLoadStatus.value() );
            if ( const auto bindStatus = shaderResult->bind( m_arrayBufferHandles ); !bindStatus.ok() )
            {
                m_shader = bindStatus;
                return false;
            }

            m_shader = VibeShader::PtrOrStatus( std::move( shaderResult ) );
            return true;
        }

        inline void imgui( const filesys::Preprocessing& ppState )
        {
            bool recompileOnStateChange = false;

            for ( auto& toggle : m_toggles )
            {
                if ( toggle.m_opts.size() == 1 )
                {
                    bool currentlyActive = ( toggle.m_active == toggle.m_opts[0] );
                    if ( ImGui::Checkbox( toggle.m_name.c_str(), &currentlyActive ) )
                    {
                        if ( currentlyActive )
                            toggle.m_active = toggle.m_opts[0];
                        else
                            toggle.m_active.clear();

                        recompileOnStateChange = true;
                    }
                }
                else
                {
                    ImGui::TextUnformatted( toggle.m_name );
                    ImGui::Indent();
                    for ( auto& radio : toggle.m_opts )
                    {
                        if ( ImGui::RadioButton( radio.c_str(), radio == toggle.m_active ) )
                        {
                            toggle.m_active = radio;
                            recompileOnStateChange = true;
                        }
                    }
                    ImGui::Unindent();
                }
                ImGui::Spacing();
            }
            if ( recompileOnStateChange )
            {
                compileShader( ppState );
            }
        }
    };

    struct Declaration
    {
        using UniquePtr = std::shared_ptr< Declaration >;

        std::string                         m_name;
        std::vector< Operation::UniquePtr > m_operations;

        // return any compilation failures in the set of operations, or Ok if we're ready to render
        inline absl::Status checkAllOperations()
        {
            for ( const auto& op : m_operations )
            {
                if ( !op->m_shader.ok() )
                    return op->m_shader.status();
            }
            return absl::OkStatus();
        }

        // recompile all shaders
        inline void recompileAllOperations( const filesys::Preprocessing& ppState )
        {
            // recompile all stages
            for ( auto& op : m_operations )
                op->compileShader( ppState );
        }

        inline void render(
            const bool bufferFlip,
            const VibeDriver::UniquePtr& vibeDriver,
            const endlesss::toolkit::Exchange& data,
            const int32_t targetWidth,
            const int32_t targetHeight )
        {
            const float timeValue = (float)ImGui::GetTime();
            const std::size_t currentBufferIdx  = bufferFlip ? 1 : 0;
            const std::size_t previousBufferIdx = bufferFlip ? 0 : 1;

            for ( auto& op : m_operations )
            {
                ScopedVibeFBO vibeBind( op->m_fboOutput, bufferFlip );

                const VibeShader* vibeShader = (*op->m_shader).get();

                gl::ScopedUseShader scopeShader( vibeShader->getShaderInstance() );

                const int32_t bufferSizeMidpoint = op->m_fboOutput->getDimensions() >> 1;
                const int32_t bufferViewStartX  = bufferSizeMidpoint - (targetWidth >> 1);
                const int32_t bufferViewStartY  = bufferSizeMidpoint - (targetHeight >> 1);

                glChecked( glViewport( 
                    bufferViewStartX,
                    bufferViewStartY,
                    targetWidth, 
                    targetHeight ) );

                glChecked( glActiveTexture( GL_TEXTURE0 ) );
                glChecked( glBindTexture( GL_TEXTURE_2D, vibeDriver->getRenderTextureID() ) );

                glChecked( glActiveTexture( GL_TEXTURE1 ) );
                if ( op->m_fboInputA != nullptr )
                {
                    const bool usePreviousBuffer = ( op->m_fboInputA == op->m_fboOutput );
                    glChecked( glBindTexture( GL_TEXTURE_2D, op->m_fboInputA->getRenderTextureID( usePreviousBuffer ? !bufferFlip : bufferFlip ) ) );
                }
                else
                {
                    glChecked( glBindTexture( GL_TEXTURE_2D, 0 ) );
                }

                glChecked( glActiveTexture( GL_TEXTURE2 ) );
                if ( op->m_fboInputB != nullptr )
                {
                    const bool usePreviousBuffer = ( op->m_fboInputB == op->m_fboOutput );
                    glChecked( glBindTexture( GL_TEXTURE_2D, op->m_fboInputB->getRenderTextureID( usePreviousBuffer ? !bufferFlip : bufferFlip ) ) );
                }
                else
                {
                    glChecked( glBindTexture( GL_TEXTURE_2D, 0 ) );
                }

                glChecked( glUniform1i( vibeShader->m_glUniformTexAudio, 0 ) );
                glChecked( glUniform1i( vibeShader->m_glUniformTexInputA, 1 ) );
                glChecked( glUniform1i( vibeShader->m_glUniformTexInputB, 2 ) );

                const float outputDimsRecp = op->m_fboOutput->getDimensionsRecp();

                
                glChecked( glUniform2f( vibeShader->m_glUniformViewportOff,  outputDimsRecp * (float)bufferViewStartX, outputDimsRecp * (float)bufferViewStartY ) );
                glChecked( glUniform2f( vibeShader->m_glUniformViewportUV,  outputDimsRecp * (float)targetWidth, outputDimsRecp * (float)targetHeight ) );
                glChecked( glUniform2f( vibeShader->m_glUniformResolution,  (float)targetWidth, (float)targetHeight ) );
                glChecked( glUniform1f( vibeShader->m_glUniformTime,        timeValue ) );

                glChecked( glUniform4f( vibeShader->m_glUniformBeat,        data.m_consensusBeat, data.m_riffBeatSegmentCount, data.m_riffBeatSegmentActive, 0.0f ) );

                glChecked( glBindVertexArray( vibeShader->m_glVAO ) );
                glChecked( glDrawArrays( GL_TRIANGLES, 0, VibeShader::cRenderTriangleCount * VibeShader::cRenderVerticesPerTriangle ) );
                glChecked( glBindVertexArray( 0 ) );

                if ( op->m_fboInputA != nullptr )
                {
                    glChecked( glActiveTexture( GL_TEXTURE1 ) );
                    glChecked( glBindTexture( GL_TEXTURE_2D, 0 ) );
                }
                if ( op->m_fboInputB != nullptr )
                {
                    glChecked( glActiveTexture( GL_TEXTURE2 ) );
                    glChecked( glBindTexture( GL_TEXTURE_2D, 0 ) );
                }
            }
        }

        inline void imgui( const filesys::Preprocessing& ppState )
        {
            for ( auto& op : m_operations )
                op->imgui( ppState );
        }
    };

    std::vector< Declaration::UniquePtr >   m_declarations;

    // unpacked shader display arrays for selecting declaration from UI
    std::vector< const char* >              m_declLabels;
    std::vector< std::size_t >              m_declIndices;
};


// ---------------------------------------------------------------------------------------------------------------------
struct Vibes::State
{
    using NamedVibeFBOs = absl::flat_hash_map< std::string, VibeFBO::SharedPtr >;


    State()
    {
        m_glArrayBufferHandles.fill( 0 );
        generateNewRandomSeedString();
    }

    ~State()
    {
        if ( m_glArrayBufferHandles[0] != 0 )
        {
            glChecked( glDeleteBuffers( 2, m_glArrayBufferHandles.data() ) );
            m_glArrayBufferHandles[0] = 0;
        }
    }

    inline void generateNewRandomSeedString()
    {
        m_randomGen.reseed();
        m_randomString.clear();
        for ( auto rI = 0; rI < 6; rI++ )
            m_randomString += (char)m_randomGen.genInt32( 'a', 'z' );
        setRandomSeedFromString();
    }

    inline void setRandomSeedFromString()
    {
        const auto newSeed = base::HashString32( m_randomString );
        blog::app( FMTX( "New random seed set from '{}' => {}" ), m_randomString, newSeed );
        m_randomGen.reseed( newSeed );
    }

    absl::Status initialize( const config::IPathProvider& pathProvider )
    {
        spacetime::ScopedTimer perf( "vibe-compile" );

        m_shaderLoadRootPath = pathProvider.getPath( config::IPathProvider::PathFor::SharedData );

        m_preprocessingState.state().includePaths.push_back( m_shaderLoadRootPath.string() );


        // create the shared vertex positions/uvs used for all shader renders
        {
            bool bufferSetupFailed = false;

            glCheckedCall( bufferSetupFailed, glGenBuffers( 2, m_glArrayBufferHandles.data() ) );

            constexpr static auto vertexDataSize = (
                VibeShader::cRenderTriangleCount *
                VibeShader::cRenderVerticesPerTriangle *
                VibeShader::cRenderFloatsPerVertex
                );

            constexpr static std::array< float, vertexDataSize > vPosition
            {
                -1.0f, -1.0f,
                 1.0f, -1.0f,
                 1.0f,  1.0f,

                 1.0f,  1.0f,
                -1.0f,  1.0f,
                -1.0f, -1.0f
            };

            constexpr static std::array< float, vertexDataSize > vUV
            {
                0.0f, 0.0f,
                1.0f, 0.0f,
                1.0f, 1.0f,

                1.0f, 1.0f,
                0.0f, 1.0f,
                0.0f, 0.0f,
            };

            glCheckedCall( bufferSetupFailed, glBindBuffer( GL_ARRAY_BUFFER, m_glArrayBufferHandles[0] ) );
            glCheckedCall( bufferSetupFailed, glBufferData( GL_ARRAY_BUFFER, sizeof( float ) * vertexDataSize, (const GLvoid*)vPosition.data(), GL_STATIC_DRAW ) );

            glCheckedCall( bufferSetupFailed, glBindBuffer( GL_ARRAY_BUFFER, m_glArrayBufferHandles[1] ) );
            glCheckedCall( bufferSetupFailed, glBufferData( GL_ARRAY_BUFFER, sizeof( float ) * vertexDataSize, (const GLvoid*)vUV.data(), GL_STATIC_DRAW ) );

            glCheckedCall( bufferSetupFailed, glBindBuffer( GL_ARRAY_BUFFER, 0 ) );

            if ( bufferSetupFailed )
                return absl::UnknownError( "unable to configure vertex buffers, check log for details" );
        }

        // create rendertarget collection
        constexpr std::array< const char*, 3 > fboList = {{ "final", "work1", "work2" }};
        for ( const auto& fboName : fboList )
        {
            VibeFBO::SharedPtr fboPtr = std::make_shared< VibeFBO >();

            if ( const auto status = fboPtr->create( 2048, fboName ); !status.ok() )
                return status;

            m_namedVibeFBOs.emplace( fboName, std::move( fboPtr ) );
        }

        // stash the final output target for use by imgui code
        m_finalOutputFBO = findFBObyName( "final" );
        if ( m_finalOutputFBO == nullptr )
            return absl::UnknownError( "final FBO not found, required to vibe" );

        // create audio data -> shader helper
        m_vibeDriver = std::make_unique< VibeDriver >();
        if ( const auto status = m_vibeDriver->create(); !status.ok() )
            return status;

        {
            const auto blueprintPath = m_shaderLoadRootPath / "shaders" / "vibes.json";
            VibeBlueprint blueprintData;

            if ( !fs::exists( blueprintPath ) )
                return absl::UnknownError( "vibes blueprint data not found" );

            try
            {
                std::ifstream is( blueprintPath );
                cereal::JSONInputArchive archive( is );

                blueprintData.serialize( archive );
            }
            catch ( cereal::Exception& cEx )
            {
                return absl::InternalError( cEx.what() );
            }

            if ( const auto status = createPlan( m_shaderLoadRootPath, blueprintData ); !status.ok() )
                return status;
        }

        return absl::OkStatus();
    }

    absl::Status createPlan( const fs::path shaderRootPath, const VibeBlueprint& blueprint )
    {
        m_plan = std::make_unique< VibePlan >();

        m_plan->m_declLabels.reserve( blueprint.declarations.size() );
        m_plan->m_declIndices.reserve( blueprint.declarations.size() );

        for ( const auto& decl : blueprint.declarations )
        {
            auto planDecl = std::make_unique< VibePlan::Declaration >();
            planDecl->m_name = decl.name;

            for ( const auto& op : decl.operations )
            {
                auto planOp = std::make_unique< VibePlan::Operation >();

                planOp->m_name = op.name;

                planOp->m_vertexShaderFile   = shaderRootPath / "shaders" / "default.vsh.glsl";
                planOp->m_fragmentShaderFile = shaderRootPath / "shaders" / fmt::format( "{}.psh.glsl", op.fragment );

                planOp->m_fboInputA    = findFBObyName( op.bufferInA );
                planOp->m_fboInputB    = findFBObyName( op.bufferInB );
                planOp->m_fboOutput     = findFBObyName( op.bufferOut );

                if ( planOp->m_fboOutput == nullptr )
                    return absl::InternalError( fmt::format( FMTX("Could not resolve output FBO [{}] for operation [{}]"), op.bufferOut, op.name ) );

                planOp->m_arrayBufferHandles = m_glArrayBufferHandles;

                // transfer toggles
                for ( const auto toggle : op.toggles )
                {
                    auto& newToggle = planOp->m_toggles.emplace_back();
                    newToggle.m_name = toggle.name;
                    newToggle.m_opts = toggle.opts;

                    // if a default was specified, check it exists and install it as 'active'
                    if ( toggle.default_value != -1 )
                    {
                        if ( toggle.default_value >= toggle.opts.size() )
                        {
                            return absl::InternalError( fmt::format( FMTX( "Default toggle {} in {} overflows the list of options" ), toggle.default_value, toggle.name ) );
                        }
                        newToggle.m_active = newToggle.m_opts[toggle.default_value];
                    }
                }
                // transfer dials
                for ( const auto dial : op.dials )
                {
                    auto& newDial = planOp->m_dials.emplace_back();
                    newDial.m_name = dial.name;
                    newDial.m_current =
                        newDial.m_default = dial.default_value;

                    if ( dial.range.size() != 2 )
                    {
                        return absl::InternalError( fmt::format( FMTX( "Expected 2 values in [default] for {} but got {}" ), dial.name, dial.range.size() ) );
                    }

                    newDial.m_min = dial.range[0];
                    newDial.m_max = dial.range[1];
                }

                planOp->compileShader( m_preprocessingState );

                planDecl->m_operations.emplace_back( std::move( planOp ) );
            }

            // stash entries for the UI display
            m_plan->m_declLabels.push_back( planDecl->m_name.c_str() );
            m_plan->m_declIndices.push_back( m_plan->m_declarations.size() );

            m_plan->m_declarations.emplace_back( std::move( planDecl ) );
        }

        return absl::OkStatus();
    }

    inline VibeFBO::SharedPtr findFBObyName( const std::string& name )
    {
        if ( name.empty() )
            return nullptr;

        if ( auto it = m_namedVibeFBOs.find( name ); it != m_namedVibeFBOs.end() )
        {
            return it->second;
        }
        return nullptr;
    }

    inline void recompileAll()
    {
        spacetime::ScopedTimer perf( "vibe-recompile" );
        for ( auto& decl : m_plan->m_declarations )
        {
            decl->recompileAllOperations( m_preprocessingState );
        }
    }


    void doImGuiDisplay(
        const endlesss::toolkit::Exchange& data,
        app::ICoreCustomRendering* ICustomRendering );

    void doImGuiCtrlShader();
    void doImGuiCtrlGlobal();


    VibePlan::UniquePtr             m_plan;
    VibesControl::Enum              m_vibesControlMode = VibesControl::Shader;

    std::size_t                     m_selectionIndex = 0;
    std::string                     m_selectionPreview;

    std::string                     m_randomString;
    math::RNG32                     m_randomGen;

    bool                            m_autoOversample = false;       // if enabled, expand target render to use as much of the 
                                                                    // reserved framebuffer size as possible (and then scale down in ImGui display)

    bool                            m_bufferFlip = false;

    int32_t                         m_framesVisible = 0;
    VibeShader::ArrayBufferHandles  m_glArrayBufferHandles;

    filesys::Preprocessing          m_preprocessingState;
    fs::path                        m_shaderLoadRootPath;

    NamedVibeFBOs                   m_namedVibeFBOs;
    VibeFBO::SharedPtr              m_finalOutputFBO;

    VibeDriver::UniquePtr           m_vibeDriver;
};

// ---------------------------------------------------------------------------------------------------------------------
void Vibes::State::doImGuiDisplay( const endlesss::toolkit::Exchange& data, app::ICoreCustomRendering* ICustomRendering )
{
    const auto renderView = ImGui::GetContentRegionAvail();

    m_vibeDriver->sync( data );

    // catch and reset any out-of-bound selection index
    if ( m_selectionIndex >= m_plan->m_declarations.size() )
        m_selectionIndex = 0;

    // fetch current shader decl, check it's compiled ok
    auto& currentDeclaration = m_plan->m_declarations[m_selectionIndex];
    auto currentStatus = currentDeclaration->checkAllOperations();

    if ( currentStatus.ok() )
    {
        // choose a target resolution; ensure we keep inside the currently allocated
        // framebuffer size and optionally scale up to greedily use as much of it as would fit
        // if auto-oversampling is enabled
        const float finalOutpuDim = (float)m_finalOutputFBO->getDimensions();
        float resolutionScale = 1.0f;
        {
            const float osW = finalOutpuDim / renderView.x;
            const float osH = finalOutpuDim / renderView.y;
            resolutionScale = std::min( osW, osH );
        }
        // only adjust scale >1, <1 means we are out of space and should always scale to fit
        if ( resolutionScale > 1.0f )
        {
            if ( !m_autoOversample )
                resolutionScale = 1.0f;
            else
                resolutionScale = std::floorf( resolutionScale );
        }

        const ImVec2 renderViewScaled = ImVec2(
            renderView.x * resolutionScale,
            renderView.y * resolutionScale );

        const int32_t renderWidth  = (int32_t)renderViewScaled.x;
        const int32_t renderHeight = (int32_t)renderViewScaled.y;

        const bool currentBufferFlip = m_bufferFlip;

        ICustomRendering->registerRenderCallback( app::ICoreCustomRendering::RenderPoint::PreImgui, [=]()
            {
                currentDeclaration->render( currentBufferFlip, m_vibeDriver, data, renderWidth, renderHeight );
            });

        ImVec2 uvOffset = ( renderViewScaled * m_finalOutputFBO->getDimensionsRecp() ) * 0.5f;
        
        ImGui::Image(
            (ImTextureID)(intptr_t)m_finalOutputFBO->getRenderTextureID( currentBufferFlip ),
            renderView,
            ImVec2( 0.5f, 0.5f ) - uvOffset,
            ImVec2( 0.5f, 0.5f ) + uvOffset,
            ImVec4( 1.0f, 1.0f, 1.0f, 1.0f ) );

        m_bufferFlip = !m_bufferFlip;
    }
    else
    {
        ImGui::TextUnformatted( "Shader Compilation Failure" );
        ImGui::CompactTooltip( currentStatus.ToString() );

        if ( ImGui::Button( "Recompile" ) )
        {
            currentDeclaration->recompileAllOperations( m_preprocessingState );
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
void Vibes::State::doImGuiCtrlShader()
{
    ImGui::Spacing();

    m_selectionPreview = ImGui::ValueArrayPreviewString( m_plan->m_declLabels, m_plan->m_declIndices, m_selectionIndex );
    if ( ImGui::ValueArrayComboBox( "Shader :", "##shdr", m_plan->m_declLabels, m_plan->m_declIndices, m_selectionIndex, m_selectionPreview, true ) )
    {
        
    }

    // shared shader configurations
    {
        ImGui::SeparatorBreak();
        ImGui::TextUnformatted( "Random Seed" );
        const bool textAccept = ImGui::InputText( "##rngs", &m_randomString, ImGuiInputTextFlags_EnterReturnsTrue );
        if ( textAccept || ImGui::IsItemDeactivatedAfterEdit() )
        {
            setRandomSeedFromString();
        }
        ImGui::SameLine();
        if ( ImGui::Button( " New " ) )
        {
            generateNewRandomSeedString();
        }
    }

    ImGui::SeparatorBreak();

    auto& currentDeclaration = m_plan->m_declarations[m_selectionIndex];
    currentDeclaration->imgui( m_preprocessingState );
}

// ---------------------------------------------------------------------------------------------------------------------
void Vibes::State::doImGuiCtrlGlobal()
{
    ImGui::Spacing();
    ImGui::TextUnformatted( "Rendering" );
    ImGui::Spacing();

    ImGui::Checkbox( "Auto Oversample", &m_autoOversample );

    if ( ImGui::Button( "Clean Buffers" ) )
    {
        for ( const auto& fbos : m_namedVibeFBOs )
            fbos.second->bufferClear();
    }
}

// ---------------------------------------------------------------------------------------------------------------------
Vibes::Vibes()
{

}

// ---------------------------------------------------------------------------------------------------------------------
Vibes::~Vibes()
{

}

// ---------------------------------------------------------------------------------------------------------------------
absl::Status Vibes::initialize( const config::IPathProvider& pathProvider )
{
    m_state = std::make_unique<State>();
    const auto stateInitStatus = m_state->initialize( pathProvider );
    if ( !stateInitStatus.ok() )
    {
        m_state.reset();
        m_initialised = false;
        return stateInitStatus;
    }

    m_initialised = true;
    return absl::OkStatus();
}

// ---------------------------------------------------------------------------------------------------------------------
void Vibes::doImGui(
    const endlesss::toolkit::Exchange&           data,
    app::ICoreCustomRendering*                   ICustomRendering )
{
    // main rendering panel
    do 
    {
        if ( ImGui::Begin( "Vibes###vibes_view" ) )
        {
            // did Vibes stuff actually boot ok?
            if ( !isInitialised() )
            {
                ImGui::TextUnformatted( ICON_FA_BOMB " [ Failed to Initialise ]" );
                ImGui::End();
                break;
            }

            // ensure we've been displayed for long enough to avoid flickers on boot
            // without this hold we can get a single frame of display as the docking UI sorts itself out (i think)
            m_state->m_framesVisible++;
            if ( m_state->m_framesVisible < 2 )
            {
                ImGui::End();
                break;
            }

            // process special key binds
            if ( ImGui::IsWindowHovered( ImGuiHoveredFlags_RootAndChildWindows ) )
            {
                // recompile everything on [home]
                if ( ImGui::IsKeyPressedMap( ImGuiKey_Home, false ) )
                {
                    blog::app( "[ Recompiling All Shaders ]" );
                    m_state->recompileAll();
                }
            }

            m_state->doImGuiDisplay( data, ICustomRendering );
        }
        ImGui::End();
    } while ( false );

    // other viewport allows for control of current effects, switching shaders etc
    do
    {
        // use a tab-switchable panel to present various vibe controls
        // m_state won't exist if vibes plan failed to load, so default to something so the UI layout remains consistent
        const auto viewTitle = generateVibesControlTitle( ( m_state != nullptr ) ? m_state->m_vibesControlMode : VibesControl::Shader );
        if ( ImGui::Begin( viewTitle.c_str() ) )
        {
            // did Vibes stuff actually boot ok?
            if ( !isInitialised() )
            {
                ImGui::TextUnformatted( ICON_FA_BOMB " [ Failed to Initialise ]" );
                ImGui::End();
                break;
            }

            // process special key binds
            if ( ImGui::IsWindowHovered( ImGuiHoveredFlags_RootAndChildWindows ) )
            {
                // swap panel on [tab]
                if ( ImGui::IsKeyPressedMap( ImGuiKey_Tab, false ) )
                {
                    m_state->m_vibesControlMode = VibesControl::getNextWrapped( m_state->m_vibesControlMode );
                }
            }

            switch ( m_state->m_vibesControlMode )
            {
                case VibesControl::Shader:  m_state->doImGuiCtrlShader(); break;
                case VibesControl::Global:  m_state->doImGuiCtrlGlobal(); break;
            }
        }
        ImGui::End();
    } while ( false );
}

} // namespace vx
