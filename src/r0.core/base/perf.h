//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//

#pragma once

namespace perf {

struct TimingPoint
{
    using HighResTimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

    TimingPoint( const char* context )
        : m_startTime( now() )
        , m_context( context )
        , m_running( true )
    {
    }

    ~TimingPoint()
    {
        if ( m_running )
        {
            const auto duration = stop();
            blog::perf( "{} took {}", m_context, duration);
        }
    }

    inline static HighResTimePoint now()
    {
        return std::chrono::high_resolution_clock::now();
    }

    inline std::chrono::milliseconds stop()
    {
        auto endTime = now();
        m_running = false;
        return std::chrono::duration_cast<std::chrono::milliseconds>(endTime - m_startTime);
    }


    HighResTimePoint    m_startTime;
    std::string         m_context;
    bool                m_running;
};


} // namespace perf