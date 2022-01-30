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
namespace live { struct Riff; using RiffPtr = std::shared_ptr<Riff>; }

// ---------------------------------------------------------------------------------------------------------------------
struct Exchange
{
    // fill in the riff-level details from a live pointer (or mark us as "not live" if the pointer is null)
    static void fillDetailsFromRiff( Exchange& data, const live::RiffPtr& riff, const char* jamName );


    #define _PPCAT_NX(A, B) A ## B
    #define _PPCAT(A, B)    _PPCAT_NX(A, B)
    #define _GLOBAL_NAME "Ouroveon_EXCH"

    // standard global names to use for local IPC exchange of this data
    // this is what you need if you're writing tools to hook into
    static constexpr auto GlobalMapppingNameA   = _GLOBAL_NAME;
    static constexpr auto GlobalMapppingNameW   = _PPCAT( L, _GLOBAL_NAME );
    static constexpr auto GlobalMutexNameA      =  "Global\\Mutex_" _GLOBAL_NAME;
    static constexpr auto GlobalMutexNameW      = L"Global\\Mutex_" _PPCAT( L, _GLOBAL_NAME );

    #undef _GLOBAL_NAME
    #undef _PPCAT
    #undef _PPCAT_NX

    static constexpr size_t MaxJamName          = 32;    // who knows
    static constexpr size_t MaxJammerName       = 32;    // no idea, probably less; also .. utf8??

    // flags to set to denote what fields can be considered valid
    enum
    {
        DataFlags_Empty      = 0,
        DataFlags_Riff       = 1 << 0,          // [R ] .. 
        DataFlags_Playback   = 1 << 1           // [ P] ..
    };

    Exchange()  { clear(); }
    ~Exchange() { clear(); }

    inline void clear()
    {
        // POD
        memset( this, 0, sizeof( Exchange ) );
    }

    inline bool hasNoData() const       { return ( m_dataflags == DataFlags_Empty ); }
    inline bool hasRiffData() const     { return ( m_dataflags & DataFlags_Riff     ) == DataFlags_Riff;     }
    inline bool hasPlaybackData() const { return ( m_dataflags & DataFlags_Playback ) == DataFlags_Playback; }


    uint32_t    m_dataflags;                    //      DataFlags_## declaring what of the following should be valid
    uint32_t    m_dataWriteCounter;             //      incremented from 0 each time block is updated so external apps
                                                //          can have some reference as to when data has changed or not

    char        m_jamName[MaxJamName];          // [R ] which jam we jammin
    uint64_t    m_riffHash;                     // [R ] u64 hash derived from original riff couch ID as some kind of UID

    uint64_t    m_riffTimestamp;                // [R ] sys_clock seconds timestamp of riff submission
    uint32_t    m_riffRoot;                     // [R ]
    uint32_t    m_riffScale;                    // [R ]
    float       m_riffBPM;                      // [R ]
    uint32_t    m_riffBeatSegmentCount;         // [R ] number of ---- ---- ---- ---- bar segments computed for the riff timing values
    uint32_t    m_riffBeatSegmentActive;        // [ P] .. and which one is currently live, based on playback time

    float       m_stemPulse[8];                 // [ P] data per stem, some from endlesss, some computed live
    float       m_stemEnergy[8];                // [ P] 
    float       m_stemGain[8];                  // [ P] 
    uint32_t    m_stemColour[8];                // [ P] 

    float       m_consensusBeat;                // [ P] if a number of stems are all reporting beats at the same time, 
                                                //          we emphasise the fact by tracking a 'consensus beat' value which
                                                //          is pulsed and decayed at the same rate as the stems

    float       m_riffTransition;               // [ P] 0..1 how far through a transition to a new riff we are

    // #HDD TODO encode notion of being in transition to entirely new jam

private:
    uint32_t    m_jammerNameValidBits;          // [R ] bit per jammer name indicating one is present
    // this is gross but marshalling multidim string arrays to C# is sketchy, so here we are
    // use set/getJammerName
    char        m_jammerName1[MaxJammerName];   // [R ] 
    char        m_jammerName2[MaxJammerName];
    char        m_jammerName3[MaxJammerName];
    char        m_jammerName4[MaxJammerName];
    char        m_jammerName5[MaxJammerName];
    char        m_jammerName6[MaxJammerName];
    char        m_jammerName7[MaxJammerName];
    char        m_jammerName8[MaxJammerName];
public:

    inline void setJammerName( const size_t index, const char* text )
    {
        if ( text == nullptr || text[0] == '\0' )
            return;

        switch ( index )
        {
            case 0: strcpy_s( m_jammerName1, text ); m_jammerNameValidBits |= ( 1 << 0 ); break;
            case 1: strcpy_s( m_jammerName2, text ); m_jammerNameValidBits |= ( 1 << 1 ); break;
            case 2: strcpy_s( m_jammerName3, text ); m_jammerNameValidBits |= ( 1 << 2 ); break;
            case 3: strcpy_s( m_jammerName4, text ); m_jammerNameValidBits |= ( 1 << 3 ); break;
            case 4: strcpy_s( m_jammerName5, text ); m_jammerNameValidBits |= ( 1 << 4 ); break;
            case 5: strcpy_s( m_jammerName6, text ); m_jammerNameValidBits |= ( 1 << 5 ); break;
            case 6: strcpy_s( m_jammerName7, text ); m_jammerNameValidBits |= ( 1 << 6 ); break;
            case 7: strcpy_s( m_jammerName8, text ); m_jammerNameValidBits |= ( 1 << 7 ); break;
        }
    }

    inline bool isJammerNameValid( const size_t index ) const 
    {
        return (m_jammerNameValidBits & (1 << index)) != 0;
    }

    inline const char* getJammerName( const size_t index ) const
    {
        switch ( index )
        {
            case 0: return m_jammerName1;
            case 1: return m_jammerName2;
            case 2: return m_jammerName3;
            case 3: return m_jammerName4;
            case 4: return m_jammerName5;
            case 5: return m_jammerName6;
            case 6: return m_jammerName7;
            case 7: return m_jammerName8;
        }
        return nullptr;
    }
};

} // namespace endlesss
