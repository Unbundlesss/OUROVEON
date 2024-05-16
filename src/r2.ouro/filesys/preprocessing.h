//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  wrapper around the simplecpp preprocessing library
//

#pragma once

namespace simplecpp { struct DUI; }

namespace filesys {

// simplified frontend for loading a text file, preprocessing it using simplecpp and returning the processed result
struct Preprocessing
{
    Preprocessing();
    ~Preprocessing();

    simplecpp::DUI& state()             { return *m_dui; }
    const simplecpp::DUI& state() const { return *m_dui; }

    absl::Status processAndAppend( const fs::path& fileIn, std::string& processedResult, bool includeLinePragmas ) const;

private:
    std::unique_ptr< simplecpp::DUI >  m_dui;
};

} // namespace filesys

