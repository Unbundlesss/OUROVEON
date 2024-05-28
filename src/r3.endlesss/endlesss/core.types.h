//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  raw basic types for key elements like Riffs and Stems providing common intermediary for systems
//  that want to consume data without needing to know where it came from (eg. network pull, or database query, or )

#pragma once

#include "base/eventbus.h"
#include "endlesss/ids.h"
#include "net/uriparse.h"

namespace endlesss {
namespace api {

// ---------------------------------------------------------------------------------------------------------------------
struct IStemAudioFormat
{
    enum class CompressionFormat
    {
        OGG,
        FLAC
    };

    virtual ~IStemAudioFormat() = default;

    virtual std::string_view getEndpoint() const = 0;
    virtual std::string_view getBucket() const { return ""; }   // optional
    virtual std::string_view getKey() const = 0;
    virtual std::string_view getMIME() const = 0;
    virtual int32_t getLength() const = 0;
    virtual CompressionFormat getFormat() const = 0;

#define IStemAudioFormat_DefaultImpl                                            \
    std::string_view getEndpoint() const override { return endpoint; }          \
    std::string_view getKey() const override { return key; }                    \
    std::string_view getMIME() const override { return mime; }                  \
    int32_t getLength() const override { return length; }

};

struct ResultStemDocument
{
    struct CDNAttachments
    {
        struct OGGAudio final : public IStemAudioFormat
        {
            std::string     bucket;                 // in old jams, this could be set (eg. would be "ndls-att0")
            std::string     endpoint;               // eg. "ndls-att0.fra1.digitaloceanspaces.com", 
            std::string     key;                    // eg. "attachments/oggAudio/####/################"
            std::string     url;                    // full URL to the audio data
            std::string     mime = "audio/ogg";     // leave as default; very old jams (/stems) lack MIME data
            int32_t         length = 0;

            // implement IStemAudioFormat
            IStemAudioFormat_DefaultImpl;
            std::string_view getBucket() const override { return bucket; }
            CompressionFormat getFormat() const override { return IStemAudioFormat::CompressionFormat::OGG; }

            // OGGAudio blocks have changed a bit over the lifetime of the app and require the most manual 
            // fix-up and error-correction
            template<class Archive>
            inline void serialize( Archive& archive )
            {
                archive( CEREAL_OPTIONAL_NVP( bucket )
                       , CEREAL_NVP( endpoint )
                       , CEREAL_OPTIONAL_NVP( key )     // in some old jams, key is missing!
                       , CEREAL_NVP( url )
                       , CEREAL_NVP( length )
                );

                // in the case where this data block is just empty, ignore any fix-up
                if ( bucket.empty() &&
                     endpoint.empty() &&
                     key.empty() &&
                     url.empty() &&
                     length == 0 )
                {
                    return;
                }

                // -- old jams can contain weird, badly formatted data : fix it as we go --

                // damage / old data missing "key"; re-derive it from URL
                if ( key.empty() )
                {
                    const base::UriParse parser( url );
                    if ( !parser.isValid() )
                    {
                        blog::error::api( "URL Parse fail : {}", url );
                        throw cereal::Exception( "failed to parse missing key from existing URL" );
                    }

                    key = parser.path().substr(1);   // skip the leading "/"
                    if ( key.empty() )
                    {
                        blog::error::api( "URL Parse fail : {}", url );
                        throw cereal::Exception( "failed to parse missing key from existing URL" );
                    }

                    blog::api( "Fixed missing [key] in ogg data" );
                }

                // some weird batch of data encoded https://<bucket> as a prefix into the endpoint; remove it
                if ( endpoint.rfind( "http", 0 ) == 0 )
                {
                    const std::size_t found = endpoint.find_last_of( '/' );
                    if ( found == 0 || found == std::string::npos )
                    {
                        blog::error::api( "Endpoint fix : {}", endpoint );
                        throw cereal::Exception( "failed to fix invalid endpoint data" );
                    }

                    endpoint = endpoint.substr( found + 1 ); // +1 to skip the /

                    blog::api( "Fixed invalid [endpoint] in ogg data" );
                }
                // check if the bucket is already prepended to the endpoint spec
                if ( !bucket.empty() && endpoint.rfind( bucket, 0 ) == 0 )
                {
                    // remove bucket, the endpoint is already valid
                    bucket = "";

                    blog::api( "Fixed invalid [bucket] in ogg data" );
                }
            }
        } oggAudio;

        struct FLACAudio final : public IStemAudioFormat
        {
            std::string     endpoint;               // eg. "endlesss-dev.fra1.digitaloceanspaces.com", 
            std::string     hash;                   // no idea 
            std::string     key;                    // eg. "attachments/flacAudio/band####/################"
            int32_t         length = 0;
            std::string     mime = "audio/flac";    // as one might expect
            std::string     url;                    // full URL to the audio data

            // implement IStemAudioFormat
            IStemAudioFormat_DefaultImpl;
            CompressionFormat getFormat() const override { return IStemAudioFormat::CompressionFormat::FLAC; }

            template<class Archive>
            inline void serialize( Archive& archive )
            {
                archive( CEREAL_NVP( endpoint )
                       , CEREAL_NVP( key )
                       , CEREAL_NVP( length )
                       , CEREAL_NVP( url )
                );
            }

        } flacAudio;

        template<class Archive>
        inline void serialize( Archive& archive )
        {
            // oggAudiomarked as OPTIONAL as there are very rare times where there seems to just be no ogg data at all
            // .. and in those times, we can leave the ogg structure above barren, the riff resolover can deal with that, 
            // it's braver than cereal's very picky json parsing
            archive( CEREAL_OPTIONAL_NVP( oggAudio )
            // and of course flacAudio is a recent addition for the lossless platform. hopefully at least one of these
            // data blocks is here otherwise we're in trouble
                   , CEREAL_OPTIONAL_NVP( flacAudio )
            );
        }

        // choose a prevailing audio format for this stem, given the deserialised data
        // one of oggData or flacData should have something for us to use...
        const IStemAudioFormat& getAudioFormat() const
        {
            const bool bHasFLAC = (flacAudio.getLength() > 0);
            const endlesss::api::IStemAudioFormat& audioFormat = bHasFLAC ?
                static_cast<const endlesss::api::IStemAudioFormat&>(flacAudio) :
                static_cast<const endlesss::api::IStemAudioFormat&>(oggAudio);

            return audioFormat;
        }

    } cdn_attachments;


    endlesss::types::StemCouchID
                            _id;    // couch uid

    float                   bps;
    float                   length16ths;
    float                   originalPitch;
    float                   barLength;
    std::string             presetName;
    std::string             creatorUserName;
    std::string             primaryColour;
    float                   sampleRate;         // has to be float now as Studio seems to save with decimal precision
    uint64_t                created;
    bool                    isDrum;
    bool                    isNote;
    bool                    isBass;
    bool                    isMic;

    template<class Archive>
    inline void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( _id )
               , CEREAL_NVP( cdn_attachments )
               , CEREAL_NVP( bps )
               , CEREAL_NVP( length16ths )
               , CEREAL_NVP( originalPitch )
               , CEREAL_NVP( barLength )
               , CEREAL_NVP( presetName )
               , CEREAL_NVP( creatorUserName )
               , CEREAL_NVP( primaryColour )
               , CEREAL_NVP( sampleRate )
               , CEREAL_NVP( created )
               , CEREAL_OPTIONAL_NVP( isDrum )
               , CEREAL_OPTIONAL_NVP( isNote )
               , CEREAL_OPTIONAL_NVP( isBass )
               , CEREAL_OPTIONAL_NVP( isMic )
        );
    }
};

struct ResultRiffDocument; 
namespace pull { struct LatestRiffInJam; }

} // namspace api

namespace types {

// we create and store a rounded-to-2-decimal-places BPM value from the BPS, for ease of display and comparison
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

    // this is transient, ouroveon-specific metadata; extra descriptors that can be embedded for use by
    // search / sort / export / etc as required. Example: name of a shared riff
    std::string               description;

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

    uint32_t        fileLengthBytes  = 0;
    uint32_t        sampleRate       = 0;

    uint64_t        creationTimeUnix = 0;

    std::string     preset;
    std::string     user;
    std::string     colour;                 // hex digit colour encoding

    float           BPS              = 0;
    float           BPMrnd           = 0;   // BPS * 60, rounded to nearest 2 decimal places
    float           length16s        = 0;
    float           originalPitch    = 0;
    float           barLength        = 0;

    bool            isDrum           = false;
    bool            isNote           = false;
    bool            isBass           = false;
    bool            isMic            = false;

    // return the endpoint string; take bucket into consideration if present
    // (some old data has bucket specification, some doesn't)
    ouro_nodiscard inline std::string fullEndpoint() const
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

    ouro_nodiscard constexpr InstrumentType getInstrumentType() const
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

    ouro_nodiscard constexpr const char* getInstrumentName() const
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

    // this is transient, ouroveon-specific metadata; extra descriptors that can be embedded for use by
    // search / sort / export / etc as required. Example: tag ordering metadata
    std::string     description;

    // another transient, ouroveon-encoded piece of data - optional URL of a cover image for this riff.
    // prime example being the image uploaded for a shared riff which we might want to display or export alongside the stems
    std::string     attachedImageURL;

    std::string     user;

    StemOn          stemsOn;                // bitfield of stem activation
    StemCIDs        stems;
    StemGains       gains;

    uint64_t        creationTimeUnix    = 0;
    uint32_t        root                = 0;
    uint32_t        scale               = 0;
    float           BPS                 = 0;
    float           BPMrnd              = 0;
    float           barLength           = 0;
    uint32_t        appVersion          = 0;
    float           magnitude           = 0;

    // return vector of stem couch IDs that are 'on'
    // commonly used when fetching stem data in batches; note this doesn't leave
    // gaps in the resulting vector for any 'off' stems, you will need to remap to the
    // appropriate slot by examining IDs (see Pipeline for example)
    ouro_nodiscard inline endlesss::types::StemCouchIDs getActiveStemIDs() const
    {
        endlesss::types::StemCouchIDs result;
        for ( std::size_t stemI = 0; stemI < 8; stemI++ )
        {
            if ( stemsOn[stemI] )
                result.push_back( stems[stemI] );
        }
        return result;
    }

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
// the complete set of metadata describing a riff
//
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

// ---------------------------------------------------------------------------------------------------------------------
// block of custom names that can be slid into live jam/riff structures on resolve; then used for export/etc
struct IdentityCustomNaming
{
    template<class Archive>
    inline void serialize( Archive& archive )
    {
        archive( CEREAL_OPTIONAL_NVP( m_jamDisplayName )
               , CEREAL_OPTIONAL_NVP( m_jamDescription )
               , CEREAL_OPTIONAL_NVP( m_riffDescription )
        );
    }

    std::string     m_jamDisplayName;
    std::string     m_jamDescription;
    std::string     m_riffDescription;
};

// ---------------------------------------------------------------------------------------------------------------------
// presently to "uniquely" identify a riff - to load it from the backend, for example - we need the jam that owns it
// as well as the riff's own Couch ID. Our own Warehouse can use just the riff ID but there isn't an existing endlesss API to look up
// full metadata just on riff ID alone (as far as I know)
//
struct RiffIdentity
{
    RiffIdentity()
    {}

    RiffIdentity(
        endlesss::types::JamCouchID jam,
        endlesss::types::RiffCouchID riff )
        : m_jam( std::move( jam ) )
        , m_riff( std::move( riff ) )
    {}

    // option for passing in additional naming data; used by (for example) shared_riff resolver to encode the 
    // original username that shared the riff, as this is not accessible during network resolve and its something
    // we want to encode for later export purposes
    RiffIdentity(
        endlesss::types::JamCouchID jam,
        endlesss::types::RiffCouchID riff,
        IdentityCustomNaming&& customNaming )
        : m_jam( std::move( jam ) )
        , m_riff( std::move( riff ) )
        , m_customNaming( std::move(customNaming) )
    {}

    template<class Archive>
    inline void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( m_jam )
               , CEREAL_NVP( m_riff )
               , CEREAL_NVP( m_customNaming )
        );
    }

    constexpr bool hasData() const
    {
        return !m_jam.empty() &&
               !m_riff.empty();
    }

    constexpr const endlesss::types::JamCouchID&  getJamID()  const { return m_jam; }
    constexpr const endlesss::types::RiffCouchID& getRiffID() const { return m_riff; }

    // check & access custom name data
    bool hasCustomJamDisplayName() const { return !m_customNaming.m_jamDisplayName.empty(); }
    bool hasCustomJamDescription() const { return !m_customNaming.m_jamDescription.empty(); }
    bool hasCustomRiffDescription() const { return !m_customNaming.m_riffDescription.empty(); }
    
    const IdentityCustomNaming& getCustomNaming() const { return m_customNaming; }

    // controls for setting an image attachment for this riff (eg. shared riff cover images)
    void setAttachedImageURL( std::string_view imageURL ) { m_attachedImage = imageURL; }
    void clearAttachedImageURL() { m_attachedImage.clear(); }
    bool hasAttachedImageURL() const { return !m_attachedImage.empty(); }
    const std::string& getAttachedImageURL() const { return m_attachedImage; }


private:
    endlesss::types::JamCouchID     m_jam;
    endlesss::types::RiffCouchID    m_riff;

    std::string                     m_attachedImage;        // (optional) URL to the image associated with the riff; eg. custom image with a shared riff
    IdentityCustomNaming            m_customNaming;         // (optional) modifications to display names and descriptions, 
                                                            //            usually for refining the export pipeline for shared riffs
};

// ---------------------------------------------------------------------------------------------------------------------
// control how the layers in a riff are played; primarily things like being able to mask out layers, but potentially
// other per-stem-DSP may be useful here
//
struct RiffPlaybackPermutation
{
    constexpr RiffPlaybackPermutation()
    {
        m_layerGainMultiplier.fill( 1.0f );
    }

    template<class Archive>
    inline void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( m_layerGainMultiplier )
        );
    }


    // multiplier for the current layer gain; also for doing mute
    std::array<float, 8>    m_layerGainMultiplier;
};

using RiffPlaybackPermutationOpt = std::optional< RiffPlaybackPermutation >;

// ---------------------------------------------------------------------------------------------------------------------
// a more 'UI focused' playback structure which can be turned into a RiffPlaybackPermutation for use by a mixer or whatnot
//
struct RiffPlaybackAbstraction
{
    enum class Query
    {
        IsMuted,
        IsSolo,
        AnyMuted,
        AnySolo,
    };
    enum class Action
    {
        ToggleMute,
        ToggleSolo,
        ClearAllMute,
        ClearSolo,
    };

    constexpr RiffPlaybackAbstraction()
    {
        m_layerMuted.fill( false );
    }

    constexpr bool query( Query q, const int32_t index )
    {
        switch ( q )
        {
            case Query::IsMuted:
                return isMute( index );

            case Query::IsSolo:
                return isSolo( index );

            case Query::AnyMuted:
                for ( bool mute : m_layerMuted )
                {
                    if ( mute )
                        return true;
                }
                return false;

            case Query::AnySolo:
                return m_layerSoloIndex != -1;
        }

        ABSL_ASSERT( false );
        return false;
    }

    constexpr bool action( Action a, const int32_t index )
    {
        switch ( a )
        {
            case Action::ToggleMute:
                return toggleMute( index );

            case Action::ToggleSolo:
                return toggleSolo( index );

            case Action::ClearAllMute:
                m_layerMuted.fill( false );
                return true;

            case Action::ClearSolo:
                m_layerSoloIndex = -1;
                return true;
        }

        ABSL_ASSERT( false );
        return false;
    }

    constexpr bool toggleSolo( const int32_t index )
    {
        if ( index >= 8 )
            return false;

        if ( m_layerSoloIndex == index )
            m_layerSoloIndex = -1;
        else
            m_layerSoloIndex = index;

        return true;
    }

    constexpr bool toggleMute( const int32_t index )
    {
        if ( index >= 8 )
            return false;

        // don't manually change mutes if we are solo'd on a layer
        if ( anySolo() )
            return false;

        m_layerMuted[index] = !m_layerMuted[index];

        return true;
    }

    constexpr bool isMute( const int32_t index ) const
    {
        if ( index >= 8 )
            return false;

        if ( anySolo() )
        {
            if ( index != m_layerSoloIndex )
                return true;
            return false;
        }

        return m_layerMuted[index];
    }

    constexpr bool isSolo( const int32_t index ) const
    {
        if ( index >= 8 )
            return false;

        return ( index == m_layerSoloIndex );
    }

    constexpr bool anySolo() const
    {
        return ( m_layerSoloIndex >= 0 );
    }

    constexpr RiffPlaybackPermutation asPermutation() const
    {
        RiffPlaybackPermutation result;
        
        if ( anySolo() )
        {
            for ( auto i = 0; i < 8; i++ )
            {
                result.m_layerGainMultiplier[i] = ( i == m_layerSoloIndex ) ? 1.0f : 0.0f;
            }
        }
        else
        {
            for ( auto i = 0; i < 8; i++ )
            {
                result.m_layerGainMultiplier[i] = m_layerMuted[i] ? 0.0f : 1.0f;
            }
        }

        return result;
    }

private:
    std::array<bool, 8>     m_layerMuted;
    int32_t                 m_layerSoloIndex    = -1;
};

// ---------------------------------------------------------------------------------------------------------------------
// a riff tagged by the user, usually via the warehouse system / LORE
//
struct RiffTag
{
    RiffTag() = default;

    RiffTag(
        const endlesss::types::JamCouchID& jam,
        const endlesss::types::RiffCouchID& riff,
        int32_t order,
        uint64_t timestamp,
        int32_t favour,
        std::string_view note )
        : m_jam(jam)
        , m_riff(riff)
        , m_order(order)
        , m_timestamp(timestamp)
        , m_favour(favour)
        , m_note(note)
    {}
    
    endlesss::types::JamCouchID     m_jam;
    endlesss::types::RiffCouchID    m_riff;
    int32_t                         m_order = -1;       // <0 means "append; find the highest current order value and set this to that + N"
    uint64_t                        m_timestamp = 0;    // original timestamp of riff in jam
    int32_t                         m_favour;
    std::string                     m_note;


    template<class Archive>
    inline void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( m_jam )
               , CEREAL_NVP( m_riff )
               , CEREAL_NVP( m_order )
               , CEREAL_NVP( m_timestamp )
               , CEREAL_NVP( m_favour )
               , CEREAL_NVP( m_note )
        );
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// custom riff description, forming a 'virual' riff out of existing stem IDs and whatever manual definition we want;
// this is a communication packet for building them into the Warehouse, see that code for more details
//
struct VirtualRiff
{
    VirtualRiff()
    {
        stemsOn.fill( false );
        gains.fill( 0 );
        stemBarLengths.fill( 4.0f );
    }

    std::string     user;

    StemOn          stemsOn;                // bitfield of stem activation
    StemCIDs        stems;
    StemGains       gains;

    std::array< float, 8 >
                    stemBarLengths;         // used to cache bar lengths

    uint32_t        root                = 0;
    uint32_t        scale               = 0;
    float           barLength           = 0;
    float           BPMrnd              = 0;


    template<class Archive>
    inline void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( user )
               , CEREAL_NVP( stemsOn )
               , CEREAL_NVP( stems )
               , CEREAL_NVP( gains )
               , CEREAL_NVP( stemBarLengths )
               , CEREAL_NVP( root )
               , CEREAL_NVP( scale )
               , CEREAL_NVP( barLength )
               , CEREAL_NVP( BPMrnd )
        );
    }
};

} // namespace types
} // namespace endlesss


// ---------------------------------------------------------------------------------------------------------------------
// enqueue an error popup

CREATE_EVENT_BEGIN( AddErrorPopup )

AddErrorPopup() = delete;

AddErrorPopup( std::string_view title, std::string_view contents )
    : m_title( title )
    , m_contents( contents )
{
}

std::string m_title;
std::string m_contents;

CREATE_EVENT_END()


// ---------------------------------------------------------------------------------------------------------------------
// enqueue a toast notification

CREATE_EVENT_BEGIN( AddToastNotification )

enum Type
{
    Info,
    Error
};

AddToastNotification() = delete;

AddToastNotification( const Type type, std::string_view title, std::string_view contents )
    : m_type( type )
    , m_title( title )
    , m_contents( contents )
{
}

Type        m_type;
std::string m_title;
std::string m_contents;

CREATE_EVENT_END()


// ---------------------------------------------------------------------------------------------------------------------
// ask for the given riff to be enqueued for playback as and when possible

CREATE_EVENT_BEGIN( EnqueueRiffPlayback )

EnqueueRiffPlayback() = delete;

EnqueueRiffPlayback( const endlesss::types::RiffIdentity& identity )
    : m_identity( identity )
{
    ABSL_ASSERT( m_identity.hasData() );
}

EnqueueRiffPlayback( const endlesss::types::JamCouchID& jam, const endlesss::types::RiffCouchID& riff )
    : m_identity( jam, riff )
{
    ABSL_ASSERT( m_identity.hasData() );
}

EnqueueRiffPlayback( const endlesss::types::JamCouchID& jam, const endlesss::types::RiffCouchID& riff, endlesss::types::IdentityCustomNaming&& customNaming )
    : m_identity( jam, riff, std::move( customNaming ) )
{
    ABSL_ASSERT( m_identity.hasData() );
}

endlesss::types::RiffIdentity   m_identity;

CREATE_EVENT_END()

// ---------------------------------------------------------------------------------------------------------------------
// request to add/update or delete the given tag data to data storage / custom user views / etc

CREATE_EVENT_BEGIN( RiffTagAction )

enum class Action
{
    Upsert,
    Remove
};

RiffTagAction() = delete;

RiffTagAction( endlesss::types::RiffTag tag, const Action act )
    : m_tag( std::move( tag ) )
    , m_action( act )
{
}

endlesss::types::RiffTag    m_tag;
Action                      m_action;

CREATE_EVENT_END()

// ---------------------------------------------------------------------------------------------------------------------
// "navigate" to the given riff; ideally load and identiy it in a jam view, for example

CREATE_EVENT_BEGIN( RequestNavigationToRiff )

RequestNavigationToRiff() = delete;

RequestNavigationToRiff( const endlesss::types::RiffIdentity& identity )
    : m_identity( identity )
{
    ABSL_ASSERT( m_identity.hasData() );
}

endlesss::types::RiffIdentity   m_identity;

CREATE_EVENT_END()

// ---------------------------------------------------------------------------------------------------------------------
// start process of sharing the riff to endlesss feed

CREATE_EVENT_BEGIN( RequestToShareRiff )

RequestToShareRiff() = delete;

RequestToShareRiff( const endlesss::types::RiffIdentity& identity )
    : m_identity( identity )
{
    ABSL_ASSERT( m_identity.hasData() );
}

endlesss::types::RiffIdentity   m_identity;

CREATE_EVENT_END()

// ---------------------------------------------------------------------------------------------------------------------
// band name service - in the instance where we find a jam that we cannot display a name for, request a network fetch of 
// metadata to name it; this will later cause a BNSWasUpdated event to be sent if/when the app was able to pull new details.
// these can get grouped together so many requests may eventually only cause a single BNSWasUpdated

CREATE_EVENT_BEGIN( BNSCacheMiss )

BNSCacheMiss() = delete;

BNSCacheMiss( const endlesss::types::JamCouchID& jamID )
    : m_jamID( jamID )
{
}

endlesss::types::JamCouchID     m_jamID;

CREATE_EVENT_END()

// ---------------------------------------------------------------------------------------------------------------------
// band name service - new jam name data is available to anyone that cares about such things

CREATE_EVENT_BEGIN( BNSWasUpdated )

BNSWasUpdated() = delete;

BNSWasUpdated( uint64_t changeIndex )
    : m_changeIndex( changeIndex )
{
}

uint64_t    m_changeIndex;

CREATE_EVENT_END()

// ---------------------------------------------------------------------------------------------------------------------
// some kind of network activity is happening; 0-bytes passed means we're just notifying of some kind of outbound event
// otherwise bytes should contain the amount of data parsed (not including any compression, wire-side);
// may set the failure tag to indicate a request bailed for noting on the UI if required

CREATE_EVENT_BEGIN( NetworkActivity )

NetworkActivity() = delete;

NetworkActivity( std::size_t bytes, bool bFailure = false )
    : m_bytes( bytes )
    , m_bFailure( bFailure )
{
}

static NetworkActivity failure()
{
    return { 0, true };
}

std::size_t    m_bytes;
bool           m_bFailure;

CREATE_EVENT_END()

// ---------------------------------------------------------------------------------------------------------------------
// similar to network activity but just a generic "async tasks are running" pulse to put something on UI to let user know

CREATE_EVENT_BEGIN( AsyncTaskActivity )
CREATE_EVENT_END()
