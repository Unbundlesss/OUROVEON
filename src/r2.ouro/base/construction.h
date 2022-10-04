//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//

#pragma once

namespace base {

// from the epic thread
// https://stackoverflow.com/questions/8147027/how-do-i-call-stdmake-shared-on-a-class-with-only-protected-or-private-const
//
// allows use of make_shared with types with non-public ctors
//
template < typename Object, typename... Args >
inline std::shared_ptr< Object >
protected_make_shared( Args&&... args )
{
    struct helper : public Object
    {
        helper( Args&&... args )
            : Object{ std::forward< Args >( args )... }
        {}
    };

    return std::make_shared< helper >( std::forward< Args >( args )... );
}

} // namespace base

#define DECLARE_NO_COPY( _classname )                           \
    _classname & operator=(const _classname&) = delete;         \
    _classname( const _classname& ) = delete                    

#define DECLARE_NO_MOVE( _classname )                           \
    _classname(_classname&& a) = delete;                        \
    _classname& operator=(_classname&& a) = delete

#define DECLARE_NO_COPY_NO_MOVE( _classname )                   \
    _classname & operator=(const _classname&) = delete;         \
    _classname( const _classname& ) = delete;                   \
    _classname(_classname&& a) = delete;                        \
    _classname& operator=(_classname&& a) = delete

