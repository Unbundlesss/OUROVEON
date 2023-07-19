//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//

#pragma once

#include "base/id.couch.h"

// ---------------------------------------------------------------------------------------------------------------------
namespace endlesss {
namespace types {

struct  _jam_couch_tag {};
struct _riff_couch_tag {};
struct _stem_couch_tag {};
struct _shared_couch_tag {};

using JamCouchID        = base::id::StringWrapper<_jam_couch_tag>;
using RiffCouchID       = base::id::StringWrapper<_riff_couch_tag>;
using StemCouchID       = base::id::StringWrapper<_stem_couch_tag>;
using SharedRiffCouchID = base::id::StringWrapper<_shared_couch_tag>;

using JamCouchIDs    = std::vector< JamCouchID >;
using RiffCouchIDs   = std::vector< RiffCouchID >;
using StemCouchIDs   = std::vector< StemCouchID >;

using JamCouchIDSet  = absl::flat_hash_set< JamCouchID >;
using RiffCouchIDSet = absl::flat_hash_set< RiffCouchID >;
using StemCouchIDSet = absl::flat_hash_set< StemCouchID >;

using JamIDToNameMap = absl::flat_hash_map< types::JamCouchID, std::string >;

struct Constants
{
    static inline JamCouchID SharedRiffJam() { return JamCouchID{ "shared_riff" }; }

    // dumb check to see if a given ID begins with 'band', marking it as the usual kind of ID we deal with
    // alternatives would be a username, or .. i dunno
    static inline bool isStandardJamID( const JamCouchID& jamID )
    {
        if ( jamID.size() > 4 )
        {
            const char* jamChars = jamID.c_str();
            if ( jamChars[0] == 'b' &&
                 jamChars[1] == 'a' &&
                 jamChars[2] == 'n' &&
                 jamChars[3] == 'd' )
            {
                return true;
            }
        }
        return false;
    }
};

} // namespace types
} // namespace endlesss

Gen_StringWrapperFormatter( endlesss::types::JamCouchID )
Gen_StringWrapperFormatter( endlesss::types::RiffCouchID )
Gen_StringWrapperFormatter( endlesss::types::StemCouchID )
Gen_StringWrapperFormatter( endlesss::types::SharedRiffCouchID )
