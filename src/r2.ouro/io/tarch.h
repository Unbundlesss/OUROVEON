//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#pragma once

namespace io {

using ArchiveProgressCallback = std::function< void( const std::size_t bytesProcessed, const std::size_t filesProcessed ) >;

// 
absl::Status archiveFilesInDirectoryToTAR(
    const std::filesystem::path& inputPath,
    const std::filesystem::path& outputTarFile,
    const ArchiveProgressCallback& archivingProgressFunction
);

//
absl::Status unarchiveTARIntoDirectory(
    const std::filesystem::path& inputTarFile,
    const std::filesystem::path& outputPath,
    const ArchiveProgressCallback& archivingProgressFunction
);

} // namespace io
