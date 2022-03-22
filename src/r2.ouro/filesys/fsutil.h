//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

namespace filesys {

bool recursiveSearchBackwards( const char* filenameToFind, fs::path& resultPath );

// TODO absl
bool ensureDirectoryExists( const fs::path& path );

bool appendAndCreateSubDir( fs::path& path, const char* suffix );

} // namespace filesys

