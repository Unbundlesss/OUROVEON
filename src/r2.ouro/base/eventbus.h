//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  event bus is a simple, multithread-friendly way for app systems to exchange generic messages
//

#pragma once
#include "base/construction.h"
#include "base/hashing.h"
#include "base/id.simple.h"

namespace base {

// ---------------------------------------------------------------------------------------------------------------------
// an event ID is the unique identifier for a defined event; it is designed to be declared as a static constant, eg
//
// constexpr static base::EventID ID = base::EventID( "My Event Name" );
//
// at compile time, the name is hashed to become the unique ID and the pointer stored for debug/log purposes
//
struct EventID
{
    EventID() = delete;
    consteval EventID( const char* eventIdentity )
        : m_name( eventIdentity )
        , m_crc32( compileTimeStringCRC( eventIdentity ) )
    {
    }

    ouro_nodiscard constexpr const char* name() const { return m_name; }
    ouro_nodiscard constexpr uint32_t uid() const { return m_crc32; }
    ouro_nodiscard constexpr bool operator == ( const EventID& rhs ) const { return rhs.m_crc32 == m_crc32; }
    ouro_nodiscard constexpr bool operator != ( const EventID& rhs ) const { return rhs.m_crc32 != m_crc32; }

    // abseil
    template <typename H>
    friend H AbslHashValue( H h, const EventID& m )
    {
        return H::combine( std::move( h ), m.m_crc32 );
    }

private:
    const char* m_name;
    uint32_t    m_crc32;
};

struct _event_listener_id {};
using EventListenerID = base::id::Simple<_event_listener_id, uint32_t, 1, 0>;

// ---------------------------------------------------------------------------------------------------------------------
// base class for an event. inherit off this to define your own (or rather, use CREATE_EVENT_BEGIN / CREATE_EVENT_END)
//
struct IEvent
{
    virtual ~IEvent() {}
    virtual const EventID& getID() const = 0;
};

#define CREATE_EVENT_BEGIN(_evtname)    namespace events {                                                          \
                                        struct _evtname ouro_final : public base::IEvent                            \
                                        {                                                                           \
                                            constexpr static base::EventID ID = base::EventID( #_evtname );         \
                                            const base::EventID& getID() const override { return ID; }

#define CREATE_EVENT_END()              }; }

// register the given event type, reporting to the log if it fails
#define APP_EVENT_REGISTER_SPECIFIC( _evtname, _maxQueuedEvents )                                                               \
        checkedCoreCall( fmt::format( "{{{}}} register [{}]", source_location::current().function_name(), #_evtname), [this]    \
            {                                                                                                                   \
                return m_appEventBus->registerEventID( events::_evtname::ID, sizeof(::events::_evtname), _maxQueuedEvents );    \
            });

#define APP_EVENT_REGISTER( _evtname )  APP_EVENT_REGISTER_SPECIFIC( _evtname, 512 )

#define APP_EVENT_BIND_TO( _eventType )                                                                             \
    m_eventLID_##_eventType = m_eventBusClient.addListener(                                                         \
        events::_eventType::ID,                                                                                     \
        [this]( const base::IEvent& eventPtr )                                                                      \
        {                                                                                                           \
            ABSL_ASSERT( eventPtr.getID() == events::_eventType::ID );                                              \
            const events::_eventType* riffChangeEvent = dynamic_cast<const events::_eventType*>(&eventPtr);         \
            ABSL_ASSERT( riffChangeEvent != nullptr );                                                              \
            event_##_eventType( riffChangeEvent );                                                                  \
        })

#define APP_EVENT_UNBIND( _eventType )                                                                              \
    checkedCoreCall( __FUNCTION__ "|" #_eventType, [this] {                                                         \
        return m_eventBusClient.removeListener( m_eventLID_##_eventType );                                          \
        });


// ---------------------------------------------------------------------------------------------------------------------
// the controlling class for marshalling events
//
struct EventBus
{
    DECLARE_NO_COPY_NO_MOVE( EventBus );

    EventBus();
    ~EventBus();

    using EventListenerFn = std::function< void( const IEvent& ) >;

    // any event ID intending to be used needs to be registered upfront so 
    // that the appropriate data-structures are all prepared in advance;
    // returns false if it was already registered
    absl::Status registerEventID( const EventID& id, const std::size_t eventSize, const std::size_t maxEvents );


    // can send from any thread; returns false if bus is not alive (in process of terminating)
    template< typename _eventType, typename... Args >
    bool send( Args&&... args )
    {
        // push event into lockfree queue
        EventPipe* pipe = getPipeByID( _eventType::ID );
        if ( pipe )
        {
            uint8_t* eventMemoryBlock = nullptr;
            const bool eventMemoryOk = pipe->m_eventMemoryQueue.try_dequeue( eventMemoryBlock );
            ABSL_ASSERT( eventMemoryOk );

            memset( eventMemoryBlock, 0, sizeof( _eventType ) );
            _eventType* eventInstance = new (eventMemoryBlock) _eventType( std::forward<Args>( args )... );

            pipe->m_queue.emplace( eventInstance );
            return true;
        }

        return false;
    }

    // register callback that will be called on main app thread
    ouro_nodiscard EventListenerID addListener( const EventID& id, const EventListenerFn& mainThreadFn );
    absl::Status removeListener( const EventListenerID& listener );

    // call from main thread to pump any waiting messages
    void mainThreadDispatch();

private:

    void flushQueues( bool notifyListeners );

    // marker that denotes the bus is ready for Send()ing on
    std::atomic_bool        m_alive = false;

    // simple counter that hands out new EventListenerIDs
    std::atomic_uint32_t    m_listenerUID = 0;


    using EventQueue   = mcc::ReaderWriterQueue< IEvent* >;
    using ListenerMap  = absl::flat_hash_map< EventListenerID, EventListenerFn >;

    // structure created on Register() to manage a single ID
    // holds all data for event dispatch and is guaranteed to exist until bus dtor
    struct EventPipe
    {
        DECLARE_NO_COPY_NO_MOVE( EventPipe );

        using MemoryBlockQueue = mcc::ConcurrentQueue< uint8_t* >;

        EventPipe() = delete;
        EventPipe( const EventID& id, const std::size_t eventSize, const std::size_t maxEvents );
        ~EventPipe();

        EventID             m_id;
        EventQueue          m_queue;
        ListenerMap         m_listeners;

        MemoryBlockQueue    m_eventMemoryQueue;
        uint8_t*            m_eventMemoryBlock;
    };

    EventPipe* getPipeByID( const EventID& id );

    using EventPipeByID       = absl::flat_hash_map< EventID, EventPipe* >;
    using EventIDByListenerID = absl::flat_hash_map< EventListenerID, EventID >;

    EventPipeByID           m_pipes;
    EventIDByListenerID     m_registeredListenerIDs;

};
using EventBusPtr = std::shared_ptr<EventBus>;
using EventBusWeakPtr = std::weak_ptr<EventBus>;



// ---------------------------------------------------------------------------------------------------------------------
// 'client' wrapper for the bus that only allows for Send'ing, designed for passing around to 
// the various systems that may generate events
struct EventBusClient
{
    EventBusClient() = delete;
    EventBusClient( EventBusPtr hostBus )
        : m_bus( hostBus )
    {}

    // can send from any thread; proxies to EventBus inside and returns true if bus was still alive
    template< typename _eventType, typename... Args >
    bool Send( Args&&... args )
    {
        if ( m_bus.expired() )
            return false;

        return m_bus.lock()->send< _eventType >( std::forward<Args>( args )... );
    }

    ouro_nodiscard EventListenerID addListener( const EventID& id, const EventBus::EventListenerFn& mainThreadFn )
    {
        if ( m_bus.expired() )
            return EventListenerID::invalid();

        return m_bus.lock()->addListener( id, mainThreadFn );
    }

    absl::Status removeListener( const EventListenerID& listener )
    {
        return m_bus.lock()->removeListener( listener );
    }

private:
    EventBusWeakPtr     m_bus;
};

} // namespace base
