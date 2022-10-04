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

namespace endlesss { namespace toolkit { struct Exchange; } }

namespace vx {

// ---------------------------------------------------------------------------------------------------------------------
// storage and conversion of audio playback data into a texture that shaders can read from
//
struct VibeDriver
{
    DECLARE_NO_COPY_NO_MOVE( VibeDriver );
    using UniquePtr = std::unique_ptr< VibeDriver >;

    VibeDriver() = default;
    ~VibeDriver();

    // height of buffer that defines how many values are kept as a rolling history, row 0 being newest data
    constexpr static uint32_t cHistoryLength = 256;

    // 8 values wide for each stem, each one having 4 values as RGBA
    using DriverData = std::array< uint32_t, 8 * cHistoryLength >;


    absl::Status create();

    void sync( const endlesss::toolkit::Exchange& exchangeData );


    constexpr bool isValid() const
    {
        return ( m_glDriverTexture != 0 );
    }

    constexpr uint32_t getRenderTextureID() const
    {
        return m_glDriverTexture;
    }

private:
    // data is double buffered to help with simple circular copying
    DriverData      m_texData[2];
    bool            m_texDataFlip = false;

    uint32_t        m_glDriverTexture = 0;
};

} // namespace vx