//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  

#include "pch.h"

#include "base/operations.h"

namespace base {

using OperationIDs = mcc::ReaderWriterQueue<OperationID>;

std::unique_ptr< OperationIDs >     gOperationIDCache;
uint32_t                            gOperationIDCounter;
std::mutex                          gOperationIDCacheFillLock;

void OperationsFill( std::size_t count )
{
    std::scoped_lock<std::mutex> fillLock( gOperationIDCacheFillLock );
    for ( auto i = 0; i < count; i++ )
    {
        gOperationIDCache->emplace( gOperationIDCounter++ );
    }
}

// ---------------------------------------------------------------------------------------------------------------------

// private call from app boot-up to one-time initialise the shared ID counter
void OperationsInit()
{
    gOperationIDCache   = std::make_unique<OperationIDs>();
    gOperationIDCounter = OperationID::defaultValue();

    OperationsFill( 1024 );
}
// .. and for symmetry
void OperationsTerm()
{
    gOperationIDCache = nullptr;
}

// ---------------------------------------------------------------------------------------------------------------------
OperationID Operations::newID()
{
    ABSL_ASSERT( gOperationIDCache != nullptr );

    // in most cases, this should be a fast & lockfree result; if we just ran out, lock and refill
    // potentially multiple threads could enqueue a refill if they all fail at the same time which is 
    // still technically fine
    OperationID result;
    while ( !gOperationIDCache->try_dequeue( result ) )
    {
        OperationsFill( 256 );
    }
    return result;
}

} // namespace base