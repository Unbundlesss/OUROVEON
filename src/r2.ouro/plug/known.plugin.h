//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  cross platform loading of shared libraries (DLLs on windows etc)
//

#pragma once

#include "base/id.simple.h"

namespace plug {

struct _plugin_system_id {};
using PluginSystemID = base::id::Simple<_plugin_system_id, uint32_t, 1, 0>;

struct _plugin_exterior_index {};
using ExteriorIndex = base::id::Simple<_plugin_exterior_index, int64_t, 0, -1>;

struct _known_plugin_index {};
using KnownPluginIndex = base::id::Simple<_known_plugin_index, int64_t, 0, -1>;

struct Systems final
{
    Systems() = delete;
    virtual void _abstract() = 0;

    // known plugin types the system can load
    static constexpr PluginSystemID CLAP{ 0x10 };
};

// a sort of common record for an audio processing plugin, derived mostly from what CLAP provides
struct KnownPlugin
{
    using Instance = std::unique_ptr< KnownPlugin >;

    enum SupportFlags
    {
        SF_ExplicitStereoSupport    = 1 << 0,       // plugin declared stereo support in the clap features fields
        SF_ExplicitMonoSupport      = 1 << 1,       // plugin declares mono support in the clap features fields
    };

    KnownPlugin() = delete;
    KnownPlugin( const PluginSystemID& systemID )
        : m_systemID( systemID )
    {}

    // system ID denotes which known plugin format is in use, eg. CLAP
    PluginSystemID  m_systemID = PluginSystemID::invalid();

    fs::path        m_fullLibraryPath;          // path to the original plugin library
    ExteriorIndex   m_exteriorIndex;            // index from the owning stash, eg. index into a list of plugin pathnames
    uint32_t        m_interiorIndex = 0;        // index inside the plugin library, for libraries that contain multiple plugins; otherwise just 0

    std::string     m_uid;                      // unique plugin identifier, used to refer to the plugin in serialisation
    std::string     m_name;                     // public name
    std::string     m_vendor;                   // vendor
    std::string     m_version;                  // version string
    std::string     m_sortable;                 // combination of vendor/name etc to sort on

    uint32_t        m_flags = 0;                // combination of SupportFlags
};

} // namespace plug
