//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#pragma once

#include "base/construction.h"

namespace vx {

// ---------------------------------------------------------------------------------------------------------------------
// fixed large render buffer used for final target and intermediaries
//
struct VibeFBO
{
    DECLARE_NO_COPY_NO_MOVE( VibeFBO );
    using SharedPtr = std::shared_ptr< VibeFBO >;

    VibeFBO() = default;
    ~VibeFBO();

    absl::Status create( const int32_t dimensions, std::string fboName );

    void bufferClear();


    constexpr bool isValid() const
    {
        return ( m_glFBO[0] != 0 ) && ( m_glRenderTexture[0] != 0 ) &&
               ( m_glFBO[1] != 0 ) && ( m_glRenderTexture[1] != 0 );
    }

    constexpr uint32_t getFramebufferID( const bool bufferFlip ) const
    {
        const std::size_t bufferIndex = bufferFlip ? 1 : 0;
        return m_glFBO[ bufferIndex ];
    }

    constexpr uint32_t getRenderTextureID( const bool bufferFlip ) const
    {
        const std::size_t bufferIndex = bufferFlip ? 1 : 0;
        return m_glRenderTexture[bufferIndex];
    }

    constexpr int32_t   getDimensions()     const { return m_dimensions; }
    constexpr float     getDimensionsRecp() const { return m_dimensionsRecp; }

    constexpr const std::string& getName()  const { return m_name; }

private:

    int32_t     m_dimensions;           // size of texture, NxN
    float       m_dimensionsRecp;       // 1.0f / m_dimensions

    std::string m_name;

    std::array< uint32_t, 2 > m_glFBO            = { 0, 0 };
    std::array< uint32_t, 2 > m_glRenderTexture  = { 0, 0 };
};


// ---------------------------------------------------------------------------------------------------------------------
// bind/unbind a FBO on variable scope
//
struct ScopedVibeFBO
{
    DECLARE_NO_COPY_NO_MOVE( ScopedVibeFBO );
    ScopedVibeFBO() = delete;

    ScopedVibeFBO( VibeFBO::SharedPtr fboPtr, const bool bufferFlip );
    ~ScopedVibeFBO();

private:
    VibeFBO::SharedPtr  m_fboPtr;
    int32_t             m_previousFBO = 0;
};

} // namespace vx