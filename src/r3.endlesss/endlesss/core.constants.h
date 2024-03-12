//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#pragma once

namespace endlesss {
namespace constants {

// ---------------------------------------------------------------------------------------------------------------------
static constexpr std::array<const char*, 18 > cScaleNames = 
{
    "Major (Ionian)",           // 
    "Dorian",                   // mn
    "Phrygian",                 // mn
    "Lydian",                   // 
    "Mixolydian",               // 
    "Minor (Aeolian)",          // mn
    "Locrian",                  // mn
    "Minor Pentatonic",         // mn
    "Major Pentatonic",         // 
    "Suspended Pent.",          // 
    "Blues Minor Pent.",        // mn
    "Blues Major Pent.",        // 
    "Harmonic Minor",           // mn
    "Melodic Minor",            // mn
    "Double Harmonic",          // 
    "Blues",                    // 
    "Whole Tone",               // 
    "Chromatic"                 // 
};


// used with ValueArrayComboBox
static constexpr std::array< uint32_t, 18 > cScaleValues{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17 };

// ---------------------------------------------------------------------------------------------------------------------
// scale names suitable for using in filename outputs
static constexpr std::array<const char*, 18 > cScaleNamesFilenameSanitize =
{
    "major",
    "dorian",
    "phrygian",
    "lydian",
    "mixoly",
    "minor",
    "locrian",
    "minor_pent",
    "major_pent",
    "susp_pent",
    "blues_mnr_p",
    "blues_mjr_p",
    "harmonic_mnr",
    "melodic_mnr",
    "dbl_harmonic",
    "blues",
    "whole",
    "chromatic"
};

// ---------------------------------------------------------------------------------------------------------------------
static constexpr std::array<const char*, 12 > cRootNames =
{
    /*  0 */    "C",
    /*  1 */    "Db",   // c#
    /*  2 */    "D",
    /*  3 */    "Eb",   // d#
    /*  4 */    "E",
    /*  5 */    "F",
    /*  6 */    "F#",   // g flat
    /*  7 */    "G",
    /*  8 */    "Ab",   // g sharp
    /*  9 */    "A",
    /* 10 */    "Bb",   // a#
    /* 11 */    "B"
};

// used with ValueArrayComboBox
static constexpr std::array< uint32_t, 12 > cRootValues{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };


// circle of fifths translations

static constexpr std::array< uint32_t, 12 > cRoot_CoT_MajorMinor // outer to inner
{
     9, // C   -> Am
    10, // C#  -> A#m
    11, // D   -> Bm
     0, // D#  -> Cm
     1, // E   -> C#m
     2, // F   -> Dm
     3, // F#  -> D#m
     4, // G   -> Em
     5, // G#  -> Fm
     6, // A   -> F#m
     7, // A#  -> Gm
     8  // B   -> G#m
};

static constexpr std::array< uint32_t, 12 > cRoot_CoT_CW // clockwise
{
     7, // C   -> G
     8, // C#  -> G#
     9, // D   -> A
    10, // D#  -> A#
    11, // E   -> B
     0, // F   -> C
     1, // F#  -> C#
     2, // G   -> D
     3, // G#  -> D#
     4, // A   -> E
     5, // A#  -> F
     6  // B   -> F#
};

static constexpr std::array< uint32_t, 12 > cRoot_CoT_CCW // counter
{
     5, // C   -> F
     6, // C#  -> F#
     7, // D   -> G
     8, // D#  -> G#
     9, // E   -> A
    10, // F   -> A#
    11, // F#  -> B
     0, // G   -> C
     1, // G#  -> C#
     2, // A   -> D
     3, // A#  -> D#
     4  // B   -> E
};

} // namespace constants
} // namespace endlesss
