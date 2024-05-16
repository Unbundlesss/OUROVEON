//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  utilities for working with tar archives
//

#pragma once

namespace io {

using ArchiveProgressCallback = std::function< void( const std::size_t bytesProcessed, const std::size_t filesProcessed ) >;

// walk through all files & directories in inputPath, pack them in order into the tar file specified by outputTarFile
absl::Status archiveFilesInDirectoryToTAR(
    const std::filesystem::path& inputPath,
    const std::filesystem::path& outputTarFile,
    const ArchiveProgressCallback& archivingProgressFunction
);

// load and extract all files and directories in inputTarFile into the root directory specified in outputPath
absl::Status unarchiveTARIntoDirectory(
    const std::filesystem::path& inputTarFile,
    const std::filesystem::path& outputPath,
    const ArchiveProgressCallback& archivingProgressFunction
);

} // namespace io
