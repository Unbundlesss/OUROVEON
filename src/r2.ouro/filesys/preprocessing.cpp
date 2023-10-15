//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#include "pch.h"

#include "filesys/preprocessing.h"

#include "simplecpp.h"

namespace filesys {

// ---------------------------------------------------------------------------------------------------------------------
Preprocessing::Preprocessing()
{
    m_dui = std::make_unique<simplecpp::DUI>();
    m_dui->std = "c++20";
}

// ---------------------------------------------------------------------------------------------------------------------
Preprocessing::~Preprocessing()
{
}

// ---------------------------------------------------------------------------------------------------------------------
absl::Status Preprocessing::processAndAppend( const fs::path& fileIn, std::string& processedResult, bool includeLinePragmas ) const
{
    simplecpp::OutputList outputList;

    std::vector<std::string> files;
    std::ifstream f( fileIn );
    simplecpp::TokenList rawtokens( f, files, fileIn.string(), &outputList );

    std::map<std::string, simplecpp::TokenList*> included = simplecpp::load( rawtokens, files, state(), &outputList );

    simplecpp::TokenList outputTokens( files );
    simplecpp::preprocess( outputTokens, rawtokens, files, included, state(), &outputList );

    processedResult += outputTokens.stringify( includeLinePragmas );

    simplecpp::cleanup( included );

    if ( outputList.empty() )
        return absl::OkStatus();


    std::string errorReport;
    errorReport.reserve( 512 );
    for ( const simplecpp::Output& output : outputList )
    {
        errorReport += fmt::format( FMTX( "{}:{} " ), output.location.file(), output.location.line );

        switch ( output.type )
        {
            case simplecpp::Output::SCPP_ERROR:
                errorReport += "#error: ";
                break;
            case simplecpp::Output::SCPP_WARNING:
                errorReport += "#warning: ";
                break;
            case simplecpp::Output::MISSING_HEADER:
                errorReport += "missing header: ";
                break;
            case simplecpp::Output::INCLUDE_NESTED_TOO_DEEPLY:
                errorReport += "include nested too deeply: ";
                break;
            case simplecpp::Output::SYNTAX_ERROR:
                errorReport += "syntax error: ";
                break;
            case simplecpp::Output::PORTABILITY_BACKSLASH:
                errorReport += "portability: ";
                break;
            case simplecpp::Output::UNHANDLED_CHAR_ERROR:
                errorReport += "unhandled char error: ";
                break;
            case simplecpp::Output::EXPLICIT_INCLUDE_NOT_FOUND:
                errorReport += "explicit include not found: ";
                break;
        }
        errorReport += output.msg;
        errorReport += "\n";
    }

    return absl::InternalError( errorReport );
}

} // namespace filesys