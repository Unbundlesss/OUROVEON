//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//

#include "pch.h"

#include "vx/vibes.fbo.h"

#include "gfx/gl/macro.h"


namespace vx {

// ---------------------------------------------------------------------------------------------------------------------
VibeFBO::~VibeFBO()
{
    for ( std::size_t idx = 0; idx < 2; idx++ )
    {
        if ( m_glRenderTexture[idx] != 0 )
        {
            glChecked( glDeleteTextures( 1, &m_glRenderTexture[idx] ) );
        }
        if ( m_glFBO[idx] != 0 )
        {
            glChecked( glDeleteFramebuffers( 1, &m_glFBO[idx] ) );
        }
    }
    m_glFBO.fill( 0 );
    m_glRenderTexture.fill( 0 );
}

// ---------------------------------------------------------------------------------------------------------------------
absl::Status VibeFBO::create( const int32_t dimensions, std::string fboName )
{
    ABSL_ASSERT( !isValid() );

    m_dimensions     = dimensions;
    m_dimensionsRecp = 1.0f / (float)m_dimensions;

    bool bufferSetupFailed = false;

    glCheckedCall( bufferSetupFailed, glGenFramebuffers( 2, m_glFBO.data() ) );
    glCheckedCall( bufferSetupFailed, glGenTextures( 2, m_glRenderTexture.data() ) );

    for ( std::size_t idx = 0; idx < 2; idx++ )
    {
        glCheckedCall( bufferSetupFailed, glBindFramebuffer( GL_FRAMEBUFFER, m_glFBO[idx] ) );

        // create RT
        glCheckedCall( bufferSetupFailed, glBindTexture( GL_TEXTURE_2D, m_glRenderTexture[idx] ) );

        glCheckedCall( bufferSetupFailed, glTexImage2D( 
            GL_TEXTURE_2D,
            0,
            GL_RGBA8,
            getDimensions(),
            getDimensions(),
            0,
            GL_BGRA,
            GL_UNSIGNED_INT_8_8_8_8_REV,
            nullptr ));

        glCheckedCall( bufferSetupFailed, glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR ) );
        glCheckedCall( bufferSetupFailed, glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR ) );
        glCheckedCall( bufferSetupFailed, glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE ) );
        glCheckedCall( bufferSetupFailed, glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE ) );

        // socket in the RT
        glCheckedCall( bufferSetupFailed, glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_glRenderTexture[idx], 0 ) );

        GLenum drawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
        glCheckedCall( bufferSetupFailed, glDrawBuffers( 1, drawBuffers ) );

        // check we have a usable framebuffer
        if ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE )
        {
            return absl::UnknownError( fmt::format( FMTX( "FBO glCheckFramebufferStatus failed [{}]" ), fboName ) );
        }

        // clean
        glCheckedCall( bufferSetupFailed, glClearColor( 0.0, 0.0, 0.0, 1.0 ) );
        glCheckedCall( bufferSetupFailed, glClear( GL_COLOR_BUFFER_BIT ) );

        // unbind
        glCheckedCall( bufferSetupFailed, glBindTexture( GL_TEXTURE_2D, 0 ) );
        glCheckedCall( bufferSetupFailed, glBindFramebuffer( GL_FRAMEBUFFER, 0 ) );

        if ( bufferSetupFailed )
            return absl::UnknownError( fmt::format( FMTX( "failed to create FBO [{}]" ), fboName ) );
    }

    m_name = std::move( fboName );
    return absl::OkStatus();
}

// ---------------------------------------------------------------------------------------------------------------------
void VibeFBO::bufferClear()
{
    for ( std::size_t idx = 0; idx < 2; idx++ )
    {
        glChecked( glBindFramebuffer( GL_FRAMEBUFFER, m_glFBO[idx] ) );
        glChecked( glClearColor( 0.0, 0.0, 0.0, 1.0 ) );
        glChecked( glClear( GL_COLOR_BUFFER_BIT ) );

        glChecked( glBindFramebuffer( GL_FRAMEBUFFER, 0 ) );
    }
}

// ---------------------------------------------------------------------------------------------------------------------
ScopedVibeFBO::ScopedVibeFBO( VibeFBO::SharedPtr fboPtr, const bool bufferFlip ) 
    : m_fboPtr( std::move( fboPtr ) )
{
    glChecked( glGetIntegerv( GL_FRAMEBUFFER_BINDING, &m_previousFBO ) );
    glChecked( glBindFramebuffer( GL_FRAMEBUFFER, m_fboPtr->getFramebufferID( bufferFlip ) ) );
}

// ---------------------------------------------------------------------------------------------------------------------
ScopedVibeFBO::~ScopedVibeFBO()
{
    glChecked( glBindFramebuffer( GL_FRAMEBUFFER, m_previousFBO ) );
}

} // namespace vx

