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
#include "base/eventbus.h"

#include "endlesss/live.riff.h"


namespace app { struct StoragePaths; }

// ---------------------------------------------------------------------------------------------------------------------
namespace endlesss {
namespace toolkit {
namespace xp {

#define _TOKEN_TOSTR(_ty)                 case _ty: return "[" #_ty "]";
#define _TOKEN_FROMSTR(_ty)               if ( const_str::compare(str, "[" #_ty "]" ) == 0) return _ty; else
#define _OUTPUT_TOKENS(_action)                                                                \
    _action( Jam_Name )                 /* 'CoolTimesAhoy'      (path sanitised)            */ \
    _action( Jam_UniqueID )             /* b7eca81...           (customisable length)       */ \
    _action( Jam_Description )          /* 'extra_context'      (optional, path sanitised)  */ \
    _action( Riff_Timestamp )           /* 20220301.152303      (customisable format)       */ \
    _action( Riff_UniqueID )            /* e50c3be...           (customisable length)       */ \
    _action( Riff_BPM )                 /* '156.3bpm'                                       */ \
    _action( Riff_Root )                /* 'Ab'                                             */ \
    _action( Riff_Scale )               /* 'phrygian'                                       */ \
    _action( Riff_Description )         /* 'tag1'               (optional, path sanitised)  */ \
    _action( Stem_Index )               /* '1'                                              */ \
    _action( Stem_Timestamp )           /* 20220301.152218      (customisable format)       */ \
    _action( Stem_Author )              /* 'ishaniii'                                       */ \
    _action( Stem_Preset )              /* 'mainline'                                       */ \
    _action( Stem_UniqueID )            /* 82c1572...           (customisable length)       */

REFLECT_ENUM_CUSTOM_STRCONV( OutputTokens, uint32_t, _OUTPUT_TOKENS, _TOKEN_TOSTR, _TOKEN_FROMSTR );
#undef _OUTPUT_TOKENS


// specific configurable options relating to token expansion
struct Customisations
{
    std::string         timestampFormatRiff     = "%Y%m%d.%H%M%S";
    std::string         timestampFormatStem     = "%Y%m%d.%H%M%S";
    uint32_t            uniqueIDLength          = 5;

    // jam description data is optional and may not be present for all exports;
    // if it is, specify a suffix to automatically add - can be a path split '/', underscore, whatever
    std::string         jamDescriptionSeparator = "/";

    template<class Archive>
    void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( timestampFormatRiff )
               , CEREAL_NVP( timestampFormatStem )
               , CEREAL_NVP( uniqueIDLength )
        );
    }
};

enum class AudioFormat
{
    FLAC,
    WAV
};

struct OutputSpec
{
    AudioFormat     format = AudioFormat::WAV;
    std::string     riff   = "[Jam_Name]/[Jam_Description][Riff_Description][Riff_Timestamp]_#[Riff_UniqueID]_[Riff_BPM]_[Riff_Root][Riff_Scale]";
    std::string     stem   = "[Stem_Index]-[Stem_Timestamp]-#[Stem_UniqueID]-[Stem_Author]";

    Customisations  custom;

    template<class Archive>
    void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( format )
               , CEREAL_NVP( riff )
               , CEREAL_NVP( stem )
               , CEREAL_NVP( custom )
        );
    }
};

// options passed through to export function that contain any (optional) tuning or adjustments to the process
// this is set and sent by the initiator of the export
struct RiffExportAdjustments
{
    // sample offset applied to all stems on export to re-one 
    int32_t     m_exportSampleOffset = 0;
};

enum class RiffExportMode
{
    DryRun,     // dont export, just report (potential) artefacts
    Stems       // export all stems
};

struct RiffExportDestination
{
    RiffExportDestination( const app::StoragePaths& paths, const OutputSpec& spec )
        : m_paths( paths )
        , m_spec( spec )
    {}

    const app::StoragePaths&        m_paths;
    const OutputSpec&               m_spec;
};

// run export process, returns the files that are created (or would be created, in DryRun mode)
std::vector<fs::path> exportRiff(
    endlesss::api::NetConfiguration&    netCfg,             // network access, if required for any external assets
    const RiffExportMode                exportMode,         // the plan
    const RiffExportDestination&        destination,        // where & how it's going
    const RiffExportAdjustments&        adjustments,        // anything else to do to it
    const endlesss::live::RiffPtr&      riffPtr );          // the what

} // namespace xp
} // namespace toolkit
} // namespace endlesss


// ---------------------------------------------------------------------------------------------------------------------
CREATE_EVENT_BEGIN( ExportRiff )

    ExportRiff( endlesss::live::RiffPtr& riff, const endlesss::toolkit::xp::RiffExportAdjustments& adjustments )
        : m_riff( riff )
        , m_adjustments( adjustments )
    {}

    endlesss::live::RiffPtr                         m_riff;
    endlesss::toolkit::xp::RiffExportAdjustments    m_adjustments;

CREATE_EVENT_END()


namespace config {
namespace endlesss {

// ---------------------------------------------------------------------------------------------------------------------
OURO_CONFIG( Export )
{
    // data routing
    static constexpr auto StoragePath       = IPathProvider::PathFor::SharedConfig;
    static constexpr auto StorageFilename   = "stem.export.json";

    ::endlesss::toolkit::xp::OutputSpec    spec;

    template<class Archive>
    void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( spec )
        );
    }
};

} // namespace endlesss
} // namespace config
