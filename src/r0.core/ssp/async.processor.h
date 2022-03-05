//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  a buffer manager built to help sample processors offload more expensive encoding/compression tasks
//  to a background worker thread; samples are added via appendStereoSamples() until the active buffer is expended, 
//  at which point the worker thread is woken up and the buffer pointers exchanged so that the audio thread is not interrupted
//
//  it is intended that an ssp inherits from this processor with a chosen interleaved buffer type and
//  implements processBufferedSamplesFromThread() that will be called (as you can imagine) from the worker thread
//

#pragma once

#include "buffer/buffer.iquant.h"

#include "base/instrumentation.h"


namespace ssp {

template< base::IQBufferType _bufferType >
struct AsyncBufferProcessor
{
    // choose a maximum buffer size and give profile points / diagnostics an identifier
    AsyncBufferProcessor( const uint32_t bufferSampleSize, const char* identifier )
        : m_identifier( identifier )
    {
        m_activePage    = new _bufferType( bufferSampleSize );
        m_reservePage   = new _bufferType( bufferSampleSize );
    }

    virtual ~AsyncBufferProcessor()
    {
        terminateProcessorThread();

        delete m_reservePage;
        delete m_activePage;
    }

    inline void launchProcessorThread()
    {
        assert( m_processorThread == nullptr );

        // launch background thread
        m_processorThreadRun = true;
        m_processorThread    = std::make_unique<std::thread>( &AsyncBufferProcessor::processorThreadWorker, this );

        // compression handler is adjacent to the realtime audio thread, so give it a lil priority bump
        // #HDD TODO other platforms, probably with abseil
#if OURO_PLATFORM_WIN
        ::SetThreadPriority( m_processorThread->native_handle(), THREAD_PRIORITY_ABOVE_NORMAL );
#endif // OURO_PLATFORM_WIN
    }

    inline void terminateProcessorThread()
    {
        // terminate compressor thread
        if ( m_processorThread )
        {
            {
                std::unique_lock<std::mutex> lock( m_processorMutex );
                m_processorThreadRun = false;
            }
            m_processorCVar.notify_one(); // unblock
            m_processorThread->join();
            m_processorThread = nullptr;
        }
    }

    // 
    inline void appendStereoSamples( float* buffer0, float* buffer1, const uint32_t sampleCount )
    {
        if ( m_activePage == nullptr )
            return;

        size_t   readOffset         = 0;
        uint32_t samplesRemaining   = sampleCount;
        uint32_t pageRemaining      = ( m_activePage->m_maximumSamples - m_activePage->m_currentSamples );
        
        // if the remaining number of samples would overrun the buffer : 
        // first take all the samples we can, then flip buffers and trigger the background worker thread
        if ( samplesRemaining > pageRemaining )
        {
            float* currentFpPos = &m_activePage->m_interleavedFloat[m_activePage->m_currentSamples * 2];
            for ( size_t idxIn = 0, idxOut = 0; idxIn < pageRemaining; idxIn++, idxOut +=2 )
            {
                currentFpPos[idxOut + 0] = buffer0[idxIn];
                currentFpPos[idxOut + 1] = buffer1[idxIn];
            }

            m_activePage->m_currentSamples += pageRemaining;

            // offset the initial values for how many samples to read and where to read from for the remaining
            // copy code outside of this block
            samplesRemaining -= pageRemaining;
            readOffset       += pageRemaining;


            // buffer complete - launch processor stage on background thread and swap to other buffer to continue work
            {
                std::unique_lock<std::mutex> lock( m_processorMutex );

                std::swap( m_activePage, m_reservePage );
                m_activePage->m_currentSamples = 0;
            }
            m_processorCVar.notify_one();
        }

        float* currentFpPos = &m_activePage->m_interleavedFloat[m_activePage->m_currentSamples * 2];
        for ( size_t idxIn = 0, idxOut = 0; idxIn < samplesRemaining; idxIn++, idxOut +=2 )
        {
            currentFpPos[idxOut + 0] = buffer0[readOffset + idxIn];
            currentFpPos[idxOut + 1] = buffer1[readOffset + idxIn];
        }

        m_activePage->m_currentSamples += samplesRemaining;
    }


protected:

    _bufferType* getActiveBuffer() { return m_activePage; }

    virtual void processBufferedSamplesFromThread( const _bufferType& buffer ) = 0;


private:

    inline void processorThreadWorker()
    {
        const auto threadName = fmt::format( "{}{}:Processor", OURO_THREAD_PREFIX, m_identifier );
        base::instr::setThreadName( threadName.c_str() );

        blog::core( "[{}] processor thread launched", m_identifier );

        assert( m_reservePage != nullptr );
        for ( ;; )
        {
            std::unique_lock<std::mutex> lock( m_processorMutex );
            m_processorCVar.wait( lock );

            if ( !m_processorThreadRun )
                break;

            {
                base::instr::ScopedEvent se( m_identifier.c_str(), "process-samples", base::instr::PresetColour::Orange );

                m_reservePage->quantise();
                processBufferedSamplesFromThread( *m_reservePage );
            }

            lock.unlock();
        }

        blog::core( "[{}] processor thread closing", m_identifier );
    }


    std::unique_ptr< std::thread >  m_processorThread;
    std::atomic_bool                m_processorThreadRun    = false;

    // inter-thread communication to signal compression jobs ready
    std::mutex                      m_processorMutex;
    std::condition_variable         m_processorCVar;

    std::string                     m_identifier;

    _bufferType*                    m_activePage            = nullptr;
    _bufferType*                    m_reservePage           = nullptr;
};

using AsyncBufferProcessorIQ16 = AsyncBufferProcessor< base::IQ16Buffer >;
using AsyncBufferProcessorIQ24 = AsyncBufferProcessor< base::IQ24Buffer >;

} // namespace ssp
