//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#pragma once

#include "plug/known.plugin.h"

namespace plug {
namespace stash {

// ---------------------------------------------------------------------------------------------------------------------
struct CLAP
{
    using Instance = std::unique_ptr<CLAP>;
    using Iterator = std::function< void( const KnownPlugin&, KnownPluginIndex index ) >;

    ~CLAP();

    static Instance createAndPopulateAsync( tf::Executor& taskExecutor );

    // returns true once the population tasks are complete and known plugin data can be fetched
    bool asyncPopulateFinished() const
    {
        return m_asyncPopulationComplete.load();
    }

    // returns true once we've got all the up-to-date file hashes done in the background
    bool asyncFileHashingFinished() const
    {
        return m_pluginFilesHashed == m_pluginFileHashes.size();
    }

    // bundled up request if all the background processing on the stash is finished
    bool asyncAllTasksComplete() const
    {
        return asyncPopulateFinished() && asyncFileHashingFinished() && m_asyncAllProcessingComplete;
    }

    // visit all known plugins; this includes the full discovered set, including ones we might not actually
    // support properly
    void iterateKnownPlugins( Iterator&& callback ) const
    {
        ABSL_ASSERT( asyncPopulateFinished() );
        if ( !asyncPopulateFinished() )
            return;

        const int64_t knownPluginCount = static_cast< int64_t >( m_knownPlugins.size() );
        for ( int64_t sI = 0; sI < knownPluginCount; sI ++ )
        {
            const auto& pluginInstance = m_knownPlugins[sI];
            callback( *pluginInstance, KnownPluginIndex{ sI } );
        }
    }

    // visit all known plugins that have been loaded, validated and sorted into a stack that we believe
    // can successfully work inside our audio engine
    void iterateKnownPluginsValidAndSorted( Iterator&& callback ) const
    {
        ABSL_ASSERT( asyncAllTasksComplete() );
        if ( !asyncAllTasksComplete() )
            return;

        for ( const auto& knownIndex : m_knownPluginsVerifiedSorted )
        {
            const auto& pluginInstance = m_knownPlugins[knownIndex.get()];
            callback( *pluginInstance, knownIndex );
        }
    }


    const KnownPlugin& getKnownPluginAtIndex( KnownPluginIndex index ) const
    {
        return *(m_knownPlugins[index.get()]);
    }

    // given an exterior-index (from KnownPlugin), return the computed hash for the given library file
    uint64_t getFileHashAtIndex( plug::ExteriorIndex index ) const
    {
        return m_pluginFileHashes[index.get()];
    }

protected: 

    CLAP();

    static uint64_t hashfile( const fs::path& filepath );

private:

    static constexpr uint64_t   cHashingVersionSeed = 0xF000;   // seed used when hashing, change to invalidate all stored hashes

    using PluginUIDLookup = absl::flat_hash_map< std::string_view, plug::KnownPluginIndex >;

    void beginPopulateAsync( tf::Executor& taskExecutor );

    // returns absl::Ok if the plugin at the given index is something we can work with - loading it into a dummy
    // clap_host and examining port layout
    absl::Status verifyPluginForEffectUsage( const plug::KnownPluginIndex& index ) const;


    std::atomic_bool                        m_asyncPopulationComplete;
    std::atomic_bool                        m_asyncAllProcessingComplete;

    std::vector< fs::path >                 m_pluginFullPaths;
    std::vector< uint64_t >                 m_pluginFileHashes;
    std::atomic_uint32_t                    m_pluginFilesHashed = 0;

    std::vector< KnownPlugin::Instance >    m_knownPlugins;
    std::vector< bool >                     m_knownPluginsVerified;
    std::vector< plug::KnownPluginIndex >   m_knownPluginsVerifiedSorted;

    PluginUIDLookup                         m_knownPluginLookupByUID;
};

} // namespace stash
} // namespace plug
