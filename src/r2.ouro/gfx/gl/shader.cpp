//   _______ _______ ______ _______ ___ ___ _______ _______ _______
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//
//

#include "pch.h"

#include "gfx/gl/shader.h"

void logGLErrorLog( GLuint glID )
{
    GLint logLength;
    glGetShaderiv( glID, GL_INFO_LOG_LENGTH, &logLength );
    if ( logLength > 0 )
    {
        GLchar* log = (GLchar*)calloc( logLength + 1, 1 );
        glGetShaderInfoLog( glID, logLength, &logLength, log );
        blog::error::gfx( "Compilation log : {}", log );
        free( log );
    }
}

GLuint compileShader(const char* context, const char* source, GLuint shaderType)
{
    GLuint result = glCreateShader(shaderType);

    glShaderSource(result, 1, &source, NULL);
    glCompileShader(result);

    GLint shaderCompiled = GL_FALSE;
    glGetShaderiv(result, GL_COMPILE_STATUS, &shaderCompiled);
    if (shaderCompiled != GL_TRUE)
    {
        blog::error::gfx( "Shader compilation error [{}] ({})", context, result );

        logGLErrorLog( result );
        glDeleteShader(result);
        result = 0;
    }
    else
    {
        blog::gfx( "Shader compiled [{}]", context );
    }
    return result;
}

GLuint compileProgram( const char* context, const char* vshFile, const char* pshFile )
{
    GLuint programId = 0;
    GLuint vtxShaderId, fragShaderId;

    programId = glCreateProgram();

    std::ifstream f(vshFile);
    std::string source((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    vtxShaderId = compileShader( context, source.c_str(), GL_VERTEX_SHADER );

    f = std::ifstream(pshFile);
    source = std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    fragShaderId = compileShader( context, source.c_str(), GL_FRAGMENT_SHADER );

    if (vtxShaderId && fragShaderId)
    {
        // Associate shader with program
        glAttachShader(programId, vtxShaderId);
        glAttachShader(programId, fragShaderId);
        glLinkProgram(programId);
        glValidateProgram(programId);

        // Check the status of the compile/link
        logGLErrorLog( programId );
    }

    return programId;
}


namespace gl {

Shader::~Shader()
{
    if ( m_handle != 0 )
        glDeleteProgram( m_handle );
}

std::shared_ptr<Shader> Shader::loadFromFiles( const std::string& context, const std::string& vsh, const std::string& psh )
{
    auto newId = compileProgram( context.c_str(), vsh.c_str(), psh.c_str() );
    if ( newId <= 0 )
        return nullptr;

    return std::make_shared<Shader>( newId );
}

ScopedShader::ScopedShader( ShaderInstance& shaderToSet )
    : m_shaderInUse( shaderToSet )
{
    const auto handle = shaderToSet->getHandle();
    if ( handle != 0 ) {
        glGetIntegerv( GL_CURRENT_PROGRAM, &m_previousHandle );
        glUseProgram( handle );
    }
}

ScopedShader::~ScopedShader()
{
    if ( m_previousHandle != 0 ) {
        glUseProgram( m_previousHandle );
    }
}

const char* checkGLErr()
{
    const auto err = glGetError();
    switch ( err )
    {
    case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
    case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
    }
    return nullptr;
}

Flatscreen::Flatscreen()
{
    glGenBuffers( 1, &m_glHandleArrayBuffer );
    auto* err = checkGLErr();

    m_shaderInstance = gl::Shader::loadFromFiles( "2d", "../../shared/shaders/vsh.2d.glsl", "../../shared/shaders/psh.2d.glsl" );
    {
        ScopedShader setShader( m_shaderInstance );

        m_glUniformResolution = glGetUniformLocation( m_shaderInstance->getHandle(), "iResolution" );
        err = checkGLErr();
        m_glUniformTime = glGetUniformLocation( m_shaderInstance->getHandle(), "iTime" );
        err = checkGLErr();
        m_glPositionVertex = (uint32_t)glGetAttribLocation( m_shaderInstance->getHandle(), "position" );
        err = checkGLErr();
    }
}

Flatscreen::~Flatscreen()
{
    if ( m_glHandleArrayBuffer ) 
    { 
        glDeleteBuffers( 1, &m_glHandleArrayBuffer ); m_glHandleArrayBuffer = 0;
    }
}
struct FsVert
{
    FsVert( float _x, float _y )
        : x(_x)
        , y(_y)
    {}
    float x, y;
};

void Flatscreen::render()
{
    ScopedShader setShader( m_shaderInstance );

    GLuint vao;
    glGenVertexArrays( 1, &vao );
    glBindVertexArray( vao );

    glDisable( GL_SCISSOR_TEST );
    glDisable( GL_BLEND );

    FsVert vertices[6]{
        {-1.0f,  1.0f},
        { 1.0f,  1.0f},
        { 1.0f, -1.0f},
        {-1.0f,  1.0f},
        { 1.0f, -1.0f},
        {-1.0f, -1.0f}
    };

    glUniform2f( m_glUniformResolution, 1920, 1080 );
    glUniform1f( m_glUniformTime, (float) ImGui::GetTime() );
    auto* err = checkGLErr();

    glBindBuffer( GL_ARRAY_BUFFER, m_glHandleArrayBuffer );
    err = checkGLErr();
    glBufferData( GL_ARRAY_BUFFER, (GLsizeiptr)6 * (int)sizeof( vertices ), (const GLvoid*)vertices, GL_STREAM_DRAW );
    err = checkGLErr();

    glEnableVertexAttribArray( m_glPositionVertex );
    err = checkGLErr();
    glVertexAttribPointer( m_glPositionVertex, 2, GL_FLOAT, GL_FALSE, sizeof( FsVert ), (GLvoid*)0 );
    err = checkGLErr();

    glDrawArrays( GL_TRIANGLES, 0, 6 );
    err = checkGLErr();

    glDeleteVertexArrays( 1, &vao );
}

} // namespace gl