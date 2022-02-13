//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  OpenGL implementation for the Sketchbook texture page manager
//

#include "pch.h"

#include "spacetime/moment.h"

#include "gfx/gl/macro.h"
#include "gfx/sketchbook.h"

// ---------------------------------------------------------------------------------------------------------------------
// specific version of the GPU task that logs our GL texture handle
struct GPUTaskGL : public gfx::GPUTask
{
    GPUTaskGL( const uint32_t uploadID, gfx::SketchBufferPtr&& buffer )
        : GPUTask( uploadID, std::move( buffer ) )
    {}

    virtual bool getStateIfValid( ValidState& result ) const override
    {
        if ( m_valid )
        {
            result = m_validState;
            return true;
        }
        return false;
    }

    // privately expose that to the GL code
    gfx::SketchBufferPtr& getBufferPtr() { return m_buffer; }

    ValidState          m_validState;
    uint32_t            m_glTextureID;
    std::atomic_bool    m_valid = false;
};

// ---------------------------------------------------------------------------------------------------------------------

namespace gfx {

// [needs to be thread-safe, will be called from scheduleBufferUploadToGPU]
GPUTask* Sketchbook::allocateGPUTask( const uint32_t uploadID, SketchBufferPtr&& buffer )
{
    return new GPUTaskGL( uploadID, std::move( buffer ) );
}

void Sketchbook::destroyGPUTask( GPUTask* task )
{
    GPUTaskGL* glTask = static_cast<GPUTaskGL*>(task);

    if ( glTask->m_valid )
    {
        uint32_t textureHandle = glTask->m_glTextureID;
        glChecked( glDeleteTextures( 1, &textureHandle ) );

        OURO_SKETCH_VERBOSE( "gl-task freed [{}]", textureHandle );
    }
    delete glTask;
}

void Sketchbook::processGPUTask( GPUTask* task )
{
    spacetime::ScopedTimer uploadTiming( "Sketchbook::processGPUTask" );

    GPUTaskGL* glTask = static_cast<GPUTaskGL*>(task);

    uint32_t textureHandle;

    glChecked( glGenTextures( 1, &textureHandle ) );
    glChecked( glBindTexture( GL_TEXTURE_2D, textureHandle ) );

    OURO_SKETCH_VERBOSE( "gl-task allocated [{}]", textureHandle );

    glChecked( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR         ) );
    glChecked( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR         ) );
    glChecked( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE  ) );
    glChecked( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE  ) );

    const base::U32Buffer& bitmapBuffer = glTask->getBufferPtr()->get();

    glChecked( glPixelStorei( GL_UNPACK_ROW_LENGTH, bitmapBuffer.getWidth() ) );
    glChecked( glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        bitmapBuffer.getWidth(),
        bitmapBuffer.getHeight(),
        0,
        GL_BGRA,
        GL_UNSIGNED_INT_8_8_8_8_REV,
        bitmapBuffer.getBuffer() ) );

    glTask->m_glTextureID = textureHandle;

    // populate the generic state block that will be used by ImGui
    glTask->m_validState.m_imTextureID          = (void*)(intptr_t)textureHandle;
    glTask->m_validState.m_textureDimensions    = glTask->getBufferPtr()->dim();
    glTask->m_validState.m_usageDimensions      = glTask->getBufferPtr()->extents();

    // produce UV coordinates representing the given extents
    ImVec2 textureDimF( (float)glTask->m_validState.m_textureDimensions.width(), (float)glTask->m_validState.m_textureDimensions.height() );
    ImVec2   usageDimF( (float)glTask->m_validState.m_usageDimensions.width(),   (float)glTask->m_validState.m_usageDimensions.height() );

    glTask->m_validState.m_usageDimensionsVec2 = usageDimF;
    glTask->m_validState.m_usageUV             = usageDimF / textureDimF;

    glTask->m_valid = true;

    // we're done with the CPU data, release it back to the pool
    glTask->getBufferPtr().reset();
}

} // namespace gfx