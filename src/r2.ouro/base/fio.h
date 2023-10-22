//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  file IO operations for bulk loading binary or text off disk
//

#pragma once

namespace base
{
    constexpr auto cFileBufferInlineCapacity = 4 * 1024;
    using TBinaryFileBuffer = absl::InlinedVector< std::byte, cFileBufferInlineCapacity >;
    using TTextFileBuffer = absl::InlinedVector< char, cFileBufferInlineCapacity >;

    // read a file returning the contents by value as a binary buffer
    absl::Status readBinaryFile( const fs::path& path, TBinaryFileBuffer& contents );

    // read a file returning the contents as a binary buffer
    absl::StatusOr< TBinaryFileBuffer > readBinaryFile( const fs::path& path );

    // read a file returning the contents by value as a text buffer
    absl::Status readTextFile( const fs::path& path, TTextFileBuffer& contents );

    // read a file returning the contents as a text buffer
    absl::StatusOr< TTextFileBuffer > readTextFile( const fs::path& path );

} // namespace base
