//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

namespace app {

struct Core;
struct Module
{
    virtual ~Module() {}
    
    virtual absl::Status create( const app::Core* appCore ) 
    { 
        ABSL_ASSERT( appCore != nullptr );
        m_appCore = appCore; 
        return absl::OkStatus();
    }

    virtual void destroy()
    { 
        m_appCore = std::nullopt;
    }

    virtual std::string getModuleName() const = 0;

    std::optional< const app::Core* >     m_appCore;
};

} // namespace app

