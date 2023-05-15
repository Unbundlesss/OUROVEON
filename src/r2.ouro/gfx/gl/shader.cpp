//   _______ _______ ______ _______ ___ ___ _______ _______ _______
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//
//

#include "pch.h"

#include "gfx/gl/macro.h"
#include "gfx/gl/enumstring.h"
#include "gfx/gl/shader.h"

#include "filesys/preprocessing.h"


// variant of glChecked that bails out with an absl error on error
#define glAbslChecked( ... )    \
        __VA_ARGS__;        \
        { \
            const auto glErr = glGetError(); \
            if ( glErr != GL_NO_ERROR ) \
            { \
                return absl::InternalError( fmt::format( FMTX("{}:{} OpenGL call '{}' error [{}] : {}"), __FUNCTION__, __LINE__, #__VA_ARGS__, glErr, gl::getGlErrorText(glErr) ) ); \
            } \
        }

// ---------------------------------------------------------------------------------------------------------------------
std::string glGetErrorLog( GLuint glID )
{
    std::string capturedLog;

    GLint logLength;
    glGetShaderiv( glID, GL_INFO_LOG_LENGTH, &logLength );
    if ( logLength > 0 )
    {
        GLchar* log = (GLchar*)calloc( logLength + 1, 1 );
        glGetShaderInfoLog( glID, logLength, &logLength, log );
        capturedLog = log;
        free( log );
    }

    return capturedLog;
}

// ---------------------------------------------------------------------------------------------------------------------
absl::StatusOr< GLuint > compileShader( const std::string& shaderName, const char* sourceText, const GLuint shaderType )
{
    // allocate shader ID, mark to delete it automatically if we exit early to avoid leaking the ID
    GLuint shaderId = glAbslChecked( glCreateShader( shaderType ) );
    absl::Cleanup discardShaderIdOnEarlyOut = [shaderId] { glChecked( glDeleteShader( shaderId ) ); };

    glAbslChecked( glShaderSource( shaderId, 1, &sourceText, NULL ) );
    glAbslChecked( glCompileShader( shaderId ) );

    GLint shaderCompiled = GL_FALSE;
    glAbslChecked( glGetShaderiv( shaderId, GL_COMPILE_STATUS, &shaderCompiled ) );
    if ( shaderCompiled != GL_TRUE )
    {
        const auto compilationLog = glGetErrorLog( shaderId );
        return absl::InternalError( fmt::format( FMTX( "{} compilation failure, log:\n{}" ), gl::enumToString( shaderType ), compilationLog ) );
    }

    // all good, so cancel deleting the ID
    std::move( discardShaderIdOnEarlyOut ).Cancel();
    return shaderId;
}

// ---------------------------------------------------------------------------------------------------------------------
absl::StatusOr< GLuint > compileProgram(
    const std::string&              shaderName,
    const filesys::Preprocessing&   ppState,
    const fs::path&                 vsh,
    const fs::path&                 psh )
{
    std::string shaderSourceWork;
    shaderSourceWork.reserve( 128 * 1024 );

    constexpr static const std::string_view shaderPrefix = R"(
#version 150 core
#extension GL_ARB_explicit_attrib_location : enable

)";

    const bool emitLinePragmas = false;

    // allocate new shader program ID; clean it on scope exit if we fail compilation for any reason to avoid leaking the ID
    GLuint programId = glAbslChecked( glCreateProgram() );
    absl::Cleanup discardProgramIdOnEarlyOut = [programId] { glChecked( glDeleteProgram( programId ) ); };

    // preprocess the vertex shader source
    shaderSourceWork = shaderPrefix;
    if ( const auto preproStatus = ppState.processAndAppend( vsh, shaderSourceWork, emitLinePragmas ); !preproStatus.ok() )
        return preproStatus;

    // .. and compile it
    const auto vertexShaderResult = compileShader( shaderName, shaderSourceWork.c_str(), GL_VERTEX_SHADER );
    if ( !vertexShaderResult.ok() )
        return vertexShaderResult;

    // then do the same for fragment shader source
    shaderSourceWork = shaderPrefix;
    if ( const auto preproStatus = ppState.processAndAppend( psh, shaderSourceWork, emitLinePragmas ); !preproStatus.ok() )
        return preproStatus;

    // .. and compile that
    const auto fragmentShaderResult = compileShader( shaderName, shaderSourceWork.c_str(), GL_FRAGMENT_SHADER );
    if ( !fragmentShaderResult.ok() )
        return fragmentShaderResult;


    // assuming all the above preprocessing and compilation was successful, we can link the result
    {
        // associate shader with program, link, validate
        glAbslChecked( glAttachShader( programId, *vertexShaderResult ) );
        glAbslChecked( glAttachShader( programId, *fragmentShaderResult ) );
        glAbslChecked( glLinkProgram( programId ) );

        // ensure that this succeeded
        GLint programLinked = 0;
        glChecked( glGetProgramiv( programId, GL_LINK_STATUS, &programLinked ) );

        if ( programLinked == 0 )
        {
            const auto compilationLog = glGetErrorLog( programId );
            return absl::InternalError( fmt::format( FMTX( "shader link failure, log:\n{}" ), compilationLog ) );
        }
    }

    // all good, so cancel deleting the ID
    std::move( discardProgramIdOnEarlyOut ).Cancel();
    return programId;
}


namespace gl {

// ---------------------------------------------------------------------------------------------------------------------
Shader::~Shader()
{
    if ( m_handle != 0 )
        glChecked( glDeleteProgram( m_handle ) );
}

// ---------------------------------------------------------------------------------------------------------------------
Shader::PtrOrStatus Shader::loadFromDisk(
    const std::string&              shaderName,
    const filesys::Preprocessing&   ppState,
    const fs::path&                 vsh,
    const fs::path&                 psh )
{
    auto compilationStatus = compileProgram( shaderName, ppState, vsh, psh );
    if ( !compilationStatus.ok() )
        return compilationStatus.status();

    return std::make_shared<Shader>( shaderName, *compilationStatus );
}

// ---------------------------------------------------------------------------------------------------------------------
absl::Status Shader::validate() const
{
    if ( m_handle <= 0 )
        return absl::InternalError( "invalid shader handle for validate()" );

    glAbslChecked( glValidateProgram( m_handle ) );

    GLint programValidated = 0;
    glChecked( glGetProgramiv( m_handle, GL_VALIDATE_STATUS, &programValidated ) );

    // validate link
    if ( programValidated == 0 )
    {
        const auto compilationLog = glGetErrorLog( m_handle );
        return absl::InternalError( fmt::format( FMTX( "shader validate() failure, log:\n{}" ), compilationLog ) );
    }

    return absl::OkStatus();
}

// ---------------------------------------------------------------------------------------------------------------------
ScopedUseShader::ScopedUseShader( const ShaderInstance& shaderToSet )
    : m_shaderInUse( shaderToSet )
{
    const auto handle = shaderToSet->getHandle();
    if ( handle != 0 )
    {
        glChecked( glGetIntegerv( GL_CURRENT_PROGRAM, &m_previousHandle ) );
        glChecked( glUseProgram( handle ) );
    }
}

// ---------------------------------------------------------------------------------------------------------------------
ScopedUseShader::~ScopedUseShader()
{
    if ( m_previousHandle != 0 )
    {
        glChecked( glUseProgram( m_previousHandle ) );
    }
}

} // namespace gl
