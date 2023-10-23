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
#include "buffer/buffer.2d.h"

#if 0
#define OURO_SKETCH_VERBOSE(...)         blog::gfx( __VA_ARGS__ )
#else
#define OURO_SKETCH_VERBOSE(...)         
#endif 


namespace gfx {

// ---------------------------------------------------------------------------------------------------------------------
// 
// immutable width x height dimensions structure
struct Dimensions
{
    Dimensions() = default;
    constexpr Dimensions( const uint32_t width, const uint32_t height )
        : m_width( width )
        , m_height( height )
    {}

    constexpr uint32_t width() const { return m_width; }
    constexpr uint32_t height() const { return m_height; }
    constexpr uint64_t comparitorU64() const { return (uint64_t)m_width | (uint64_t)m_height << 32; }

private:

    uint32_t    m_width  = 0;
    uint32_t    m_height = 0;
};

// distinct Dimensions variant that guarantees ^2 dimensions
struct DimensionsPow2
{
    DimensionsPow2() = default;
    constexpr DimensionsPow2( const uint32_t width, const uint32_t height )
        : m_width( base::nextPow2( width ) )
        , m_height( base::nextPow2( height ) )
    {}
    constexpr DimensionsPow2( const Dimensions& dims )
        : DimensionsPow2( dims.width(), dims.height() )
    {}

    constexpr uint32_t width() const { return m_width; }
    constexpr uint32_t height() const { return m_height; }
    constexpr uint64_t comparitorU64() const { return (uint64_t)m_width | (uint64_t)m_height << 32; }

private:

    uint32_t    m_width  = 0;
    uint32_t    m_height = 0;
};


struct Sketchbook;

// ---------------------------------------------------------------------------------------------------------------------
// a SketchBuffer wraps the CPU-side buffer of pixels that we will eventually want to trade for a GPU object. It contains
// an instance of a U32 buffer as well as control over how much of that buffer will eventually be masked for rendering
struct SketchBuffer
{
    DECLARE_NO_COPY( SketchBuffer );

    friend Sketchbook;

    // creating a SketchBuffer requires a weak_ptr back to the origin Sketchbook; during the destructor, the 
    // SketchBuffer will ask the owning Sketchbook to dispose of / recycle the embedded raw buffer pointer; this ensures
    // the buffer is always correctly handled (and returned to a pool) without any calling code having to remember to do so
    SketchBuffer() = delete;
    SketchBuffer( 
        std::weak_ptr< Sketchbook > sketchbook,
        const DimensionsPow2&       dimensions );       // to help any issues with non-pow2 textures, all buffers are required
                                                        // to be constructed with pre-pow2 dimensions; use setExtents() to then
                                                        // select a portion of the buffer to consider valid for rendering
    ~SketchBuffer();


    constexpr void setExtents( const Dimensions& usage )
    {
        ABSL_ASSERT( usage.width()  <= m_dimensions.width()  );
        ABSL_ASSERT( usage.height() <= m_dimensions.height() );
        m_usageExtents = usage;
    }

    constexpr const DimensionsPow2&  dim()     const { return m_dimensions;   }
    constexpr const Dimensions&      extents() const { return m_usageExtents; }

    constexpr             bool hasBuffer() const { return m_buffer != nullptr; }
    constexpr const base::U32Buffer& get() const { return *m_buffer; }
    constexpr       base::U32Buffer& get()       { return *m_buffer; }

private:
    DimensionsPow2              m_dimensions;       // full size of the underlying buffer
    Dimensions                  m_usageExtents;     // w x h of how much is actually in use
    base::U32Buffer*            m_buffer;           // pixel data
    std::weak_ptr< Sketchbook > m_sketchbook;       // validity tether back to origin sketchbook instance
};
using SketchBufferPtr = std::unique_ptr< SketchBuffer >;


// ---------------------------------------------------------------------------------------------------------------------
// abstract interface representing the operation of taking CPU data and turning it into a usable GPU object
// the actual version in use will be defined and used by the graphics-api-specific code
//
// the task takes sole ownership of the CPU buffer and will dispose of it when appropriate
//
struct GPUTask
{
    DECLARE_NO_COPY( GPUTask );

    // a state block that is returned once the GPU object is finalised; it contains the size/extents as 
    // defined by the original CPU buffer as well as a usable ImGui-compliant ID to get things rendered
    struct ValidState
    {
        DimensionsPow2  m_textureDimensions;
        Dimensions      m_usageDimensions;
        ImVec2          m_usageDimensionsVec2;
        ImVec2          m_usageUV;

        ImTextureID     m_imTextureID = 0;
    };

    GPUTask( const uint32_t uploadID, SketchBufferPtr&& buffer )
        : m_uploadID( uploadID )
        , m_buffer( std::move( buffer ) )
    {}
    virtual ~GPUTask() {}


    // the simple interface for client code to [a] check if GPU upload is done yet (based on return value) and [b]
    // get the texture state and ID if it does return true
    virtual bool getStateIfValid( ValidState& result ) const = 0;

protected:
    uint32_t              m_uploadID;
    SketchBufferPtr       m_buffer;
};


// ---------------------------------------------------------------------------------------------------------------------
// encapsulation of a GPUTask raw instance; held by client code as an interface to check if the GPU conversion is done
// (and get the results if it is) as well as a smart handle that will cancel/retire the GPUTask automatically when the
// SketchUpload is destroyed
//
struct SketchUpload
{
    SketchUpload(
        std::weak_ptr< Sketchbook > sketchbook, // like SketchBuffer, we need a link back to the issuing sketchbook so
                                                // that resources can be automatically retired at destructor-time
        const uint32_t      uploadID,
        const GPUTask*      gpuTask,
        const Dimensions&   bounds );           // w x h of original buffer, can be used as stand-in/dummy while gpu task running
    ~SketchUpload();

    constexpr const Dimensions& bounds() const { return m_bounds; }
    constexpr uint32_t uploadID() const { return m_uploadID; }

    // pass-through to GPUTask call of the same name
    bool getStateIfValid( GPUTask::ValidState& result ) const
    {
        return m_gpuTask->getStateIfValid( result );
    }

private:
    const uint32_t              m_uploadID;
    const Dimensions            m_bounds;
    const GPUTask*              m_gpuTask;
    std::weak_ptr< Sketchbook > m_sketchbook;
};
using SketchUploadPtr = std::unique_ptr<SketchUpload>;


// ---------------------------------------------------------------------------------------------------------------------
//
struct Sketchbook
{
    Sketchbook();
    ~Sketchbook();


    // [thread-safe] request a new buffer to draw pixels into
    SketchBufferPtr getBuffer( const DimensionsPow2& dimensions )
    {
        return make_unique< SketchBuffer >( m_lifecycle, dimensions );
    }

    // [thread-safe] exchange a CPU buffer for a handle representing its eventual GPU upload
    SketchUploadPtr scheduleBufferUploadToGPU( SketchBufferPtr&& buffer );



    // call regularly from main thread to move buffer->texture processing onwards
    void processPendingUploads( const int32_t taskProcessLimit = 0 );


private:

    // graphics-API-specific implementations

    // take a CPU-side buffer and create a GPUTask instance to transform it into a GPU object
    // [needs to be thread-safe, will be called from scheduleBufferUploadToGPU]
    GPUTask* allocateGPUTask( const uint32_t uploadID, SketchBufferPtr&& buffer );

    // GPUTask is deemed done with, either toss it or return to some kind of pooling system
    // [only called from processPendingUploads on main thread]
    void destroyGPUTask( GPUTask* task );

    // called from main thread loop to ask API-specific code to do something with the task
    // [only called from processPendingUploads on main thread]
    void processGPUTask( GPUTask* task );


private:
    friend SketchBuffer;
    friend SketchUpload;

    using GPUTaskQueue = mcc::ReaderWriterQueue< GPUTask* >;
    using GPUTaskList  = std::vector< GPUTask* >;



    // sketch buffer pool
    base::U32Buffer* borrow( const DimensionsPow2& dimensions );
    void recycle( base::U32Buffer* buffer );

    // texture pool
    void retire( const GPUTask* gpuTask );


    std::mutex                      m_bufferPoolMutex;          // serial access to push/pull from the buffer pool
    std::vector< base::U32Buffer* > m_bufferPool;

    std::atomic_uint32_t            m_uploadCounter;
    GPUTaskQueue                    m_uploadQueue;
    GPUTaskQueue                    m_retirementQueue;

    GPUTaskList                     m_gpuTasksProcessed;
    GPUTaskList                     m_gpuTasksProcessedPersist;
    GPUTaskList                     m_gpuTasksRetired;

    std::shared_ptr<Sketchbook>     m_lifecycle;                // handle tracking the Sketchbook instance, allowing use of weak_ptrs to validate 
                                                                // distributed objects' lifetimes in relation to the owning Sketchbook
};


} // namespace gfx
