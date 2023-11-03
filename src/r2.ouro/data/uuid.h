//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  bespoke, compact generator for UUID v1 / rev A .. more or less
//  https://www.uuidtools.com/uuid-versions-explained
//  https://www.rfc-editor.org/rfc/rfc4122
//

#pragma once

namespace data {

    std::string generateUUID_V1( bool withHypens );

} // namespace data
