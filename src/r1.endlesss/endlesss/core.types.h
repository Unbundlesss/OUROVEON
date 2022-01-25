//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  raw basic types for key elements like Riffs and Stems providing common intermediary for systems
//  that want to consume data without needing to know where it came from (eg. network pull, or database query, or )

#pragma once

#include "endlesss/ids.h"


namespace endlesss {
namespace api { struct ResultRiffDocument; struct ResultStemDocument; namespace pull { struct LatestRiffInJam; } }

namespace types {

inline float BPStoRoundedBPM( const double bps )
{
    return (float)( std::ceil( ( bps * 60.0 ) * 100.0 ) / 100.0 );
}

// ---------------------------------------------------------------------------------------------------------------------
struct Jam
{
    Jam() = default;
    Jam( const JamCouchID& jamCouchID, const std::string& jamDisplayName )
        : couchID( jamCouchID )
        , displayName( jamDisplayName )
    {}

    JamCouchID                couchID;
    std::string               displayName;

    template<class Archive>
    inline void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( couchID )
               , CEREAL_NVP( displayName )
        );
    }
};

// ---------------------------------------------------------------------------------------------------------------------
struct Stem
{
    Stem() = default;
    Stem( const JamCouchID& jamCouchID, const endlesss::api::ResultStemDocument& stemFromNetwork );

    StemCouchID     couchID;
    JamCouchID      jamCouchID;             // CID for the jam that owns the stem

    std::string     fileEndpoint;
    std::string     fileBucket;             // optional
    std::string     fileKey;
    std::string     fileMIME;

    uint32_t        fileLengthBytes;
    uint32_t        sampleRate;

    uint64_t        creationTimeUnix;

    std::string     preset;
    std::string     user;
    std::string     colour;                 // hex digit colour encoding

    float           BPS;
    float           BPMrnd;                 // BPS * 60, rounded to nearest 2 decimal places
    float           length16s;
    float           originalPitch;
    float           barLength;

    bool            isDrum = false;
    bool            isNote = false;
    bool            isBass = false;
    bool            isMic = false;

    // take bucket into consideration
    inline std::string fullEndpoint() const
    {
        // majority case
        if ( fileBucket.empty() )
            return fileEndpoint;
        return fmt::format( "{}.{}", fileBucket, fileEndpoint );
    }

    template<class Archive>
    inline void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( couchID )
               , CEREAL_NVP( jamCouchID )
               , CEREAL_NVP( fileEndpoint )
               , CEREAL_NVP( fileBucket )
               , CEREAL_NVP( fileKey )
               , CEREAL_NVP( fileMIME )
               , CEREAL_NVP( fileLengthBytes )
               , CEREAL_NVP( sampleRate )
               , CEREAL_NVP( creationTimeUnix )
               , CEREAL_NVP( preset )
               , CEREAL_NVP( user )
               , CEREAL_NVP( colour )
               , CEREAL_NVP( BPS )
               , CEREAL_NVP( BPMrnd )
               , CEREAL_NVP( length16s )
               , CEREAL_NVP( originalPitch )
               , CEREAL_NVP( barLength )
               , CEREAL_NVP( isDrum )
               , CEREAL_NVP( isNote )
               , CEREAL_NVP( isBass )
               , CEREAL_NVP( isMic )
        );
    }


    enum class InstrumentType
    {
        Drum,
        Note,
        Bass,
        Mic,
        Other
    };

    inline InstrumentType getInstrumentType() const
    {
        if ( isDrum )
            return InstrumentType::Drum;
        if ( isNote )
            return InstrumentType::Note;
        if ( isBass )
            return InstrumentType::Bass;
        if ( isMic )
            return InstrumentType::Mic;

        return InstrumentType::Other;
    }

    inline const char* getInstrumentName() const
    {
        switch ( getInstrumentType() )
        {
        case InstrumentType::Drum: return "DRUM";
        case InstrumentType::Note: return "NOTE";
        case InstrumentType::Bass: return "BASS";
        case InstrumentType::Mic:  return "MIC";
        default:
            return "OTHER";
        }
    }
};

using StemCIDs      = std::array< StemCouchID, 8 >;
using StemGains     = std::array< float, 8 >;
using StemOn        = std::array< bool, 8 >;
using StemArray     = std::array< Stem, 8 >;

// ---------------------------------------------------------------------------------------------------------------------
struct Riff
{
    Riff() = default;
    Riff( const JamCouchID& jamCouchID, const endlesss::api::ResultRiffDocument& riffFromNetwork );

    RiffCouchID     couchID;
    JamCouchID      jamCouchID;             // CID for the jam that owns the riff

    std::string     user;

    StemOn          stemsOn;                // bitfield of stem activation
    StemCIDs        stems;
    StemGains       gains;

    uint64_t        creationTimeUnix;
    uint32_t        root;
    uint32_t        scale;
    float           BPS;
    float           BPMrnd;
    float           barLength;
    uint32_t        appVersion;
    float           magnitude;

    template<class Archive>
    inline void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( couchID )
               , CEREAL_NVP( jamCouchID )
               , CEREAL_NVP( user )
               , CEREAL_NVP( stems )
               , CEREAL_NVP( gains )
               , CEREAL_NVP( creationTimeUnix )
               , CEREAL_NVP( root )
               , CEREAL_NVP( scale )
               , CEREAL_NVP( BPS )
               , CEREAL_NVP( BPMrnd )
               , CEREAL_NVP( barLength )
               , CEREAL_NVP( appVersion )
               , CEREAL_NVP( magnitude )
        );
    }
};

// ---------------------------------------------------------------------------------------------------------------------
struct RiffComplete
{
    RiffComplete() = default;
    RiffComplete( const endlesss::api::pull::LatestRiffInJam& riffDetails );

    Jam         jam;
    Riff        riff;
    StemArray   stems;

    template<class Archive>
    inline void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( jam )
               , CEREAL_NVP( riff )
               , CEREAL_NVP( stems )
        );
    }
};

} // namespace types
} // namespace endlesss
