//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  

#include "pch.h"

#include "spacetime/moment.h"

namespace spacetime {

ScopedTimer::~ScopedTimer()
{
    if ( m_running )
    {
        const auto duration = stop();
        blog::perf( "{} took {}", m_context, duration );
    }
}

void ScopedTimer::stage( const char* name )
{
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>( now() - m_initialTime );
    blog::perf( "{} [{}] took {}", m_context, name, duration );
}

} // namespace spacetime
