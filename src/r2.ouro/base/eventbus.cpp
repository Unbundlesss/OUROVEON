//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  

#include "pch.h"

#include "base/eventbus.h"


namespace base {

// ---------------------------------------------------------------------------------------------------------------------
EventBus::EventBus()
{
    m_pipes.reserve( 32 );
    m_alive = true;
}

// ---------------------------------------------------------------------------------------------------------------------
EventBus::~EventBus()
{
    m_alive = false;

    // chew through any events in the pipe but don't notify listeners
    flushQueues( false );

    // rip down pipes
    for ( auto kv : m_pipes )
    {
        delete kv.second;
    }

    m_pipes.clear();
}

// ---------------------------------------------------------------------------------------------------------------------
absl::Status EventBus::registerEventID( const EventID& id, const std::size_t eventSize, const std::size_t maxEvents )
{
    if ( m_pipes.contains( id ) )
    {
        return absl::AlreadyExistsError(
            fmt::format( FMTX( "EventBus::Register with multiple ids ({}, [{}])" ), id.name(), id.uid() ) );
    }

    EventPipe* pipe = new EventPipe( id, eventSize, maxEvents );

    m_pipes.emplace( id, pipe );
    return absl::OkStatus();
}

// ---------------------------------------------------------------------------------------------------------------------
EventListenerID EventBus::addListener( const EventID& id, const EventListenerFn& mainThreadFn )
{
    EventPipe* pipe = getPipeByID( id );
    if ( pipe )
    {
        EventListenerID newListenerID = EventListenerID( m_listenerUID++ );
        pipe->m_listeners.emplace( newListenerID, mainThreadFn );
        m_registeredListenerIDs.emplace( newListenerID, id );
        return newListenerID;
    }
    return EventListenerID::invalid();
}

// ---------------------------------------------------------------------------------------------------------------------
absl::Status EventBus::removeListener( const EventListenerID& listener )
{
    auto it = m_registeredListenerIDs.find( listener );
    if ( it == m_registeredListenerIDs.end() )
        return absl::NotFoundError( "EventBus::RemoveListener - no registered listener found" );

    EventID listenerEventID = it->second;
    EventPipe* pipe = getPipeByID( listenerEventID );
    if ( !pipe )
    {
        return absl::NotFoundError( fmt::format( FMTX( "EventBus::RemoveListener - event ID [{}] not found" ), listenerEventID.name() ) );
    }

    pipe->m_listeners.erase( listener );
    m_registeredListenerIDs.erase( listener );

    return absl::OkStatus();
}

// ---------------------------------------------------------------------------------------------------------------------
void EventBus::mainThreadDispatch()
{
    flushQueues( true );
}

// ---------------------------------------------------------------------------------------------------------------------
void EventBus::flushQueues( bool notifyListeners )
{
    for ( auto kv : m_pipes )
    {
        EventPipe* pipe = kv.second;

        IEvent* eventInstance;
        while ( pipe->m_queue.try_dequeue( eventInstance ) )
        {
            if ( notifyListeners )
            {
                for ( auto lst : pipe->m_listeners )
                {
                    lst.second( *eventInstance );
                }
            }
            eventInstance->~IEvent();
            pipe->m_eventMemoryQueue.enqueue( (uint8_t*)eventInstance );
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
base::EventBus::EventPipe* EventBus::getPipeByID( const EventID& id )
{
    if ( !m_alive )
        return nullptr;

    auto it = m_pipes.find( id );

    // assuming it was registered properly, this shouldn't fail - if it breaks, check the event ID has been registered
    ABSL_ASSERT( it != m_pipes.end() );
    if ( it == m_pipes.end() )
        return nullptr;

    EventPipe* pipe = it->second;
    return pipe;
}

// ---------------------------------------------------------------------------------------------------------------------
EventBus::EventPipe::EventPipe( const EventID& id, std::size_t eventSize, std::size_t maxEvents )
    : m_id( id )
    , m_eventMemoryQueue( maxEvents )
{
    m_eventMemoryBlock = mem::alloc16To<uint8_t>( maxEvents * eventSize, 0 );

    // store each of the allocated event-sized lumps in the queue
    uint8_t* blockAddress = m_eventMemoryBlock;
    for ( auto evI = 0; evI < maxEvents; evI++ )
    {
        m_eventMemoryQueue.enqueue( blockAddress );
        blockAddress += eventSize;
    }

    blog::core( FMTX( "allocated event pipe [{}] {} Kb" ), id.name(), ( maxEvents * eventSize ) / 1024 );
}

// ---------------------------------------------------------------------------------------------------------------------
EventBus::EventPipe::~EventPipe()
{
    mem::free16( m_eventMemoryBlock );
}

} // namespace base

