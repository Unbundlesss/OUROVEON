//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "app/core.h"
#include "base/construction.h"

namespace app { struct ICoreCustomRendering; }
namespace config { struct IPathProvider; }
namespace endlesss { namespace toolkit { struct Exchange; } }

namespace vx {

// ---------------------------------------------------------------------------------------------------------------------
struct Vibes
{
    DECLARE_NO_COPY_NO_MOVE( Vibes );

    Vibes();
    ~Vibes();

    ouro_nodiscard absl::Status initialize( const config::IPathProvider& pathProvider );
    constexpr bool isInitialised() const { return m_initialised && m_state != nullptr; }

    void doImGui(
        const endlesss::toolkit::Exchange& data,            // audio analysis from stems/riffs playback
        app::ICoreCustomRendering* ICustomRendering );      // render stage injcetion to schedule custom rendering

protected:

    bool                        m_initialised = false;

    struct State;
    std::unique_ptr< State >    m_state;
};

} // namespace vx
