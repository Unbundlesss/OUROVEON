//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#include "pch.h"

// pcg
#include "pcg_random.hpp"

namespace data {

struct UUID_V1_Time
{
    union
    {
        uint64_t    timestamp;
        uint8_t     bytes[8];
    } ts;
};

std::string generateUUID_V1( bool withHypens )
{
    using namespace date;
    using namespace std::chrono;

    pcg_extras::seed_seq_from<std::random_device> seed_source;

    pcg32 rng( seed_source );

    std::string result;

    UUID_V1_Time timePart;
    {
        // The timestamp is a 60-bit value.  For UUID version 1, this is
        // represented by Coordinated Universal Time (UTC) as a count of 100-
        // nanosecond intervals since 00:00:00.00, 15 October 1582 (the date of
        // Gregorian reform to the Christian calendar).

        const auto oct_15_1582 = date::sys_days{ date::month{10} / 15 / 1582 };
        const auto unix_epoch  = date::sys_days{ date::month{1} / 1 / 1970 };

        const auto now = system_clock::now();

        // seconds between 1582 and unix epoch
        const auto gregorianDiff                = unix_epoch.time_since_epoch() - oct_15_1582.time_since_epoch();
        const auto secondsFromOriginToUnixEpoch = duration_cast<seconds>(gregorianDiff);

        // unix epoch to right now
        const auto unixDiff                     = now.time_since_epoch() - unix_epoch.time_since_epoch();
        const auto secondsFromUnixToNow         = duration_cast<seconds>(unixDiff);

        // total seconds from 1582 to now
        const uint64_t totalSeconds = static_cast<uint64_t>( secondsFromOriginToUnixEpoch.count() ) +
                                      static_cast<uint64_t>( secondsFromUnixToNow.count() );

        // expand to 100-nanosecond intervals
        timePart.ts.timestamp = totalSeconds * 10000000;

        // simulate some bonus nanoseconds
        std::uniform_int_distribution<int32_t> nanoFill( 1, 10000000 - 1 );
        const int32_t nanoFillValue = nanoFill( rng );

        timePart.ts.timestamp += nanoFillValue;

        // encode 'version 1' nibble
        timePart.ts.bytes[7] |= 0x10;
    }

    // encode B variant into the clock sequence
    // endlesss ones seem to be 'A' so we're picking out OURO ones
    uint32_t clockSequence = 0xB000;
    {
        std::uniform_int_distribution<int32_t> clockFill( 1, 0x0FFF );
        clockSequence += clockFill( rng );
    }

    // instead of a MAC, 
    std::array< uint8_t, 6 > uuidNode;
    {
        std::uniform_int_distribution<int32_t> clockFill( 1, 0xFF );
        for ( auto index = 0; index < uuidNode.size(); index ++ )
            uuidNode[index] = clockFill( rng );
    }

    // 3d97c19b-74bd-11ee-a750-98ba22c5a8d6

    const std::string separator = withHypens ? "-" : "";

    result = fmt::format( FMTX( "{:02x}{:02x}{:02x}{:02x}{}{:02x}{:02x}{}{:02x}{:02x}{}{:02x}{:02x}{}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}" ),
        timePart.ts.bytes[3],
        timePart.ts.bytes[2],
        timePart.ts.bytes[1],
        timePart.ts.bytes[0],
        separator,
        timePart.ts.bytes[5],
        timePart.ts.bytes[4],
        separator,
        timePart.ts.bytes[7],
        timePart.ts.bytes[6],
        separator,
        ( ( clockSequence >> 8 ) & 0xff ),
        ( clockSequence & 0xff ),
        separator,
        uuidNode[0],
        uuidNode[1],
        uuidNode[2],
        uuidNode[3],
        uuidNode[4],
        uuidNode[5]
    );


    return result;
}

} // namespace data
