//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
// a tool for generating 'page' management with ImGui - the notion we use for having a single tab panel have multiple
// switchable views / pages. this little macro nightmare wraps up the safely-typed per-page IDs, a "move to next"
// function that wraps around and a title text generator that produces a consistent ImGui name+internal-ID string
// 
// first define the states as a meta macro
// 
//      #define _JAM_VIEW_STATES(_action)   \
//            _action(Default)              \
//            _action(Visualisation)
// 
//      DEFINE_PAGE_MANAGER( JamView, ICON_FA_GRIP " Jam View", "jam_view", _JAM_VIEW_STATES );
//
// then use the generated JamView structure to manage the state and launch the UI block
// 
//      JamView jamView( JamView::Default );
// 
//      if ( ImGui::Begin( jamView.generateTitle().c_str() ) ) ...
//
//      if ( ... ) jamView.switchToNextPage(); or
//      jamView.checkForImGuiTabSwitch();
//

#pragma once

#include <string>
#include "base/hashing.h"

namespace base {
namespace detail {

// compile-time hashed string, used as a base for the internal page type in place of a traditional enum
struct PageID
{
    PageID() = delete;
    consteval PageID( const char* eventIdentity ) noexcept
        : m_name( eventIdentity )
        , m_crc32( base::compileTimeStringCRC( eventIdentity ) )
    {
        ABSL_ASSERT( eventIdentity != nullptr );
    }
    constexpr const char* name() const { return m_name; }
    constexpr bool operator == ( const PageID& rhs ) const { return rhs.m_crc32 == m_crc32; }
    constexpr bool operator != ( const PageID& rhs ) const { return rhs.m_crc32 != m_crc32; }
private:
    const char* m_name;
    uint32_t    m_crc32;
};

}

#define _PAGING_PAGE_DEF(_ty)              static constexpr LocalStateID _ty{ #_ty };
#define _PAGING_ACTIVE_ICON(_ty)           ( Value == _ty ? ICON_FC_FILLED_SQUARE : ICON_FC_HOLLOW_SQUARE ),
#define _PAGING_ICON_FMT(_ty)              "{}"
#define _PAGING_NEXT_WRAP(_ty)             { Value = _ty; return; } if ( Value == _ty )
#define _PAGING_PLAIN(_ty)                 _ty,

#define DEFINE_PAGE_MANAGER( _stateType, _publicName, _internalName, _statesFunc )                                  \
        struct _stateType                                                                                           \
        {                                                                                                           \
            struct LocalStateID : public base::detail::PageID {};                                                   \
                                                                                                                    \
            constexpr _stateType() : Value( getFirst() ) {}                                                         \
            constexpr _stateType( const LocalStateID& v ) : Value(v) {}                                             \
                                                                                                                    \
            LocalStateID Value;                                                                                     \
            _statesFunc( _PAGING_PAGE_DEF )                                                                         \
                                                                                                                    \
            ouro_nodiscard constexpr bool operator == ( const LocalStateID& rhs ) const { return Value == rhs; }    \
            ouro_nodiscard constexpr operator const LocalStateID& () const { return Value; }                        \
                                                                                                                    \
            std::string generateTitle() const                                                                       \
            {                                                                                                       \
                return fmt::format( FMTX( _publicName " [" _statesFunc( _PAGING_ICON_FMT ) "]###" _internalName ),  \
                    _statesFunc( _PAGING_ACTIVE_ICON )                                                              \
                    "" );                                                                                           \
            }                                                                                                       \
            constexpr void switchToNextPage()                                                                       \
            {                                                                                                       \
                if ( Value.name() == nullptr )                                                                      \
                _statesFunc( _PAGING_NEXT_WRAP )                                                                    \
                { Value = getFirst(); }                                                                             \
            }                                                                                                       \
            void checkForImGuiTabSwitch()                                                                           \
            {                                                                                                       \
                const bool bSwitchOnBarRClick = ( ImGui::IsItemHovered() && ImGui::IsItemClicked( 1 ) );            \
                const bool bShiftKeys         = ( ImGui::IsKeyPressed( ImGuiKey_PageUp, false) );                   \
                const bool bSwitchOnKeyPress  = ( ImGui::IsWindowHovered( ImGuiHoveredFlags_RootAndChildWindows )   \
                                               && bShiftKeys );                                                     \
                                                                                                                    \
                if ( bSwitchOnBarRClick || bSwitchOnKeyPress )                                                      \
                {                                                                                                   \
                    switchToNextPage();                                                                             \
                }                                                                                                   \
            }                                                                                                       \
            private:                                                                                                \
            static constexpr LocalStateID _flat_order[] = {                                                         \
                _statesFunc( _PAGING_PLAIN )                                                                        \
            };                                                                                                      \
            static constexpr LocalStateID getFirst()                                                                \
            {                                                                                                       \
                return _flat_order[0];                                                                              \
            }                                                                                                       \
        };

} // namespace base
