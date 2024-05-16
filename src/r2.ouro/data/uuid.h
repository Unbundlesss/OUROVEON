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
//  example, either with or without hyphens
// 
//  71ba9e6b-04e0-11ef-b5e1-8637b9061254
//  716682d704e011efb29c3e1c25946fb1
//

#pragma once

namespace data {

    // produce a UUID of a similar form to the ones Endlesss use, with the distinction of 
    // using "B" variant instead of "A" in the clock sequence so it's very clear when we 
    // encounter a OUROVEON generated one
    std::string generateUUID_V1( bool bWithHyphens );

} // namespace data
