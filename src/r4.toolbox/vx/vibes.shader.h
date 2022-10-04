//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "base/construction.h"

#include "gfx/gl/shader.h"

namespace vx {

// ---------------------------------------------------------------------------------------------------------------------
struct VibeShader
{
    DECLARE_NO_COPY_NO_MOVE( VibeShader );

    constexpr static uint32_t   cRenderTriangleCount        = 2;
    constexpr static uint32_t   cRenderVerticesPerTriangle  = 3;
    constexpr static uint32_t   cRenderFloatsPerVertex      = 2;    // xy, uv

    using ArrayBufferHandles    = std::array<uint32_t, 2>;

    using UniquePtr             = std::unique_ptr< VibeShader >;
    using PtrOrStatus           = absl::StatusOr< UniquePtr >;


    VibeShader( std::string name, gl::ShaderInstance& instance );
    ~VibeShader();

    absl::Status bind( const ArrayBufferHandles& arrayBufferHandles );

    const gl::ShaderInstance& getShaderInstance() const { return m_shaderInstance; }

private:

    std::string         m_shaderName;
    gl::ShaderInstance  m_shaderInstance;

public:
    uint32_t            m_glVAO                 = 0;

    uint32_t            m_glUniformViewportUV   = 0;
    uint32_t            m_glUniformViewportOff  = 0;
    uint32_t            m_glUniformResolution   = 0;
    uint32_t            m_glUniformTime         = 0;
    uint32_t            m_glUniformBeat         = 0;
    uint32_t            m_glUniformTexAudio     = 0;
    uint32_t            m_glUniformTexInputA    = 0;
    uint32_t            m_glUniformTexInputB    = 0;
    uint32_t            m_glPositionVertex      = 0;
    uint32_t            m_glTexcoordVertex      = 0;
};

} // namespace vx