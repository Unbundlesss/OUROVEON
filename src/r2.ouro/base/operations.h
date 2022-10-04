//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//

#pragma once

#include "base/id.simple.h"
#include "base/eventbus.h"

namespace base {

struct _operation_id {};
using OperationID = base::id::Simple<_operation_id, uint32_t, 1, 0>;

// ---------------------------------------------------------------------------------------------------------------------
// 
struct Operations
{
    static OperationID newID();
};

// ---------------------------------------------------------------------------------------------------------------------
template< typename _withType >
struct ValueWithOperation
{
    ValueWithOperation()
        : m_operation( OperationID::invalid() )
    {}

    ValueWithOperation( const OperationID& id, _withType& val )
        : m_operation( id )
        , m_value( val )
    {}

    ValueWithOperation( const OperationID& id, const _withType& val )
        : m_operation( id )
        , m_value( val )
    {}

    OperationID         m_operation;
    _withType           m_value;
};

} // namespace base


// ---------------------------------------------------------------------------------------------------------------------
CREATE_EVENT_BEGIN( OperationComplete )

    OperationComplete( const base::OperationID& id )
        : m_id( id )
    {
        ABSL_ASSERT( m_id.isValid() );
    }

    base::OperationID     m_id;

CREATE_EVENT_END()

