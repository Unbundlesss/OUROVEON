//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#pragma once
#include "base/metaenum.h"

namespace endlesss {
namespace constants {

// ---------------------------------------------------------------------------------------------------------------------
static constexpr std::array<const char*, 18 > cScaleNames = 
{
  /*  0 */  "Major (Ionian)",           // 
  /*  1 */  "Dorian",                   // mn
  /*  2 */  "Phrygian",                 // mn
  /*  3 */  "Lydian",                   // 
  /*  4 */  "Mixolydian",               // 
  /*  5 */  "Minor (Aeolian)",          // mn
  /*  6 */  "Locrian",                  // mn
  /*  7 */  "Minor Pentatonic",         // mn
  /*  8 */  "Major Pentatonic",         // 
  /*  9 */  "Suspended Pent.",          // 
  /* 10 */  "Blues Minor Pent.",        // mn
  /* 11 */  "Blues Major Pent.",        // 
  /* 12 */  "Harmonic Minor",           // mn
  /* 13 */  "Melodic Minor",            // mn
  /* 14 */  "Double Harmonic",          // 
  /* 15 */  "Blues",                    // 
  /* 16 */  "Whole Tone",               // 
  /* 17 */  "Chromatic"                 // 
};

enum EnumScale : int8_t
{
    ES_MajorIonian,
    ES_Dorian,
    ES_Phrygian,
    ES_Lydian,
    ES_Mixolydian,
    ES_MinorAeolian,
    ES_Locrian,
    ES_MinorPentatonic,
    ES_MajorPentatonic,
    ES_SuspendedPentatonic,
    ES_BluesMinorPentatonic,
    ES_BluesMajorPentatonic,
    ES_HarmonicMinor,
    ES_MelodicMinor,
    ES_DoubleHarmonic,
    ES_Blues,
    ES_WholeTone,
    ES_Chromatic
};

// used with ValueArrayComboBox
static constexpr std::array< uint32_t, 18 > cScaleValues{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17 };

static_assert(ES_Chromatic == cScaleNames.size() - 1, "scale / enum mismatch");

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


// ---------------------------------------------------------------------------------------------------------------------
// circle of fifths translations

static constexpr std::array< int8_t, 12 > cRoot_CoT_MajorMinor // outer to inner
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

static constexpr std::array< int8_t, 12 > cRoot_CoT_CW // clockwise
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

static constexpr std::array< int8_t, 12 > cRoot_CoT_CCW // counter
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

// ---------------------------------------------------------------------------------------------------------------------
// control enum for picking adjacent key/scale pairs from a base
#define _HARMONIC_SEARCH(_action)           \
        _action(NoAdditions)                \
        _action(BasicAdjacent)              \
        _action(CloselyRelated)             \
        _action(RelatedAndInteresting)      \
        _action(RelatedAndInterestingExtra) \
        _action(NoRules)
REFLECT_ENUM( HarmonicSearch, uint32_t, _HARMONIC_SEARCH );
#undef _HARMONIC_SEARCH


// ---------------------------------------------------------------------------------------------------------------------
struct RootScalePair
{
    using List = std::vector< RootScalePair >;

    explicit RootScalePair( const int8_t _r, const int8_t _s )
        : root(_r)
        , scale(_s) 
    {}
    explicit RootScalePair( const uint32_t _r, const uint32_t _s )
    {
        ABSL_ASSERT( _r < cRootNames.size() );
        ABSL_ASSERT( _s < cScaleNames.size() );
        root = static_cast<int8_t>(_r);
        scale = static_cast<int8_t>(_s);
    }
    explicit RootScalePair( const int8_t _r, const EnumScale _es )
        : root( _r )
        , scale( static_cast<int8_t>(_es) )
    {}

    int8_t root;
    int8_t scale;
};

struct RootScalePairs
{
    RootScalePair::List     pairs;
    HarmonicSearch::Enum    searchMode;
};


inline void computeTonalAdjacents( const RootScalePair rootScale, RootScalePairs& results )
{
    const HarmonicSearch::Enum HarmonicSearchMode = results.searchMode;

    // nothing to do?
    if ( HarmonicSearchMode == HarmonicSearch::NoAdditions )
    {
        return;
    }

    // simple CW / CCW on the circle of fifths
    if ( HarmonicSearchMode == HarmonicSearch::BasicAdjacent )
    {
        results.pairs.emplace_back( cRoot_CoT_CW[rootScale.root], rootScale.scale );
        results.pairs.emplace_back( cRoot_CoT_CCW[rootScale.root], rootScale.scale );

        return;
    }

    // NO RULES is handled at the query point, we don't do anything here
    if ( HarmonicSearchMode == HarmonicSearch::NoRules )
    {
        results.pairs.clear();
        return;
    }

    const bool addCloselyRelated =
        ( HarmonicSearchMode == HarmonicSearch::CloselyRelated             ) ||
        ( HarmonicSearchMode == HarmonicSearch::RelatedAndInteresting      ) ||
        ( HarmonicSearchMode == HarmonicSearch::RelatedAndInterestingExtra );

    const bool addInteresting =
        ( HarmonicSearchMode == HarmonicSearch::RelatedAndInteresting      ) ||
        ( HarmonicSearchMode == HarmonicSearch::RelatedAndInterestingExtra );

    const bool addExtras =
        ( HarmonicSearchMode == HarmonicSearch::RelatedAndInterestingExtra );

#define IncWrapRoot( _v )   (rootScale.root + _v) % 12

    switch ( rootScale.scale )
    {
        case 0: // Major (Ionian)
        {
            if ( addCloselyRelated )
            {
                const int32_t minorKey = IncWrapRoot( 9 );              // C   -> Am
                results.pairs.emplace_back( minorKey, ES_MinorAeolian );

                const int32_t closeKey1 = IncWrapRoot( 5 );             // C   -> F
                results.pairs.emplace_back( closeKey1, ES_MajorIonian );

                const int32_t closeKey2 = IncWrapRoot( 7 );             // C   -> G
                results.pairs.emplace_back( closeKey2, ES_Mixolydian );
            }
            if ( addInteresting )
            {
                const int32_t minorInt1 = IncWrapRoot( 2 );             // C   -> Dm
                results.pairs.emplace_back( minorInt1, ES_Dorian );

                const int32_t minorInt2 = IncWrapRoot( 4 );             // C   -> Em
                results.pairs.emplace_back( minorInt2, ES_Phrygian );

                const int32_t majorInt = IncWrapRoot( 10 );              // C   -> Bb
                results.pairs.emplace_back( majorInt, ES_MajorIonian );
            }
            if ( addExtras )
            {
                // C D E F# G# A#
                if ( ( rootScale.root % 2 ) == 0 )
                    results.pairs.emplace_back( rootScale.root, ES_WholeTone );
            }
        }
        break;

        case 1: // Dorian
        {
            if ( addCloselyRelated )
            {
                const int32_t closeKey1 = IncWrapRoot( 7 );             // C   -> Gm
                results.pairs.emplace_back( closeKey1, ES_MinorAeolian );

                const int32_t closeKey2 = IncWrapRoot( 9 );             // C   -> Am
                results.pairs.emplace_back( closeKey2, ES_Dorian );

                const int32_t closeKey3 = IncWrapRoot( 5 );             // C   -> F
                results.pairs.emplace_back( closeKey3, ES_MajorIonian );
            }
            if ( addInteresting )
            {
                const int32_t interKey1 = IncWrapRoot( 10 );            // C   -> Bb
                results.pairs.emplace_back( interKey1, ES_Dorian );

                const int32_t interKey2 = IncWrapRoot( 4 );             // C   -> Em
                results.pairs.emplace_back( interKey2, ES_Phrygian );

                const int32_t interKey3 = IncWrapRoot( 8 );             // C   -> Ab
                results.pairs.emplace_back( interKey3, ES_MajorIonian );
            }
        }
        break;

        case 2: // Phrygian
        {
            if ( addCloselyRelated )
            {
                const int32_t closeKey1 = IncWrapRoot( 9 );             // C   -> Am
                results.pairs.emplace_back( closeKey1, ES_MinorAeolian );

                const int32_t closeKey2 = IncWrapRoot( 11 );            // C   -> Bm
                results.pairs.emplace_back( closeKey2, ES_Phrygian );

                const int32_t closeKey3 = IncWrapRoot( 7 );             // C   -> G
                results.pairs.emplace_back( closeKey3, ES_MajorIonian );
            }
            if ( addInteresting )
            {
                const int32_t interKey1 = IncWrapRoot( 3 );             // C   -> Eb
                results.pairs.emplace_back( interKey1, ES_Phrygian );

                const int32_t interKey2 = IncWrapRoot( 5 );             // C   -> Fm
                results.pairs.emplace_back( interKey2, ES_Phrygian );

                const int32_t interKey3 = IncWrapRoot( 1 );             // C   -> Db
                results.pairs.emplace_back( interKey3, ES_MajorIonian );
            }
        }
        break;

        case 3: // Lydian
        {
            if ( addCloselyRelated )
            {
                const int32_t closeKey1 = IncWrapRoot( 6 );             // C   -> F#m
                results.pairs.emplace_back( closeKey1, ES_MinorAeolian );

                const int32_t closeKey2 = IncWrapRoot( 7 );             // C   -> G
                results.pairs.emplace_back( closeKey2, ES_MajorIonian );

                const int32_t closeKey3 = IncWrapRoot( 5 );             // C   -> F
                results.pairs.emplace_back( closeKey3, ES_Mixolydian );
            }
            if ( addInteresting )
            {
                const int32_t interKey1 = IncWrapRoot( 9 );             // C   -> A
                results.pairs.emplace_back( interKey1, ES_Lydian );

                const int32_t interKey2 = IncWrapRoot( 11 );            // C   -> Bm
                results.pairs.emplace_back( interKey2, ES_Phrygian );

                const int32_t interKey3 = IncWrapRoot( 4 );             // C   -> Em
                results.pairs.emplace_back( interKey3, ES_Phrygian );
            }
        }
        break;

        case 4: // Mixolydian
        {
            if ( addCloselyRelated )
            {
                const int32_t closeKey1 = IncWrapRoot( 5 );             // C   -> Fm
                results.pairs.emplace_back( closeKey1, ES_MinorAeolian );

                const int32_t closeKey2 = IncWrapRoot( 7 );             // C   -> G
                results.pairs.emplace_back( closeKey2, ES_MajorIonian );
            }
            if ( addInteresting )
            {
                const int32_t interKey1 = IncWrapRoot( 10 );            // C   -> Bb
                results.pairs.emplace_back( interKey1, ES_Mixolydian );

                const int32_t interKey2 = IncWrapRoot( 2 );             // C   -> Dm
                results.pairs.emplace_back( interKey2, ES_Dorian );

                const int32_t interKey3 = IncWrapRoot( 3 );             // C   -> Eb
                results.pairs.emplace_back( interKey3, ES_MajorIonian );
            }
        }
        break;

        case 5: // Minor (Aeolian)
        {
            if ( addCloselyRelated )
            {
                const int32_t closeKey1 = IncWrapRoot( 7 );             // C   -> Gm
                results.pairs.emplace_back( closeKey1, ES_MinorAeolian );

                const int32_t closeKey2 = IncWrapRoot( 2 );             // C   -> Dm
                results.pairs.emplace_back( closeKey2, ES_Dorian );
            }
            if ( addInteresting )
            {
                const int32_t interKey1 = IncWrapRoot( 8 );             // C   -> Ab
                results.pairs.emplace_back( interKey1, ES_MinorAeolian );

                const int32_t interKey2 = IncWrapRoot( 4 );             // C   -> Em
                results.pairs.emplace_back( interKey2, ES_Phrygian );

                const int32_t interKey3 = IncWrapRoot( 5 );             // C   -> F
                results.pairs.emplace_back( interKey3, ES_MajorIonian );
            }
        }
        break;

        case 6: // Locrian
        {
            if ( addCloselyRelated )
            {
                const int32_t closeKey1 = IncWrapRoot( 8 );             // C   -> Abm
                results.pairs.emplace_back( closeKey1, ES_MinorAeolian );

                const int32_t closeKey2 = IncWrapRoot( 10 );            // C   -> Bb
                results.pairs.emplace_back( closeKey2, ES_Locrian );
            }
            if ( addInteresting )
            {
                const int32_t interKey1 = IncWrapRoot( 6 );             // C   -> F#
                results.pairs.emplace_back( interKey1, ES_Locrian );

                const int32_t interKey2 = IncWrapRoot( 0 );             // C   -> Cm
                results.pairs.emplace_back( interKey2, ES_Phrygian );

                const int32_t interKey3 = IncWrapRoot( 1 );             // C   -> Db
                results.pairs.emplace_back( interKey3, ES_MajorIonian );
            }
        }
        break;

        case 7: // Minor Pentatonic
        {
            if ( addCloselyRelated )
            {
                const int32_t closeKey1 = IncWrapRoot( 9 );             // C   -> Am
                results.pairs.emplace_back( closeKey1, ES_MinorAeolian );
            }
        }
        break;

        case 8: // Major Pentatonic
        {
            if ( addCloselyRelated )
            {
                const int32_t closeKey1 = IncWrapRoot( 0 );             // C   -> C
                results.pairs.emplace_back( closeKey1, ES_MajorIonian );
            }
        }
        break;

        case 9: // Suspended Pent
        {

        }
        break;

        case 10: // Blues Minor Pent
        {

        }
        break;

        case 11: // Blues Major Pent
        {

        }
        break;

        case 12: // Harmonic Minor
        {
            if ( addCloselyRelated )
            {
                const int32_t closeKey1 = IncWrapRoot( 9 );             // C   -> Am
                results.pairs.emplace_back( closeKey1, ES_MinorAeolian );
            }
        }
        break;

        case 13: // Melodic Minor
        {
            if ( addCloselyRelated )
            {
                const int32_t closeKey1 = IncWrapRoot( 9 );             // C   -> Am
                results.pairs.emplace_back( closeKey1, ES_MinorAeolian );
            }
        }
        break;

        case 14: // Double Harmonic
        {

        }
        break;

        case 15: // Blues
        {
            if ( addCloselyRelated )
            {
                const int32_t closeKey1 = IncWrapRoot( 0 );             // C   -> C
                results.pairs.emplace_back( closeKey1, ES_MajorIonian );
            }
        }
        break;

        case 16: // Whole Tone
        {

        }
        break;

        case 17: // Chromatic
        {

        }
        break;
    }

#undef IncWrapRoot
}


} // namespace constants
} // namespace endlesss
