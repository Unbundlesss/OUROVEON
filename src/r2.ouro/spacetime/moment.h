//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//

#pragma once

namespace spacetime {

// eg. delta<std::chrono::seconds>()
template< typename T >
concept TDurationType = requires(T x)
{
    { std::chrono::duration{ std::move( x ) } } -> std::same_as<T>;
};

// ---------------------------------------------------------------------------------------------------------------------
// represents a stored moment in time
class Moment
{
public:
    Moment()
    {
        setToNow();
    }

    // set the moment to be the current instant
    void setToNow()
    {
        m_instant = now();
    }

    // set the moment to be a point in the future, offset from the current instant
    template< TDurationType TDuration >
    void setToFuture( const TDuration& timeOffset )
    {
        m_instant = now() + timeOffset;
    }

    // has the moment passed? if setToFuture() was used, this checks if that moment has gone by
    ouro_nodiscard bool hasPassed() const
    {
        return ( m_instant < now() );
    }

    // return the difference between this moment and the current instant in a chosen temporal format, eg
    // std::chrono::seconds
    // std::chrono::milliseconds
    // std::chrono::microseconds
    template< TDurationType TDuration >
    ouro_nodiscard constexpr auto delta() const
    {
        return std::chrono::duration_cast< TDuration >( now() - m_instant );
    }

private:
    using TTimeClock = std::chrono::high_resolution_clock;
    using TTimePoint = std::chrono::time_point< TTimeClock >;

    static TTimePoint now()
    {
        return TTimeClock::now();
    }

    TTimePoint m_instant;
};

// ---------------------------------------------------------------------------------------------------------------------
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
        const auto deltaMs = delta< std::chrono::milliseconds >();
        m_running = false;
        return deltaMs;
    }

    void stage( const char* name );

private:
    std::string         m_context;
    bool                m_running;
};

} // namespace spacetime
