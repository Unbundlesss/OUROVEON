//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//

#pragma once

namespace spacetime {

struct Moment
{
    using HighResTimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

    Moment()
        : m_initialTime( now() )
    {
    }

    void restart()
    {
        m_initialTime = now();
    }

    ouro_nodiscard inline static HighResTimePoint now()
    {
        return std::chrono::high_resolution_clock::now();
    }

    ouro_nodiscard std::chrono::seconds deltaSec()
    {
        return std::chrono::duration_cast<std::chrono::seconds>(now() - m_initialTime);
    }

    ouro_nodiscard std::chrono::milliseconds deltaMs()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(now() - m_initialTime);
    }

    ouro_nodiscard std::chrono::microseconds deltaUs()
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(now() - m_initialTime);
    }

protected:
    HighResTimePoint    m_initialTime;
};

struct ScopedTimer : public Moment
{
    ScopedTimer( const char* context )
        : Moment()
        , m_context( context )
        , m_running( true )
    {
    }

    ~ScopedTimer();

    std::chrono::milliseconds stop()
    {
        const auto delta = deltaMs();
        m_running = false;
        return delta;
    }

    void stage( const char* name );

private:
    std::string         m_context;
    bool                m_running;
};

} // namespace spacetime
