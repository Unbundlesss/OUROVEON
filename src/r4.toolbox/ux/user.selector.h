//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//

#include "base/utils.h"
#include "endlesss/core.types.h"
#include "endlesss/toolkit.population.h"

namespace ImGui {
namespace ux {

struct UserSelector
{
    UserSelector() = default;
    UserSelector( const std::string_view defaultUsername )
        : m_username( defaultUsername )
    {}

    void imgui( const endlesss::toolkit::PopulationQuery& population, float itemWidth = -1.0f );

    ouro_nodiscard const std::string& getUsername() const { return m_username; }
    void setUsername( const std::string_view username ) { m_username = username; }


private:

    using PopulationAutocomplete = endlesss::toolkit::PopulationQuery::Result;

    int32_t                     m_suggestionIndex = -1;
    std::string                 m_username;
    PopulationAutocomplete      m_autocompletion;
};

} // namespace ux
} // namespace ImGui
