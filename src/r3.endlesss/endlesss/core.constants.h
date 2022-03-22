//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

namespace endlesss {
namespace constants {

// ---------------------------------------------------------------------------------------------------------------------
static constexpr std::array<const char*, 18 > cScaleNames = 
{
    "Major (Ionian)",
    "Dorian",
    "Phrygian",
    "Lydian",
    "Mixolydian",
    "Minor (Aeolian)",
    "Locrian",
    "Minor Pentatonic",
    "Major Pentatonic",
    "Suspended Pent.",
    "Blues Minor Pent.",
    "Blues Major Pent.",
    "Harmonic Minor",
    "Melodic Minor",
    "Double Harmonic",
    "Blues",
    "Whole Tone",
    "Chromatic"
};

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
static constexpr std::array<const char*, 13 > cRootNames =
{
    "C",
    "Db",
    "D",
    "Eb",
    "E",
    "F",
    "F#",
    "G",
    "Ab",
    "A",
    "Bb",
    "B",
    "-",  //  12
};

} // namespace constants
} // namespace endlesss
