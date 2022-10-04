//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//

#include "pch.h"

#include "vx/vibes.shader.h"

#include "gfx/gl/macro.h"

#define HandleIsValid( _hd )    ( _hd != 0xffffffff )

namespace vx {

// ---------------------------------------------------------------------------------------------------------------------
VibeShader::VibeShader( std::string name, gl::ShaderInstance& instance ) 
    : m_shaderName( std::move( name ) )
    , m_shaderInstance( instance )
{
}

// ---------------------------------------------------------------------------------------------------------------------
VibeShader::~VibeShader()
{
    if ( m_glVAO != 0 )
    {
        glChecked( glDeleteVertexArrays( 1, &m_glVAO ) );
    }
}

// ---------------------------------------------------------------------------------------------------------------------
absl::Status VibeShader::bind( const ArrayBufferHandles& arrayBufferHandles )
{
    {
        gl::ScopedUseShader setShader( m_shaderInstance );

        const int32_t shaderHandle = m_shaderInstance->getHandle();

        bool uniformBindingFailed = false;

        m_glUniformViewportUV   = glCheckedCall( uniformBindingFailed, glGetUniformLocation( shaderHandle, "iViewportUV" ) );
        m_glUniformViewportOff  = glCheckedCall( uniformBindingFailed, glGetUniformLocation( shaderHandle, "iViewportOffset" ) );
        m_glUniformResolution   = glCheckedCall( uniformBindingFailed, glGetUniformLocation( shaderHandle, "iResolution" ) );
        m_glUniformTime         = glCheckedCall( uniformBindingFailed, glGetUniformLocation( shaderHandle, "iTime" ) );
        m_glUniformBeat         = glCheckedCall( uniformBindingFailed, glGetUniformLocation( shaderHandle, "iBeat" ) );
        m_glUniformTexAudio     = glCheckedCall( uniformBindingFailed, glGetUniformLocation( shaderHandle, "iAudio" ) );
        m_glUniformTexInputA    = glCheckedCall( uniformBindingFailed, glGetUniformLocation( shaderHandle, "iInputBufferA" ) );
        m_glUniformTexInputB    = glCheckedCall( uniformBindingFailed, glGetUniformLocation( shaderHandle, "iInputBufferB" ) );

        m_glPositionVertex      = glCheckedCall( uniformBindingFailed, (uint32_t)glGetAttribLocation( shaderHandle, "iPosition" ) );
        if ( !HandleIsValid( m_glPositionVertex ) )
            blog::error::app( "[position] attrib index is not valid" );

        m_glTexcoordVertex      = glCheckedCall( uniformBindingFailed, (uint32_t)glGetAttribLocation( shaderHandle, "iTexcoord" ) );
        if ( !HandleIsValid( m_glTexcoordVertex ) )
            blog::error::app( "[texcoord] attrib index is not valid" );

        if ( uniformBindingFailed )
            return absl::UnknownError( "could not bind shader uniforms" );
    }
    {
        bool vaoCreationFailed = false;

        glCheckedCall( vaoCreationFailed, glGenVertexArrays( 1, &m_glVAO ) );
        glCheckedCall( vaoCreationFailed, glBindVertexArray( m_glVAO ) );

        if ( HandleIsValid( m_glPositionVertex ) )
        {
            glCheckedCall( vaoCreationFailed, glBindBuffer( GL_ARRAY_BUFFER, arrayBufferHandles[0] ) );
            glCheckedCall( vaoCreationFailed, glEnableVertexAttribArray( m_glPositionVertex ) );
            glCheckedCall( vaoCreationFailed, glVertexAttribPointer(
                m_glPositionVertex,
                VibeShader::cRenderFloatsPerVertex,
                GL_FLOAT,
                GL_FALSE,
                sizeof( float ) * VibeShader::cRenderFloatsPerVertex,
                (GLvoid*)0
            ) );
        }
        if ( HandleIsValid( m_glTexcoordVertex ) )
        {
            glCheckedCall( vaoCreationFailed, glBindBuffer( GL_ARRAY_BUFFER, arrayBufferHandles[1] ) );
            glCheckedCall( vaoCreationFailed, glEnableVertexAttribArray( m_glTexcoordVertex ) );
            glCheckedCall( vaoCreationFailed, glVertexAttribPointer(
                m_glTexcoordVertex,
                VibeShader::cRenderFloatsPerVertex,
                GL_FLOAT,
                GL_FALSE,
                sizeof( float ) * VibeShader::cRenderFloatsPerVertex,
                (GLvoid*)0
            ) );
        }

        // unbind
        glCheckedCall( vaoCreationFailed, glBindVertexArray( 0 ) );
        glCheckedCall( vaoCreationFailed, glBindBuffer( GL_ARRAY_BUFFER, 0 ) );

        if ( vaoCreationFailed )
            return absl::UnknownError( "could not create shader VAO" );
    }

    return absl::OkStatus();
}

} // namespace vx

