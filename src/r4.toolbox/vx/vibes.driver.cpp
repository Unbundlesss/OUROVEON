//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//

#include "pch.h"

#include "vx/vibes.driver.h"

#include "gfx/gl/macro.h"
#include "spacetime/moment.h"

#include "endlesss/toolkit.exchange.h"

namespace vx {

// ---------------------------------------------------------------------------------------------------------------------
VibeDriver::~VibeDriver()
{
    if ( m_glDriverTexture != 0 )
    {
        glChecked( glDeleteTextures( 1, &m_glDriverTexture ) );
        m_glDriverTexture = 0;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
absl::Status VibeDriver::create()
{
    ABSL_ASSERT( m_glDriverTexture == 0 );

    m_texData[0].fill( 0 );
    m_texData[1].fill( 0 );

    bool bufferSetupFailed = false;

    glCheckedCall( bufferSetupFailed, glGenTextures( 1, &m_glDriverTexture ) );
    glCheckedCall( bufferSetupFailed, glBindTexture( GL_TEXTURE_2D, m_glDriverTexture ) );

    glCheckedCall( bufferSetupFailed, glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST ) );
    glCheckedCall( bufferSetupFailed, glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST ) );
    glCheckedCall( bufferSetupFailed, glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE ) );
    glCheckedCall( bufferSetupFailed, glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE ) );

    glCheckedCall( bufferSetupFailed, glPixelStorei( GL_PACK_ALIGNMENT, 4 ) );
    glCheckedCall( bufferSetupFailed, glPixelStorei( GL_UNPACK_ROW_LENGTH, 8 ) );
    glCheckedCall( bufferSetupFailed, glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        8,
        cHistoryLength,
        0,
        GL_BGRA,
        GL_UNSIGNED_INT_8_8_8_8_REV,
        (void*)m_texData[0].data() ) );

    glCheckedCall( bufferSetupFailed, glBindTexture( GL_TEXTURE_2D, 0 ) );

    if ( bufferSetupFailed )
        return absl::UnknownError( "failed to create audio texture" );

    return absl::OkStatus();
}

// ---------------------------------------------------------------------------------------------------------------------
void VibeDriver::sync( const endlesss::toolkit::Exchange& exchangeData )
{
    m_texDataFlip = !m_texDataFlip;
    const uint32_t currentBuffer = m_texDataFlip ? 1 : 0;
    const uint32_t previousBuffer = m_texDataFlip ? 0 : 1;

    memcpy(
        m_texData[currentBuffer].data() + 8,    // fill the second row onwards of the current buffer with the previous buffer
        m_texData[previousBuffer].data(),       // to keep a history of N previous values scrolling down
        sizeof( uint32_t ) * 8 * (cHistoryLength - 1) );

    for ( std::size_t stemI = 0; stemI < 8; stemI++ )
    {
        m_texData[currentBuffer][stemI] = ImGui::ColorConvertFloat4ToU32_BGRA_Flip( ImVec4(
            exchangeData.m_stemPulse[stemI],
            exchangeData.m_stemEnergy[stemI],
            exchangeData.m_scope[stemI],
            exchangeData.m_stemGain[stemI]
        ) );
    }

    glChecked( glBindTexture( GL_TEXTURE_2D, m_glDriverTexture ) );

    glChecked( glPixelStorei( GL_PACK_ALIGNMENT, 4 ) );
    glChecked( glPixelStorei( GL_UNPACK_ROW_LENGTH, 8 ) );

    glChecked( glTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        0,
        0,
        8,
        cHistoryLength,
        GL_BGRA,
        GL_UNSIGNED_INT_8_8_8_8_REV,
        (void*)m_texData[currentBuffer].data() ) );

    glChecked( glBindTexture( GL_TEXTURE_2D, 0 ) );
}


} // namespace vx

