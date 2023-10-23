//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#include "pch.h"

#include "gfx/sketchbook.h"


namespace gfx {

SketchBuffer::SketchBuffer( std::weak_ptr< Sketchbook > sketchbook, const DimensionsPow2& dimensions ) 
    : m_dimensions( dimensions )
    , m_usageExtents( dimensions.width(), dimensions.height() )
    , m_buffer( nullptr )
    , m_sketchbook( std::move( sketchbook ) )
{
    auto mgr = m_sketchbook.lock();
    ABSL_ASSERT( mgr != nullptr );
    if ( mgr != nullptr )
        m_buffer = mgr->borrow( dimensions );
}

SketchBuffer::~SketchBuffer()
{
    if ( auto sbk = m_sketchbook.lock() )
    {
        sbk->recycle( m_buffer );
    }
    else
    {
        // this indicates we have objects holding onto buffers beyond Sketchbook shut-down, which is not valid;
        // check that client code is correctly terminated in the right order
        ABSL_ASSERT( false );
    }
    m_buffer = nullptr;
}

SketchUpload::SketchUpload(
    std::weak_ptr< Sketchbook > sketchbook,
    const uint32_t      uploadID,
    const GPUTask*      gpuTask,
    const Dimensions&   bounds )
    : m_uploadID( uploadID )
    , m_bounds( bounds )
    , m_gpuTask( gpuTask )
    , m_sketchbook( std::move( sketchbook ) )
{
}

SketchUpload::~SketchUpload()
{
    if ( auto sbk = m_sketchbook.lock() )
    {
        // let sketchbook know this gpu task is no longer in scope, discard it when viable
        sbk->retire( m_gpuTask );
    }
    else
    {
        // as with SketchBuffer, we are expecting the issuing Sketchbook to remain valid
        // for longer than these distributed tokens. if the lock() fails, the link has expired
        ABSL_ASSERT( false );
    }
}



Sketchbook::Sketchbook()
    : m_uploadCounter( 0 )
    , m_lifecycle( this, []( Sketchbook* ) {} )
{

}

Sketchbook::~Sketchbook()
{
    // try and flush out and kill all remaining tasks, cleaning up buffers in the background
    processPendingUploads(0);

    // detonate lifecycle hook
    m_lifecycle = nullptr;

    // toss all our cpu buffers
    {
        std::scoped_lock<std::mutex> bufferLock( m_bufferPoolMutex );
        for ( base::U32Buffer* buf : m_bufferPool )
        {
            delete buf;
        }
        m_bufferPool.clear();
    }
}


gfx::SketchUploadPtr Sketchbook::scheduleBufferUploadToGPU( SketchBufferPtr&& buffer )
{
    const uint32_t uploadID = m_uploadCounter++;

    const auto dummyBounds = buffer->extents();

    GPUTask* gpuTask = allocateGPUTask( uploadID, std::move( buffer ) );

    OURO_SKETCH_VERBOSE( "gpu-task enqueued [{0:x}]", (uint64_t)gpuTask );
    m_uploadQueue.enqueue( gpuTask );

    return std::make_unique<SketchUpload>( m_lifecycle, uploadID, gpuTask, dummyBounds );
}

void Sketchbook::processPendingUploads( const int32_t taskProcessLimit )
{
    GPUTask* task;

    // move retired tasks from lf queue into main thread storage
    while ( m_retirementQueue.try_dequeue( task ) )
    {
        m_gpuTasksRetired.push_back( task );
    }

    if ( !m_gpuTasksRetired.empty() )
    {
        m_gpuTasksProcessedPersist.clear();
        for ( GPUTask* processed : m_gpuTasksProcessed )
        {
            auto taskIt = std::find( m_gpuTasksRetired.begin(), m_gpuTasksRetired.end(), processed );
            if ( taskIt != m_gpuTasksRetired.end() )
            {
                auto deallocTask = *taskIt;
                m_gpuTasksRetired.erase( taskIt );

                OURO_SKETCH_VERBOSE( "gpu-task destroyed [{:x}]", (uint64_t)task );

                destroyGPUTask( deallocTask );
            }
            else
            {
                m_gpuTasksProcessedPersist.push_back( processed );
            }
        }
        m_gpuTasksProcessed = m_gpuTasksProcessedPersist;
    }

    // run a number of waiting GPU tasks
    int32_t remainingTasksToProcess = ( taskProcessLimit > 0 ) ? taskProcessLimit : std::numeric_limits<int32_t>::max();
    while ( m_uploadQueue.try_dequeue( task ) )
    {
        OURO_SKETCH_VERBOSE( "gpu-task processed [{:x}]", (uint64_t)task );

        processGPUTask( task );
        m_gpuTasksProcessed.push_back( task );

        remainingTasksToProcess--;
        if ( remainingTasksToProcess <= 0 )
            break;
    }
}

// TODO better heuristics than this MVP
base::U32Buffer* Sketchbook::borrow( const DimensionsPow2& dimensions )
{
    std::scoped_lock<std::mutex> bufferLock( m_bufferPoolMutex );

    // find a suitable existing buffer
    auto bufIt = std::find_if( 
        m_bufferPool.begin(), 
        m_bufferPool.end(), [&]( base::U32Buffer*& obj )
        {
            return obj->getWidth()  == dimensions.width() &&
                   obj->getHeight() == dimensions.height();
        });

    if ( bufIt != m_bufferPool.end() )
    {
        auto oldBuf = *bufIt;
        m_bufferPool.erase( bufIt );

        OURO_SKETCH_VERBOSE( "buffer re-used [{:x}]", (uint64_t)oldBuf );
        oldBuf->clear(0);
        return oldBuf;
    }


    base::U32Buffer* newBuffer = new base::U32Buffer( dimensions.width(), dimensions.height() );
    OURO_SKETCH_VERBOSE( "new buffer created [{:x}] [{}, {}]", (uint64_t)newBuffer, dimensions.width(), dimensions.height() );

    return newBuffer;
}

void Sketchbook::recycle( base::U32Buffer* buffer )
{
    std::scoped_lock<std::mutex> bufferLock( m_bufferPoolMutex );

    OURO_SKETCH_VERBOSE( "buffer recycled [{:x}]", (uint64_t)buffer );
    m_bufferPool.push_back( buffer );
}

void Sketchbook::retire( const GPUTask* gpuTask )
{
    m_retirementQueue.emplace( const_cast<GPUTask*>( gpuTask ) );
}

} // namespace gfx
