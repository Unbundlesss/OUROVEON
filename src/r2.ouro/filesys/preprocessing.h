//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#pragma once

namespace simplecpp { struct DUI; }

namespace filesys {

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

