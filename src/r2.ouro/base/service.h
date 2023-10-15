//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#pragma once

#include "base/construction.h"

namespace base {

struct ServiceToken {};

template< typename _T >
struct ServiceBound
{
    ServiceBound() = delete;

    ServiceBound& operator=(const ServiceBound&) = default;
    ServiceBound( const ServiceBound& )          = default;
    ServiceBound( ServiceBound&& a)              = default;
    ServiceBound& operator=( ServiceBound&& a)   = default;


    constexpr _T* get() const
    {
        // ensure the original object is still alive
        const bool alive = !m_alive.expired();
        ABSL_ASSERT( alive );
        if ( !alive )
            return nullptr;

        return m_instance;
    }
    constexpr _T* operator->()             { return get(); }
    constexpr const _T* operator->() const { return get(); }

private:
    template< typename _TS > friend struct ServiceInstance;

    ServiceBound( _T* inst, const std::shared_ptr<ServiceToken>& alive )
        : m_instance( inst )
        , m_alive( alive )
    {}

    _T*                             m_instance = nullptr;
    std::weak_ptr<ServiceToken>     m_alive;
};


template< typename _T >
struct ServiceInstance
{
    ServiceInstance() = delete;
    DECLARE_NO_COPY( ServiceInstance );
    DECLARE_NO_MOVE( ServiceInstance );

    explicit ServiceInstance( _T* ptr )
        : m_instance( ptr )
        , m_alive( std::make_shared<ServiceToken>() )
    {}

    ~ServiceInstance()
    {
        m_instance = nullptr;
    }

    ServiceBound<_T> makeBound()
    {
        return ServiceBound<_T>( m_instance, m_alive );
    }

private:
    _T* m_instance = nullptr;
    std::shared_ptr<ServiceToken>   m_alive;
};

} // namespace base
