//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  spacetime is a namespace full of datetime/chrono/etc related stuff
//  named as such to be clear separate and distinct, even if a little silly. 
//
//  'chronicle' relates to date/time stuff
//  everything here makes substantial use of chrono as well as Howard Hinnant's [date] library and TZ extension

#pragma once

namespace spacetime {

extern const char* defaultDisplayTimeFormatTZ;

using InSeconds = std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>;

// ---------------------------------------------------------------------------------------------------------------------
struct Delta
{
    date::years             m_years;
    date::months            m_months;
    date::days              m_days;
    std::chrono::hours      m_hours;
    std::chrono::minutes    m_mins;
    std::chrono::seconds    m_secs;

    // generate a human readable 'time in the past' string, omitting any 0-value entries
    // eg. "2 days, 18 hours, 3 minutes ago"
    // [maxShownElements] controls how many non-0 elements are shown to reduce precision
    //                    as the delta goes up; eg. if a date is >1 year ago and you ask
    //                    for max 3 elements, you'll get "2 years, 1 month, 3 days" rather
    //                    than a breakdown down to the second(s). 
    //
    std::string asPastTenseString( const int32_t maxShownElements = 6 ) const;
};

// ---------------------------------------------------------------------------------------------------------------------
// https://stackoverflow.com/questions/34916758/human-readable-duration-between-two-unix-timestamps
Delta calculateDelta( InSeconds t1, InSeconds t0 );

inline std::chrono::seconds getUnixTimeNow()
{
    return std::chrono::seconds{ std::time( nullptr ) };
}

inline Delta calculateDeltaFromNow( InSeconds t0 )
{
    return spacetime::calculateDelta( InSeconds{ getUnixTimeNow() }, t0 );
} 


// ---------------------------------------------------------------------------------------------------------------------
uint8_t getDayIndex( const InSeconds tss );

// ---------------------------------------------------------------------------------------------------------------------
template <typename TP>
std::time_t to_time_t( const TP tp )
{
    using namespace std::chrono;
    auto sctp = time_point_cast<system_clock::duration>(tp - TP::clock::now()
        + system_clock::now());
    return system_clock::to_time_t( sctp );
}

// ---------------------------------------------------------------------------------------------------------------------
// convert a ISO8601 time stamp string to seconds
int64_t parseISO8601( const std::string& timestamp );

// ---------------------------------------------------------------------------------------------------------------------
std::string datestampStringFromUnix( const uint64_t unix );
std::string datestampStringFromUnix( const InSeconds tss );

// ---------------------------------------------------------------------------------------------------------------------
bool datestampUnixExpiryFromNow( const uint64_t unix, uint32_t& outDays, uint32_t& outHours, uint32_t& outMins, uint32_t& outSecs );

// ---------------------------------------------------------------------------------------------------------------------
// generate something like 20210503.001242_ for use with adding as a prefix to files for date sorting
std::string createPrefixTimestampForFile();

} // namespace spacetime
