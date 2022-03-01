//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "base/metaenum.h"
#include "endlesss/live.riff.h"

namespace app { struct StoragePaths; }

// ---------------------------------------------------------------------------------------------------------------------
namespace endlesss {
namespace xp {

#define _TOKEN_TOSTR(_ty)                 case _ty: return "[" #_ty "]";
#define _TOKEN_FROMSTR(_ty)               if ( cx_strcmp(str, "[" #_ty "]" ) == 0) return _ty; else
#define _OUTPUT_TOKENS(_action)                                                                \
    _action( Jam_Name )                 /* 'CoolTimesAhoy'      (path sanitised)            */ \
    _action( Jam_UniqueID )             /* b7eca81...           (customisable length)       */ \
    _action( Riff_Timestamp )           /* 20220301.152303      (customisable format)       */ \
    _action( Riff_UniqueID )            /* e50c3be...           (customisable length)       */ \
    _action( Riff_BPM )                 /* '156.3bpm'                                       */ \
    _action( Riff_Root )                /* 'Ab'                                             */ \
    _action( Riff_Scale )               /* 'phrygian'                                       */ \
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
    std::string     riff   = "[Jam_Name]/[Riff_Timestamp]_#[Riff_UniqueID]_[Riff_BPM]_[Riff_Root][Riff_Scale]";
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


enum class ExportMode
{
    DryRun,     // dont export, just report (potential) artefacts in return 
    Stems       // export all stems
};

// run export process, returns the files that are created (or would be created, in DryRun mode)
std::vector<fs::path> exportRiff(
    const ExportMode                exportMode,         // the plan
    const app::StoragePaths&        storagePaths,       // where it's going
    const OutputSpec&               outputSpec,         // how it's going
    const endlesss::live::RiffPtr&  riffPtr );          // the what

} // namespace xp
} // namespace endlesss


namespace config {
namespace endlesss {

// ---------------------------------------------------------------------------------------------------------------------
struct Export : public Base
{
    // data routing
    static constexpr auto StoragePath       = IPathProvider::PathFor::SharedConfig;
    static constexpr auto StorageFilename   = "stem.export.json";

    ::endlesss::xp::OutputSpec    spec;

    template<class Archive>
    void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( spec )
        );
    }
};

} // namespace endlesss
} // namespace config
    