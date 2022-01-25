//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

namespace gl {

struct Shader
{
    Shader( int32_t handle )
        : m_handle( handle )
    {}
    ~Shader();

    static std::shared_ptr<Shader> loadFromFiles( const std::string& context, const std::string& vsh, const std::string psh );

    int32_t getHandle() const { return m_handle; }

private:

    int32_t        m_handle;
};
using ShaderInstance = std::shared_ptr<Shader>;

struct ScopedShader
{
    ScopedShader() = delete;
    ScopedShader( ShaderInstance& shaderToSet );
    ~ScopedShader();

private:
    ShaderInstance  m_shaderInUse;
    int32_t         m_previousHandle;
};

} // namespace gl

namespace gl {

struct Flatscreen
{
    Flatscreen();
    ~Flatscreen();

    void render();

    ShaderInstance  m_shaderInstance;
    uint32_t        m_glHandleArrayBuffer;
    uint32_t        m_glPositionVertex;

    uint32_t        m_glUniformResolution;
    uint32_t        m_glUniformTime;
};

} // namespace gl