//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
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
        blog::instr( "{} took {}", m_context, duration );
    }
}

void ScopedTimer::stage( const char* name )
{
    const auto duration = delta<std::chrono::milliseconds>();
    blog::instr( "{} [{}] took {}", m_context, name, duration );
}

} // namespace spacetime
