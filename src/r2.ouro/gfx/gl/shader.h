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

namespace filesys { struct Preprocessing; }

namespace gl {

// ---------------------------------------------------------------------------------------------------------------------
// simple shader instance, built from a vsh/psh file combo
//
struct Shader
{
    Shader( const std::string& name, const int32_t handle )
        : m_name( name )
        , m_handle( handle )
    {}
    ~Shader();

    using SharedPtr = std::shared_ptr<Shader>;
    using PtrOrStatus = absl::StatusOr< SharedPtr >;

    ouro_nodiscard static PtrOrStatus loadFromDisk(
        const std::string& shaderName,                  // name for context
        const filesys::Preprocessing& ppState,          // preprocessing pipeline for shader source text
        const fs::path& vsh,                            // vertex shader
        const fs::path& psh );                          // fragment shader

    ouro_nodiscard constexpr const std::string& getName() const { return m_name; }
    ouro_nodiscard constexpr int32_t getHandle() const { return m_handle; }

private:

    std::string     m_name;
    int32_t         m_handle;
};
using ShaderInstance = Shader::SharedPtr;


// ---------------------------------------------------------------------------------------------------------------------
// swap in a shader, preserving and restoring the currently active one as this goes out of scope
//
struct ScopedUseShader
{
    DECLARE_NO_COPY_NO_MOVE( ScopedUseShader );
    ScopedUseShader() = delete;

    ScopedUseShader( const ShaderInstance& shaderToSet );
    ~ScopedUseShader();

private:
    ShaderInstance  m_shaderInUse;
    int32_t         m_previousHandle;
};

} // namespace gl
