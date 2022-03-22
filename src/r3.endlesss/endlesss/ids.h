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
using JamCouchID     = base::id::StringWrapper<_jam_couch_tag>;
using RiffCouchID    = base::id::StringWrapper<_riff_couch_tag>;
using StemCouchID    = base::id::StringWrapper<_stem_couch_tag>;

using JamCouchIDs    = std::vector< JamCouchID >;
using RiffCouchIDs   = std::vector< RiffCouchID >;
using StemCouchIDs   = std::vector< StemCouchID >;

using JamCouchIDSet  = absl::flat_hash_set< JamCouchID >;
using RiffCouchIDSet = absl::flat_hash_set< RiffCouchID >;
using StemCouchIDSet = absl::flat_hash_set< StemCouchID >;

} // namespace types
} // namespace endlesss

Gen_StringWrapperFormatter( endlesss::types::JamCouchID )
Gen_StringWrapperFormatter( endlesss::types::RiffCouchID )
Gen_StringWrapperFormatter( endlesss::types::StemCouchID )

