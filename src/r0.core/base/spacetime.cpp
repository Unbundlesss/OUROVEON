//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  spacetime is a namespace full of datetime/chrono/etc related stuff
//  named as such to be clear separate and distinct, even if a little silly

#include "pch.h"

#include "base/spacetime.h"

namespace base {
namespace spacetime {

const char* defaultDisplayTimeFormatTZ  = "%Y-%m-%d %I:%M %p %Z";
const char* defaultFileTimeFormatTZ     = "%Y%m%d.%H%M%S_";
                                           
// ---------------------------------------------------------------------------------------------------------------------
std::string Delta::asPastTenseString( const int32_t maxShownElements /* = 6 */ ) const
{
    std::string pastTense;
    pastTense.reserve( 70 );    // enough to cover our longest case

    const int32_t numYears  = (int32_t)m_years.count();
    const int32_t numMonths = (int32_t)m_months.count();
    const int32_t numDays   = (int32_t)m_days.count();
    const int32_t numHours  = (int32_t)m_hours.count();
    const int32_t numMins   = (int32_t)m_mins.count();
    const int32_t numSecs   = (int32_t)m_secs.count();

    bool needsCommaPrefix = false;
    int32_t stillValidElements = maxShownElements;

    const auto appendEntry = [&pastTense, &needsCommaPrefix, &stillValidElements]( int32_t count, const char* name )
    {
        // no elements left, ignore this
        if ( stillValidElements == 0 )
            return;

        // only append if we have a useful value to display
        if ( count > 0 )
        {
            pastTense += fmt::format( "{}{} {}{}", 
                needsCommaPrefix ? ", " : "",
                count,
                name, 
                (count > 1) ? "s" : "" );

            // consume a 'valid element' count
            stillValidElements--;

            needsCommaPrefix = true;
        }
    };

    const bool isToday  = (numYears == 0 && numMonths == 0 && numDays == 0);
    const bool isNow    = (numHours == 0 && numMins == 0 && numSecs == 0);

    if ( isToday && isNow )
        return "Now";

#if 0   // prefer the consistent look of limited number of elements rather than "Today" occasionally
    if ( isToday )
    {
        pastTense = "Today, ";
    }
    else
#endif
    {
        appendEntry( numYears, "year" );
        appendEntry( numMonths, "month" );
        appendEntry( numDays, "day" );
    }

    appendEntry( numHours, "hour" );
    appendEntry( numMins, "min" );
    appendEntry( numSecs, "sec" );

    pastTense += " ago";

    return std::move(pastTense);
}

// ---------------------------------------------------------------------------------------------------------------------
base::spacetime::Delta calculateDelta( InSeconds t1, InSeconds t0 )
{
    using namespace date;
    auto dp0 = floor<days>( t0 );
    auto dp1 = floor<days>( t1 );
    date::year_month_day ymd0 = dp0;
    date::year_month_day ymd1 = dp1;
    auto time0 = t0 - dp0;
    auto time1 = t1 - dp1;
    auto dy = ymd1.year() - ymd0.year();
    ymd0 += dy;
    dp0 = ymd0;
    t0 = dp0 + time0;
    if ( t0 > t1 )
    {
        --dy;
        ymd0 -= years{ 1 };
        dp0 = ymd0;
        t0 = dp0 + time0;
    }
    auto dm = ymd1.year() / ymd1.month() - ymd0.year() / ymd0.month();
    ymd0 += dm;
    dp0 = ymd0;
    t0 = dp0 + time0;
    if ( t0 > t1 )
    {
        --dm;
        ymd0 -= months{ 1 };
        dp0 = ymd0;
        t0 = dp0 + time0;
    }
    auto dd = dp1 - dp0;
    dp0 += dd;
    t0 = dp0 + time0;
    if ( t0 > t1 )
    {
        --dd;
        dp0 -= days{ 1 };
        t0 = dp0 + time0;
    }
    auto delta_time = time1 - time0;
    if ( time0 > time1 )
        delta_time += days{ 1 };
    auto dt = make_time( delta_time );
    return { dy, dm, dd, dt.hours(), dt.minutes(), dt.seconds() };
}

// ---------------------------------------------------------------------------------------------------------------------
uint8_t getDayIndex( const InSeconds t0 )
{
    using namespace date;
    auto dp = floor<days>( t0 );
    auto ymd = year_month_day{ dp };

    return (unsigned) ymd.day();
}

// ---------------------------------------------------------------------------------------------------------------------
int64_t parseISO8601( const std::string& timestamp )
{
    std::istringstream is( timestamp );

    std::chrono::sys_time<std::chrono::microseconds> tp;
    is >> date::parse( "%FT%TZ", tp );

    const auto timePointSec = std::chrono::time_point_cast<std::chrono::seconds>(tp);

    return timePointSec.time_since_epoch().count();
}

// ---------------------------------------------------------------------------------------------------------------------
std::string datestampStringFromUnix( const uint64_t unix )
{
    return datestampStringFromUnix( InSeconds{ std::chrono::seconds{ unix } } );
}

std::string datestampStringFromUnix( const InSeconds tss )
{
    auto zonedTime = date::make_zoned( date::current_zone(), tss );
    return date::format( base::spacetime::defaultDisplayTimeFormatTZ, zonedTime );
}

// ---------------------------------------------------------------------------------------------------------------------
bool datestampUnixExpiryFromNow( const uint64_t unix, uint32_t& outDays, uint32_t& outHours, uint32_t& outMins, uint32_t& outSecs )
{
    // TODO port this to our other func above
    std::time_t tA = (uint32_t)(unix);
    std::time_t tB = std::chrono::system_clock::to_time_t( std::chrono::system_clock::now() );

    std::tm* tmA = std::gmtime( &tA );
    std::tm* tmB = std::gmtime( &tB );

    auto ms = difftime( tA, tB );

    // in the past!
    if ( ms < 0 )
    {
        outDays = 0;
        outHours = 0;
        outMins = 0;
        outSecs = 0;
        return false;
    }

    outDays = (uint32_t)(ms / (60.0 * 60.0 * 24.0));
    ms -= outDays * (60.0 * 60.0 * 24.0);

    outHours = (uint32_t)(ms / (60.0 * 60.0));
    ms -= outHours * (60.0 * 60.0);

    outMins = (uint32_t)(ms / (60.0));
    ms -= outMins * (60.0);

    outSecs = (uint32_t)(ms);
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
std::string createPrefixTimestampForFile()
{
    const auto zonedNow = date::make_zoned( 
        date::current_zone(), 
        date::floor<std::chrono::seconds>(std::chrono::system_clock::now())
    );
    return date::format( defaultFileTimeFormatTZ, zonedNow );
}

} // namespace base
} // namespace spacetime